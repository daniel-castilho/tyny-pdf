# ADR-0003: Error model

- Status: Accepted (ratified by the project owner, 2026-09-16)
- Date: 2026-09-16
- Deciders: project owner
- Related: ADR-0001, ADR-0002; `include/pdfcore/pdfcore.h`

## Context

Three execution environments must share one error story:

1. The engine. MuPDF reports through its own mechanism: `fz_try`, `fz_always`, `fz_catch` and
   `fz_rethrow`, which are built on `setjmp` and a per-context exception stack. A C++ exception
   crossing that boundary is a real class of defect, not a style preference.
2. `pdfcore`, a C library whose public surface must be usable from any language and must not
   depend on a C++ ABI.
3. The viewer application, where failing a page render is a user-visible event with recovery,
   and where exceptions and RAII are the natural tools.

The error model must also carry product requirements: a corrupt document must fail cheaply and
locally (per page), never abort the process; the CLI must map failures to documented exit codes;
the accessibility and conformance work must be able to name a failing check precisely.

## Decision

**One model per layer, with two explicit bridges, and a stable numeric contract at the boundary.**

### 1. Public C surface: `pc_status`

```c
typedef enum pc_error {
  PC_OK = 0,
  PC_ERR_UNSUPPORTED = 1,        /* capability not implemented by this backend */
  PC_ERR_CORRUPT = 2,            /* document cannot be parsed or repaired */
  PC_ERR_ENCRYPTED = 3,          /* password required or insufficient */
  PC_ERR_LIMIT = 4,              /* a configured budget or hard limit was hit */
  PC_ERR_CANCELLED = 5,
  PC_ERR_IO = 6,
  PC_ERR_STATE = 7,              /* API used out of order; a programming error */
  PC_ERR_BACKEND = 8,            /* engine reported a failure, see detail */
  PC_ERR_CAPABILITY = 9          /* operation not available, see caps */
  /* append-only: never renumber, never remove */
} pc_error;

typedef struct pc_status {
  uint32_t size;        /* struct size, for forward compatibility */
  pc_error code;
  uint32_t detail_id;   /* engine-specific code, when meaningful, else 0 */
  const char *detail;   /* static string table, or NULL; never freed by caller */
} pc_status;

#define PC_STATUS_OK ((pc_status){sizeof(pc_status), PC_OK, 0, NULL})
```

Rules: error codes are append-only (an ABI promise, tested by a committed golden header);
`detail` points to static storage, so returning a status never allocates; `PC_ERR_STATE` is a
bug and must be fatal in debug builds; every `pc_status`-returning function must be documented
with the codes it can produce, and `tools/spec-check.py` requires a requirement id for each
non-`PC_OK` code path that is reachable from a public function.

### 2. `pdfcore` internals: `std::expected<T, pc_error>`

Internally, failure is a value, not a control-flow event. Exceptions are not used inside
`src/core` or `src/backends`: those targets are compiled with `-fno-exceptions -fno-rtti`, so a
`throw` is a compile error rather than a code review comment.

### 3. Engine bridge, in one file

All calls into the engine go through `src/backends/mupdf/exception_bridge.h`, which converts the
engine's exception stack into a `pc_status` and never lets anything else escape:

```cpp
#define PC_FZ_TRY(out)  fz_try(ctx)
#define PC_FZ_CATCH(out) fz_catch(ctx) { (out) = pc_from_fz(ctx); }
```

with `pc_from_fz` reading `fz_code()` and `fz_message()` and mapping them onto `pc_error`.
Anything that needs a resource release on both paths uses `fz_always`. The bridge is the only
place in the repository allowed to mention the engine's control-flow primitives, and that is a
grep-enforced rule.

### 4. Viewer application: exceptions are allowed

`src/app` and `src/os/win32` may use exceptions and RAII freely, because the C++/Win32 surface
(`HWND`, COM-ish APIs, allocation failures) is where stack unwinding pays. One rule: no
exception may cross into or out of `pdfcore`, and any status returned by the core that the app
cannot recover from is escalated into a user-facing failure dialog, not into a crash.

### 5. Contracts are debug instrumentation only

C++26 contract style preconditions may be used in `src/core` as documentation-grade assertions
in debug builds. They are not a security boundary and must not be relied on for input
validation; validation belongs in the parser limits of the input hardening document.

### 6. Failure presentation is part of the error model

- A page that fails to render shows a per-page error tile and keeps the document usable.
- A document that fails to open offers repair, then explains what failed and which requirement
  the behaviour satisfies.
- The CLI maps: `0` success, `1` assertion failure, `2` usage or input error, `3` document or
  backend error. Tests exist for all four.
- Every user-visible error message is a key in the string catalog; messages are never
  concatenated from fragments.

## Enforcement

| Check | Command | Where |
| --- | --- | --- |
| No exceptions in core or backends | `-fno-exceptions` in both CMake targets (build failure is the check) | CI |
| No raw engine control flow outside the bridge | `grep -rn "fz_try" src` restricted to `exception_bridge.h` | CI |
| Error code stability | regeneration of `pc_error` golden header must leave `git status` clean | CI |
| Every public function documents its codes | `tools/spec-check.py` section for error documentation | CI |
| Exit code mapping | `tests/unit/test_cli_exit_codes.cc` covers all four | CI |

## Consequences

- Positive: the classic renderer defect (an exception unwinding through engine state and
  leaving the context inconsistent) becomes unrepresentable, not merely discouraged.
- Positive: the C ABI stays stable for non-C++ consumers and for a future language swap.
- Negative: two error idioms in one repository. Mitigated by the composition root being the only
  place where `pc_status` and `std::expected` meet application exceptions.
- Negative: `std::expected` on hot paths adds a small per-call cost; acceptable because the
  raster loop does not return statuses per span. If a profile disproves that, the loop switches
  to a monotonic "last error" design recorded in a superseding ADR.
- Negative: `-fno-exceptions` blocks third-party headers that throw; those headers are confined
  to the app target.

## Alternatives rejected

- Exceptions in the core with one catch-all boundary: unrepresentable interaction with the
  engine's `setjmp` state, and it makes the C ABI unreliable.
- Coded errors plus a global `get_last_error`: friendly to C, hostile to threads and to
  testability, and it loses type-level exhaustiveness in `src/core`.
