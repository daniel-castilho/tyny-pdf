# Epic 3: The Heart -- src/core IR, Text, Budget and Sidecar Semantics

**Project:** tyny-pdf
**Context:** C++20 core behind a C ABI (`pdfcore`), Win32 + Direct2D presentation,
LLVM-MinGW cross
build from Linux, `tynypdf-cli` as the CI transport (ADR-0001, ADR-0002, ADR-0011),
MuPDF 1.26.8
vendored behind `include/pdfcore/backend.h`.
**Goal:** build the document heart that every later delta depends on. After this
epic, annotation,
undo, text, forms, redaction and the render budget operate on our own intermediate
representation,
never on engine handles -- so a bug reproduces in `tynypdf-cli` on Linux (R-M8) and
the engine
remains swappable within the 0.15 budget (R-M11).

---

## Repo state (measured 2026-09-20, execution time, not planning)

Measured on `main @ 73dec5f` -- the tree this epic starts from. Every number below
was pasted from
a command whose output is in `epic-3-dod.md` §3.0.

- **14/14 gates green.** `sh tools/check.sh` -- language, naming, sidecar-fmt,
  canonical, living
specs, style, diff-scan, layering, SBOM, deps-refresh, patch-report, docs-check plus
self-tests.
`sh tools/gates-selftest.sh` -- 13/13 suites hold. `python3 tools/spec-check.py` -- 7
specs, 28
requirements, 0 orphans, **17 pending** artefacts.
- **Build matrix green.** `core-linux`, `windows-mingw-cross`, `windows-msvc` -- four
  jobs required
on `main` (PR #31). `cmake --preset linux-core && ctest --preset linux-core` green
with ASan/UBSan
and `-Werror`.
- **MuPDF pinned but patches are placeholders.** `third_party/mupdf` at `v1.26.8`,
`third_party/UPSTREAM.toml` carries 40-hex, `third_party/patches/` holds 4 files of
135 bytes each
with `Upstream-status: none` and generic `Reason`. `sh tools/patch-report.sh
--fail-on-stale` is
180 days from failing -- this epic hardens the backend.
- **Baseline measured but not trustworthy for C3.** `tests/baseline.json` exists
  (schema_version 1,
19/09, 5 runs per target) but records `tynypdf (null backend)` in Debug/ASan at 14.19
ms vs Sumatra
3.6.1 at 1155 ms / 3.7pre at 2239 ms -- `null` pattern, not MuPDF pixels, and Debug +
ASan, not
Release on the Windows reference machine. Epic 3 records a **re-measured** baseline
with
MuPDF/Release as its closing evidence.
- **Core empty, app stub, render/os spec-only.** `src/core` is an INTERFACE library
  with a dummy
`pdfcore_dummy.c`. `src/app/main.cc` is 10 lines (`"not yet implemented"`).
`src/render/SPEC.md`
(R15.1-15.4) and `src/os/win32/SPEC.md` (R14.1-14.3) are `manual: implementation in
progress`. `sh
tools/layering-check.sh` reports `backend_line_ratio=0.1014` (13 files) -- green only
because the
denominator is almost empty. After this epic the ratio must fall to **<=0.07** on a
tree with ~2500
new core lines.
- **Sidecar spec complete, implementation 10 pending.**
  `src/features/sidecar/SPEC.md` defines
R1-R6. `spec-check` pending list: `R2.2, R2.3, R3.1, R3.2, R4.1, R4.2, R5.1, R5.2,
R6.1, R6.2` --
all addressed by this epic.

## Why this epic now

- **It is the critical path.** The debt table in `README.md` item 1 and the roadmap's
  phase graph
both name `src/core` empty as the P0 that blocks D-1 (sidecar), D-2 (forms), D-3
(redaction), D-4
(text), D-5 (a11y) and D-6 (undo). No vertical delta can be evidenced while semantics
live in the
backend or the window proc (R-M8, R-M10).
- **It pays the swap budget.** `backend_line_ratio` is 0.1014 today because
  `src/core` has no
lines. Any helper added to `src/backends/` now risks tripping R-M11 for the wrong
reason. Once
`src/core/doc`, `text`, `budget`, `sidecar` exist, the denominator grows and the 0.15
ceiling
becomes a real constraint on coupling rather than an accident of an empty tree.
- **It retires ADR-0011 R-M7/R-M8 with code, not prose.** R-M7 (one concurrency model
  per process,
context not shared) and R-M8 (IR owns semantics) are accepted but unimplemented --
the MuPDF
backend still calls `fz_new_context(nullptr,nullptr,FZ_STORE_UNLIMITED)` with no
isolated lock set,
and no IR exists to own undo. This epic makes both checkable.
- **It unblocks the file the product is named for.** D-1 (sidecar) is the
  differentiator
(`kickoff.md` §4). D-1's re-anchoring ladder, tombstones and projection digest all
operate on the
IR. Delivering D-1 before the IR would put reconciliation in the UI -- the exact
layering violation
R-M10 forbids.

## What this epic is and is not

**Is:**
- The document model and value types that cross every seam (ADR-0011 R-M4: no engine
  object crosses
a seam).
- Geometry, text, budget and sidecar semantics that are testable on Linux with the
  `null` backend
-- so a bug reproduces without a window (AGENTS.md rule 2).
- The API freeze for `pdfcore` that later deltas extend by appending to the vtable
  (R-M3), never by
renumbering.

**Is not:**
- Not `src/render`, not `src/os/win32`, not `src/app` window -- those are Epic 2 (M1
  spike) and
remain spec-only except for the 10-line stub.
- Not D-6 undo command log beyond its data structures -- full `pc_txn_undo/redo` is
  Epic 4. This
epic provides the IR and value types the log will operate on.
- Not form validation rules, redaction proof, or UIA provider -- those are Epic 4/5/6
  and must not
be smuggled into a core PR.
- Not a second backend -- `pdfium`/`hayro` remain evaluated, not implemented. Adding
  a dependency
here violates R-M9.

## Architecture impact

```
include/pdfcore/          # frozen after story 3.5 -- append-only from here
  pdfcore.h  backend.h  doc.h  page.h  status.h  sidecar.h  text.h  budget.h  (new)

src/core/                 # NEW -- this epic creates it, no Windows/engine header ever
  doc/       -- IR, page boxes, geometry, pc_doc / pc_page value types, ownership
  text/      -- NFKC + casefold + whitespace collapse, pt-BR break, caret over combining marks
  budget/    -- memory + tile budget struct (read by render, decided here -- R-M8)
  sidecar/   -- reader/writer semantics: atomic write, lock, unknown-key preserve, stale, version gate
  geom/      -- user-space <-> device conversion, CropBox vs MediaBox, 4 rotations

src/backends/             # hardened, not enlarged -- backend_ratio must fall
  mupdf/  -- isolated lock set per context, allocator ownership R-M6, exception_bridge.h only TU with fz_try
  null/   -- deterministic pattern stays, now exercises the IR

src/cli/                  # extended -- sidecar lock/verify helpers for CI, txn replay stub for next epic
```

**Dependency arrow after this epic:** `app/cli -> pdfcore (core) -> backends` --
never the reverse.
`grep -rEn '#include *[<\"]\(windows|d2d1|mupdf|fitz)' src/core` must print 0. `sh
tools/layering-check.sh --strict` must print a number <=0.07.

## Acceptance criteria (grounded)

1. **`src/core` exists and compiles with `-fno-exceptions -fno-rtti` on both
toolchains.** `cmake
--preset linux-core && ctest --preset linux-core` green with ASan/UBSan; `cmake
--preset
win-cross-x64` green. `grep` gate above prints 0 on `src/core` and `src/render`.
2. **Geometry and page boxes are engine-free and tested without a window.**
MediaBox/CropBox/rotation 0/90/180/270 covered by unit tests that run on Linux with
the `null`
backend. Approval-free, value-type only.
3. **Text is NFKC + casefold + whitespace collapse with a pt-BR golden corpus.** Same
string breaks
identically on Linux and Windows; caret over combining marks arithmetic lives in
`src/core/text`
and is unit-tested headless (the IME plumbing stays in `src/os/win32` and is tested
only on the
Windows job -- ADR-0010).
4. **Budget lives in the core and is read by render.** `src/core/budget` decides
retention;
`src/render` only reads it. Eviction policy is unit-tested on Linux with no GPU
(R-M8).
5. **Sidecar semantics close 10 pending requirements.** R3.1 atomic tmp->rename +
fsync, R3.2
`.tynypdf.lock` 5-minute freshness, R4.1 unknown keys preserved, R6.1/R6.2
exclusions, R2.2 version
gate read-only, R2.3 id base32 10 chars, R5.1/R5.2 fingerprint vs mtime stale report
-- each with
`Verification:` artefact and `spec-check` 0 orphans (pending drops 17 -> 7).
6. **`pdfcore` C API frozen for M0 and golden-guarded.** `pc_status` append-only,
`detail` static
storage, `tests/golden/status_enum.txt` comparison green, and an intentional renumber
on a
throwaway branch fails the build (R-M12).
7. **Layering and swap budget healthy on a real tree.** `backend_line_ratio <= 0.07`
with ~2500 new
core lines, `layering-check --strict` exit 0, no `fz_*` or `FPDF_*` in
`app/cli/core`.
8. **Baseline re-measured with MuPDF/Release.** `tests/baseline.json` updated with
`tynypdf (mupdf
backend)` Release without ASan on the reference machine, 2 runs within 10% per
metric, machine_spec
pasted -- so M1/C3 comparisons are apples-to-apples.
9. **Rule zero.** Every number, sha or count in `epic-3-dod.md` is pasted from a
command output
included in that same document.

**Quick traceability:**

| Story | Reference doc | Key aspect |
|-------|---------------|------------|
| 3.1 | ADR-0011 R-M4/R-M8, docs/coding-standards.md §2/§3.1 | IR, page boxes, geometry, copy-out value types |
| 3.2 | docs/kickoff.md D-4, ADR-0001 value-oriented loops | NFKC text, pt-BR break, caret over combining marks |
| 3.3 | ADR-0007, src/features/sidecar/SPEC.md R3/R4/R6 | Atomic write, lock, unknown-key preserve, exclusions |
| 3.4 | src/features/sidecar/SPEC.md R2/R5, ADR-0007 §3/§5 | Version gate, id validation, fingerprint vs mtime staleness |
| 3.5 | ADR-0003, ADR-0011 R-M3/R-M12, ADR-0002 | pdfcore API freeze, golden header, contract final, ratio |

---

*Next step: stories 3.1-3.5 in order -- IR first, because everything else reads it.
Definitions in
`epic-3-stories.md`, tasks in `epic-3-technical-tasks.md`, evidence in
`epic-3-dod.md`.*
