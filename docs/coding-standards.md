# Coding Standards — C++20 / C ABI / Win32 (Tyny PDF)

Practical reference for solo and AI-assisted development of **Tyny PDF**. Goal: **consistency over
time, not ceremony**. Living document - edit as the project evolves, and keep every rule attached to
the command that enforces it.

**Official Domain:** [https://tyny.ca](https://tyny.ca) | **App ID:** `ca.tyny.pdf`

> **Status of this file.** The repository is a seed: there is no `src/`, no `CMakeLists.txt` and no
> `.clang-format` yet. That changes how a rule here is written, not whether it binds: each section
> names what enforces it *today* (a committed script) and what will enforce it *from M0* (a compiler
> flag, a CI job). A rule whose enforcement is marked `planned` is a commitment in
> `docs/kickoff.md`, not an achieved property - do not cite it as one in a PR body.

**Relationship to other docs:**

| Doc                                      | Wins when                                                       |
| :--------------------------------------- | :-------------------------------------------------------------- |
| [`../AGENTS.md`](../AGENTS.md)           | Project conventions, release flow, hard agent rules             |
| **This file**                            | Day-to-day coding detail that does not fit in `AGENTS.md`       |
| [`../adr/`](../adr/)                     | Any decision on structure, format, dependency or toolchain      |
| `src/features/<capability>/SPEC.md`      | What a feature must do, and how it is verified                  |
| [`lessons.md`](lessons.md)               | Durable rules learned the hard way                              |

Where this file conflicts with `AGENTS.md`, **`AGENTS.md` wins**. Where it conflicts with an ADR,
**the ADR wins** - and the ADR is the place to fix the disagreement.

---

## 1. Naming Conventions

| Element                        | Convention                    | Example                                                        |
| :----------------------------- | :---------------------------- | :------------------------------------------------------------- |
| **C symbols (public ABI)**     | `pc_<noun>_<verb>`            | `pc_doc_open`, `pc_doc_page_text`, `pc_txn_undo`               |
| **C macros and enums**         | `PC_*`, errors `PC_ERR_*`     | `PC_STATUS_OK`, `PC_CAP_TABLES`, `PC_ERR_CORRUPT`              |
| **Public typedefs**            | `pc_*` snake case             | `pc_status`, `pc_error`, `pc_doc`, `pc_backend_caps`           |
| **Public headers**             | one per seam, in `include/pdfcore/` | `pdfcore.h`, `backend.h`, `sealer.h`, `print.h`, `sidecar.h` |
| **Capability flags**           | `PC_CAP_*`, declared not inferred | `PC_CAP_TABLES` (R-M5)                                   |
| **C++ types**                  | `PascalCase`                  | `AnnotationRecord`, `SidecarWriter`, `TileCache`               |
| **C++ functions and variables** | `snake_case`                 | `flatten_form_fields()`, `page_box`                            |
| **Class members**              | `snake_case_` (trailing underscore) | `doc_`, `budget_`                                        |
| **Files**                      | `snake_case.cc` / `snake_case.h` | `sidecar_writer.cc`, `tile_cache.h`                        |
| **Test files**                 | `test_<subject>.cc`           | `tests/unit/test_sidecar_writer.cc`                            |
| **Feature directories**        | one capability per directory  | `src/features/sidecar/`, `src/features/redaction/`             |
| **Requirement ids**            | `R<n>.<m>`, cited in code     | `// R4.1: tombstone survives a three-way merge`                |
| **Environment variables**      | `TYNYPDF_*`                   | `TYNYPDF_BACKEND` (`docs/naming.md`, `env.prefix`)             |
| **Log subsystems**             | `tynypdf.<area>`              | `tynypdf.core`, `tynypdf.backend.mupdf`, `tynypdf.ui`, `tynypdf.cli` |
| **Ports and adapters**         | port = the seam, adapter = the directory | `include/pdfcore/backend.h` versus `src/backends/mupdf/` |
| **Branches and commits**       | `<type>/<subject>`, Conventional Commits | `feat/sidecar-tombstone`, `fix(text): break lines at grapheme boundaries` |

Name for **what it is or does**, never for the engine that happens to provide it:
`pc_doc_find_tables`, not `mupdf_find_tables`; `AnnotationRecord`, not `FzAnnotView`. The whole
point of the seam is that the engine is swappable, and a name that carries the engine's name is
the first crack in it (R-M2). Use domain language at the boundary (`document`, `page`,
`annotation`, `redaction`, `sidecar`) and library language inside an adapter.

> **Generated identifiers rule.** Never hand-write or hand-edit a derived identifier - product name,
> app id, artefact stems, sidecar suffix, log targets. They come from [`naming.md`](naming.md); edit
> that table and run `python3 tools/naming-sync.py write`. The public API stays brand-free on
> purpose (`pdfcore`, `pc_`), so that renaming the product never touches the ABI.

---

## 2. Directory Boundaries (Clean Architecture, enforced by linking)

The dependency arrow points inward, and the rule is enforced by what a target may **link**, not by
which folder a file sits in (ADR-0001 item 3).

```
tyny-pdf/
|-- include/pdfcore/            # PUBLIC ABI. C only. No C++ types, no engine types, no windows.h
|-- src/
|   |-- core/                   # entities + use cases. No OS header, no engine header. -fno-exceptions
|   |-- backends/<name>/        # the only tree allowed to know an engine exists
|   |-- render/                 # swapchain, tile cache, cachemap. Presentation, never parsing
|   |-- os/win32/               # the only OS-specific tree: window, dpi, uia, clipboard, policy
|   |-- sealer/                 # pkcs7_local/, stub/
|   |-- print/                  # win32_gdi/, preview/
|   |-- features/<capability>/  # vertical slice: wiring, SPEC.md, its own tests
|   |-- app/                    # composition root: wiring only, no logic
|   `-- cli/                    # headless twin, JSON/JUnit reporters, exit codes
|-- tests/
|   |-- unit/                   # one file per requirement cluster, test_<subject>.cc
|   |-- contract/               # same suite against every backend
|   |-- conformance/<clause>/   # clause-keyed corpus, veraPDF as the oracle
|   `-- approvals/              # golden structured dumps (committed; received files are not)
|-- docs/                       # this file, kickoff, naming, dev-environment, git-workflow, lessons
|-- adr/                        # one file per decision, each with a "Cost of swapping" section
`-- tools/                      # the gates: check.sh plus the six checks
```

### Boundary Rules

| Directory            | Allowed                                                          | Prohibited (a violation is a build or gate failure) |
| :------------------- | :--------------------------------------------------------------- | :--------------------------------------------------- |
| **`src/core`**       | its own headers, `include/pdfcore/*`, C++20 stdlib               | any `<windows.h>`/`<d2d1.h>`/DWrite header, any engine header, `-fexceptions`, `-frtti` |
| **`src/backends`**   | the engine, one bridge file, `include/pdfcore/backend.h`         | UI headers, document semantics that belong to core (R-M8) |
| **`src/render`**     | core value types, DXGI/DComp/D2D                                | parsing, hit-testing, annotation mutation (R-M10)    |
| **`src/os/win32`**   | Win32, UIA, spooler, DPI                                          | document logic, engine handles                       |
| **`src/app`, `src/cli`** | public headers only                                          | an engine symbol, a `fz_*` type, a private core header |

`tools/layering-check.sh` (planned, PR #4) is what turns the "Prohibited" column into a number
(`backend_line_ratio <= 0.15`) instead of an argument.

---

## 3. Practical "Do vs Don't" Code Examples

The shapes below are normative; the exact signatures are declared by PR #5, the first PR that
builds code, so treat a name here as the *role* it plays unless it is quoted from an ADR.

### 3.1 Nothing of the engine crosses the seam

**Don't - the handle passes through, and the caller inherits the engine:**

```c
/* src/core/doc.h */
#include <mupdf/fitz.h>
pc_status pc_doc_page(pc_doc *doc, uint32_t index, fz_page **out);
```

**Do - copy out into our own value type; the document may be closed afterwards:**

```c
/* include/pdfcore/pdfcore.h */
typedef struct pc_text_run {
  uint32_t size;                 /* struct size, first field, as in pc_status */
  float rect[4];                 /* document units, top-left origin */
  uint32_t font_face;
  const char *utf8;              /* owned by the array, not by the engine */
} pc_text_run;

pc_status pc_doc_page_text(pc_doc *doc, uint32_t page,
                           pc_text_run **out_runs, uint32_t *out_count);
void      pc_text_run_free(pc_text_run *runs, uint32_t count);
```

_Why: R-M4. A `pc_doc` can be closed while the caller still holds the runs; a viewer that keeps an
engine handle alive is a use-after-free waiting for a repaint. Checked by the include grep in
[`../AGENTS.md`](../AGENTS.md) rule 1._

### 3.2 Ask for a capability, do not infer it from an empty result

**Don't - "unsupported" and "nothing found" collapse into the same answer:**

```c
pc_table *tables; uint32_t n = 0;
if (pc_doc_find_tables(doc, &tables, &n) == PC_STATUS_OK && n == 0) {
  show_message("This document has no tables");   /* the backend may not implement them at all */
}
```

**Do - query, then branch, and let the UI say what is actually true:**

```c
if (!pc_doc_has_capability(doc, PC_CAP_TABLES)) {
  return (pc_status){sizeof(pc_status), PC_ERR_CAPABILITY, 0,
                     "tables require PC_CAP_TABLES"};
}
```

_Why: R-M5 - a silent empty result is indistinguishable from an empty document. The exact query
function is declared at M0; the behaviour above is already decided in ADR-0011._

### 3.3 Failure is a value, not a control-flow event

**Don't - throw across a boundary, then swallow it into "no annotations":**

```c++
std::vector<Annot> read_sidecar(std::string_view text) {
  try { return parse(text); } catch (...) { return {}; }   /* corrupt file == empty file */
}
```

**Do - a typed failure, mapped once at the C bridge:**

```c++
// src/core/sidecar/reader.h
std::expected<std::vector<Annot>, pc_error> read_sidecar(std::string_view text) noexcept;

// include/pdfcore bridge
pc_status pc_sidecar_read(const pc_doc *doc, pc_sidecar **out) {
  auto got = read_sidecar(read_file_to_string(doc));
  if (!got) return make_status(got.error());      /* PC_ERR_CORRUPT, PC_ERR_IO, ... */
  *out = adopt(std::move(*got));
  return PC_STATUS_OK;
}
```

_Why: ADR-0003 sections 1 and 2. `src/core` and `src/backends` compile with `-fno-exceptions
-fno-rtti`, so a `throw` there is a compile error, not a review comment; `PC_ERR_STATE` stays
fatal in debug because it means the caller broke the contract, not the document._

### 3.4 One allocation owner, named in the header

**Don't - free what another module allocated, or "helpfully" take ownership silently:**

```c
pc_text_run_free(runs, count);   /* wrong owner */
free(runs);                      /* worse: the backend allocated it (R-M6) */
```

**Do - the type that allocates provides the release function, and the allocating declaration names
it in its comment:**

```c
/* pc_doc_page_text: on success, *out_runs is owned by the caller and MUST be released with
 * pc_text_run_free(*out_runs, *out_count). Returning without freeing is a leak, and freeing with
 * anything else is undefined behaviour. */
```

### 3.5 Semantics live on the IR, so undo is not a UI feature

**Don't - the toolbar reaches for the engine on Undo:**

```c++
void Viewer::on_undo() { engine_revert_annotation(page_, selected_); repaint(); }
```

**Do - one command log, published on the ABI so the CLI and third parties share it:**

```c++
void Viewer::on_undo() {
  pc_status st = pc_txn_undo(txn_, &affected_);        /* D-6: the same history the UI sees */
  if (st.code == PC_ERR_LIMIT) show_undo_window_shrunk();
  repaint(affected_);
}
```

_Why: R-M8. `undo`, `redo`, redaction, flatten and the sidecar serialize must all be expressible
without a window; the byte-identical replay test (`tynypdf-cli txn replay`) is the check._

### 3.6 Presentation stays presentation

**Don't - hit-testing and geometry reconciliation inside the window procedure:**

```c++
case WM_LBUTTONDOWN: hit = annot_hit_test(engine_page, GET_X_LPARAM(l), GET_Y_LPARAM(p)); break;
```

**Do - convert, then ask core in document units; the handler only routes:**

```c++
case WM_LBUTTONDOWN:
  hit = core_hit_test(view_.document(), view_.to_doc_point(GET_X_LPARAM(l), GET_Y_LPARAM(p)));
  break;
```

_Why: R-M10. A click at 250 % DPI that only works on Windows is a layering bug, not a DPI bug: the
same coordinates replay on Linux through the CLI._

### 3.7 Sidecar bytes are normative, so formatting is not a preference

**Don't - reformat a fixture, or hand-write the JSON in a script:**

```python
json.dump(doc, fh, indent=2, sort_keys=True)   # close, and still not canonical
```

**Do - use the committed canonicaliser, which is the same code the tests use:**

```bash
python3 tools/sidecar-fmt.py fix document.pdf.tynypdf.json
python3 tools/sidecar-fmt.py check tests/fixtures/sidecar
```

The format is fixed by ADR-0007: UTF-8 without BOM, LF, two-space indent, keys in ascending
code-point order, geometry at at most 3 decimals, timestamps at second precision, unknown keys
preserved, deletion written as a tombstone. Two bytes of meaning must stay two lines of diff.

### 3.8 Logging carries a subsystem, never a path or a secret

**Don't:**

```c++
LOG("opened {} (key {})", doc.path().string(), pfx_key_blob);
```

**Do:**

```c++
log::write(log::Subsystem::Core, "document opened ({} pages, sha256:{})", n, digest);
```

The subsystem names are not invented here: they come from `docs/naming.md` (`tynypdf.core`,
`tynypdf.backend.mupdf`, `tynypdf.ui`, `tynypdf.cli`). A document is identified by its fingerprint,
never by an absolute path - the same value that makes a sidecar stale is the value that names a
person's file in a log.

---

## 4. Deep-Dive C++ Directives (`src/`)

1. **The flags are the enforcement.** `src/core` and `src/backends/**` compile with
   `-fno-exceptions -fno-rtti` (ADR-0003 section 2); no `dynamic_cast` on hot paths; failure
   travels as `std::expected`. A rule that only a reviewer can see is not a rule.
2. **Includes.** System and third-party headers in `<...>`, project headers in `"..."`, never a
   path that climbs (`"../core/doc.h"` is a boundary smell). `src/core` includes exactly the
   public headers it implements. No `#ifdef _WIN32` inside `src/core`: if a file needs one, it is
   in the wrong directory.
3. **Hot loops are data-oriented** (ADR-0001 item 4): contiguous storage, no per-frame allocation,
   no virtual dispatch inside the paint loop, and an explicit memory budget owned by
   `src/core/budget`. The floor is the one recorded in `docs/kickoff.md` - a 4000x3000 blit at
   60 fps with no frame over 33 ms p99, RSS at or below 250 MB with 1000 pages open at three tiles
   each and no growth when scrolling back - and no measurement of it exists yet: M0.4 produces the
   first numbers, reproducible within 10% on a second run.
4. **Concurrency is one model, declared at the seam** (R-M7): engine contexts are not shared across
   threads, and a thread that touches the engine owns its context. The measured reason is in
   [`adr/0011`](../adr/0011-modularity-rules.md): a binding that gave every context one shared mutex
   array ran ten threads at 13.3x single-thread time where ten processes ran at 3.3x.
5. **Handles borrow, owners own.** A borrowing type names its owner in the comment and carries no
   destructor; only the owner frees (R-M6). A member that can outlive its document is a bug you have
   not found yet.
6. **Comments carry the requirement, not the syntax.** Every deliberate deviation, budget or
   ordering rule cites its id (`// R2.1`); no commented-out code, no aspirational API, no header
   that exists only as a declaration in one file.
7. **House style, cheap to reverse** (R-M12 - recorded here rather than in an ADR for that
   reason): `PascalCase` types, `snake_case` everything else, trailing underscore on members,
   `auto` only where the right-hand side names the type, `noexcept` on anything the C bridge
   calls, `enum class` for flags with an explicit underlying type, and no `using namespace`
   outside a function body.

---

## 5. Deep-Dive C ABI Directives (`include/pdfcore/`)

1. **Fixed-width, C-only, no C++ type in a public header.** Counts are `uint32_t`, byte sizes
   `size_t`, geometry `float` in document units with the origin stated once per struct.
2. **Every struct that crosses the boundary begins with `uint32_t size`** - the pattern `pc_status`
   and the vtable use (R-M3) - so a newer writer can hand an older reader a struct it can validate.
3. **Error codes are append-only: never renumber, never remove** (ADR-0003 section 1). A committed
   golden header test fails a renumbering, because it would silently re-map every caller.
4. **A function documents the codes it can produce,** and `tools/spec-check.py` requires a
   requirement id for each reachable non-`PC_OK` path (ADR-0003 section 1). No undocumented
   failure.
5. **Ownership is part of the signature:** the allocating call names its release function in the
   header comment, and a `const char *` documented as static storage is never freed by the caller
   (`pc_status.detail` is exactly that).
6. **A vtable entry may only be appended;** a breaking change is an `abi_major` bump with a
   migration note, and a backend built against a newer major is refused with a message naming both
   versions (R-M3). The public surface stays brand-free (`pdfcore`, `pc_`) per `docs/naming.md`.

---

## 6. Secrets, Privacy & Network Discipline

- **No route unless the operation asked.** Networking is default-deny per operation, with an
  in-UI disclosure and an individual switch (ADR-0003); nothing may open a socket for a
  preference, an update check or a font lookup. From M0 the proof is a run with no route but
  localhost, and a `strace`/ETW capture during a full open-edit-save cycle where any outbound
  socket is a build failure (`docs/kickoff.md` section 8).
- **Key material never enters the tree.** `.gitignore` excludes `*.pfx`, `*.p12`, `*.key` and
  `tests/conformance/private/` - that exclusion is committed today, and a fixture that needs a key
  uses a generated test key recorded in the test, never a user's.
- **Logs and crash reports are local and minimal.** No absolute path, no document text, no key
  bytes; a crash writes a local report and prints its path, and there is no uploader in v1.
- **Private corpora stay private.** A real-world PDF that a test needs goes to
  `tests/conformance/private/`; if a check cannot be written without it, the check is written
  against a synthetic file instead.
- **A signature produced locally is labelled locally** (`PAdES-B-B (local)`); revalidation,
  revocation and TSA are explicit user actions, so "no network" stays true by default rather than
  by omission.

---

## 7. Testing Strategy & Coverage Expectations

| Kind                 | Tooling                            | Scope                                                       | Oracle                                   |
| :------------------  | :--------------------------------  | :---------------------------------------------------------  | :--------------------------------------  |
| :---------------     | :--------------------------------  | :------------------------------------------------------     | :--------------------------------------  |
| :------------------- | :--------------------------------- | :---------------------------------------------------------- | :--------------------------------------- |
| **Unit**             | GoogleTest, `tests/unit/test_*.cc` | core semantics on the `null` backend                        | the requirement id in the test's name    |
| **Contract**         | same suite, every backend          | the vtable is the whole interface, not a convenience        | identical expectations per backend       |
| **Conformance**      | `tests/conformance/<clause>/`      | validity and tagged-PDF claims                              | veraPDF, not our own reading of the spec |
| **Approval**         | `tests/approvals/`                 | IR dumps, rendered digests, CLI JSON                        | committed golden files                   |
| **Property**         | sidecar and txn round-trips        | canonicalisation idempotence, key order, tombstone merge    | algebraic invariants                     |
| **Fuzz**             | libFuzzer, nightly                 | parsing, sidecar loading, sealer, engine bridge             | a crash is a bug with a committed repro  |
| **Performance**      | trend job per release              | blit frame time p99, RSS at 1000 pages, cold start          | `tests/baseline.json` next to Sumatra    |
| **Accessibility**    | `docs/a11y/` scripts, UIA asserts  | canvas, annotation list, form fields                        | expected narration text                  |
| **Privacy**          | no-route run, `strace`/ETW         | a full open-edit-save cycle                                 | zero outbound sockets                    |

- **Mocks:** only seams. The `null` backend is a real implementation, not a mock for show (it is
  the proof that no engine type leaks); never mock our own IR, and never assert on a private call
  sequence where a behaviour assertion is available.
- **A test that is not traced is not evidence** (`docs/kickoff.md` section 8): a test cites its
  requirement, and a requirement's `Verification:` line uses one of the four prefixes
  `tools/spec-check.py` accepts - `unit:<path>`, `golden:<path>`, `script:<path>`, `manual:<note>`.
- **Coverage floor: not stated, on purpose.** A percentage for a tree with no `src/` measures
  nothing, and `docs/kickoff.md` forbids a coverage badge before the job that produces the number
  exists. The floor is set in M0 when the measurement lands in CI, in the same PR as the first
  line of code it covers, and is never retrofitted to flatter a release. What is enforced today is
  traceability: `spec-check` reports 14 requirements, 0 orphans, 10 still pending their artefact.

### Verification commands

```bash
python3 tools/spec-check.py                       # today: shape, orphans, artefacts
python3 tools/sidecar-fmt.py check tests/fixtures/sidecar
ctest --preset linux-core --output-on-failure      # from M0, with PR #5
```

---

## 8. Formatting, Tooling & Pre-Commit Quality Gate

**Committed today (`.editorconfig`, read by every editor and by the agents):**

- `indent_size = 2` for `*.{c,cc,cpp,h,hpp}` and for `*.md`, `*.yml`, `*.json`, `*.cmake`; 4 for
  Python and elsewhere; tabs only in `Makefile`.
- `end_of_line = lf`, `insert_final_newline = true`, `trim_trailing_whitespace = true`,
  `max_line_length = 100` - the same limit `tools/docs-check.py` enforces on markdown.
- `tests/golden/**` keeps whitespace and skips the final newline: those files are compared byte for
  byte, so "tidy up" is a test failure there.
- `.bat`, `.cmd` and `.rc` are CRLF on purpose; `.gitattributes` owns the rest (ADR-0007 makes bytes
  normative, so a re-save is a semantic change).

**Not committed yet, and therefore not a standard - a gap:** there is no `.clang-format`, no
`.clang-tidy` and no `CMakeLists.txt` in the tree: the build system and its style configuration
are written by PR #5, the first PR that compiles anything. Until then `.githooks/pre-commit` runs
`clang-format --dry-run -Werror` on staged C/C++ files when the binary is installed, so the style
it enforces is that tool's default rather than a decision of this project - raise it in review
instead of assuming the tree has agreed. The two toolchains also disagree about warnings: the
MSVC parity job and the MinGW cross job are both required, the latter with `-Werror`, and a
MinGW-only failure is fixed by dropping the vendor extension, never by adding an `#ifdef`
(ADR-0010).

**Every commit satisfies the local gate** - the hook runs most of it; `tools/check.sh` is the
whole of it:

```bash
sh tools/check.sh                                   # seven sections, all green
python3 tools/lang-check.py --self-test             # 5/5
python3 tools/naming-sync.py self-test              # 5/5
python3 tools/sidecar-fmt.py self-test              # 5/5
python3 tools/spec-check.py --self-test             # 14/14
python3 tools/docs-check.py --self-test             # 11/11
# from M0, in the same order CI runs them:
cmake --preset linux-core && cmake --build --preset linux-core
ctest --preset linux-core --output-on-failure
clang-format --dry-run -Werror $(git diff --name-only -- '*.cc' '*.h')
clang-tidy $(git diff --name-only -- '*.cc') -- -std=c++20
```

The self-tests are not decoration: a check that cannot detect its own violation is how `tyny-pulse`
ended up with a README describing a pipeline CI never ran (see [`lessons.md`](lessons.md)), and that
history is why this repository adds the checker's test before it adds a rule.

---

## Quick Pre-Commit Checklist

- [ ] `sh tools/check.sh` prints `check: all gates green`, run just now, output pasted in the PR.
- [ ] No engine or Windows header outside `src/backends/**` and `src/os/win32` (rule 1 grep in
      [`../AGENTS.md`](../AGENTS.md)); no `throw` or `catch` in `src/core`.
- [ ] No raw path, key or document text in a log line, a fixture or a crash report.
- [ ] Every new `pc_*` function documents its error codes and names its release function.
- [ ] Every requirement touched by the diff has a test that cites its `R<n>.<m>` id, or the
      `Verification:` line says which artefact will and in which PR.
- [ ] No sidecar fixture was edited by hand: `python3 tools/sidecar-fmt.py check` is clean.
- [ ] Docs moved with the code: capability `SPEC.md`, `README.md`, `CHANGELOG.md`, the debt matrix
  in `AGENTS.md`, and this file if a convention changed.
- [ ] No new dependency, build flag or toolchain without an ADR that prices it (R-M9).

---

> _"When in doubt, prefer the simpler solution that still respects the boundary. Cleverness is debt,
> and an unenforceable rule is decoration."_

