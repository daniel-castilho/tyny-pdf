# Epic 6: Interaction — Selection, Search and Annotations (D-1 + D-6)

**Project:** tyny-pdf
**Context:** C++20 core behind C ABI (`pdfcore`),
Win32 + Direct2D over DComp/D3D11 swapchain,
viewer loop with tiles/cachemap/budget (Story 1.5),
`tynypdf-cli` + `bench-measure.sh` + `win32-ui-selftest.sh`
(ADR-0001, ADR-0002, ADR-0010, ADR-0011).
**Goal:** text is selectable, searchable and annotatable
without leaving the IR. After this epic, hit-testing
reconciles geometry headless, caret follows selection,
search highlights 1000 pages in <100ms, and every
annotation is an undoable `pc_txn_*` that survives
`sidecar` round-trip (ADR-0007).

---

## Repo state (measured 2026-09-24, execution time)

Measured on `main @ 5863926` (post Story 1.5) — tree
this epic starts from. Every number pasted from
command output in `epic-6-dod.md` §6.0.

- **14/14 gates green.** `sh tools/check.sh` — 14
  sections, `gates-selftest 13/13`, `spec-check
  21 specs / 80 reqs / 0 orphans / 0 pending`,
  `layering 0.0254` on 61 source files, 0 violations.
- **Presentation done.** `src/os/win32/window` +
  `swapchain` (R15.1, R24.4-R24.6) + `src/render/tiles`
  + `cachemap` + `src/core/budget` (R27-R29) +
  `src/app` viewer loop with 1000-page corpus
  (R30.1-R31.1) — `blit p99 0.68ms` + `RSS 44.6 MiB`
  measured via `bench-measure.sh`.
- **Semantics done.** `src/core/doc + txn + text`
  (fallback/break/caret) + `sidecar` canonical JSON
  — `pc_txn_to_json/from_json` byte-identical,
  `tynypdf-cli txn replay` diff 0.
- **Interaction is spec-only.** `src/core/selection`,
  `src/core/search`, `src/core/annot` do not exist
  yet (no directory, no source); nor do
  `src/features/selection|search|annot/SPEC.md`.
  No backend exposes text layout either: the
  vtable (abi 1.1) ends at face coverage, so 6.1
  appends the bridge (`PC_CAP_TEXT_LAYOUT`,
  `page_text_layout`) as an abi 1.2 event.
- **Toolchain proven.** `win-cross-x64` +
  `windows-msvc` both build `tynypdf.exe` with viewer
  loop; `tests/bench/corpus/corpus-1000p.pdf` committed,
  sha256 `e8da98f3...` pinned by
  `tests/unit/test_corpus_contract.cc` (pasted in
  `tasks/epic-01/story-1.5-content-viewer.md`). The
  corpus is rect-only (no text streams), so 6.3
  regenerates it with a text line per page for the
  search bench; the tile/RSS numbers recorded at 1.5
  stay historical.

## Why this epic now

- **Tripwire from kickoff §10.** "Hand-written
  selection costs more than predicted" — without
  geometry reconciliation in core, every later delta
  (forms D-2, redaction D-3, a11y editor D-5) spends
  on the wrong layer.
- **Viewer without interaction is a demo.** M1 (Epic 5)
  + 1.5 proved we can blit. Now we prove we can
  `hit_test` the IR headless so a bug reproduces in
  `tynypdf-cli` on Linux (R-M8, R-M10).
- **Budget now testable for highlights.** `pc_budget`
  from Epic 4 + `tiles` from 5.3 is the lever for
  highlight quads (1000 hits must not blow `250MB`).
- **Undo must own annotations.** Every `annot` is a
  `pc_command` in the txn log — if not, sidecar
  re-anchoring (D-1 ladder) has no anchor.

## What this epic is and is not

**Is:**
- `src/core/selection/` — hit-testing, geometry
  reconciliation, per-run fallback aware.
- `src/core/search/` — indexed search, highlight
  quads, budget-aware.
- `src/core/annot/` — create/edit/delete as
  `pc_txn_*` commands, sidecar round-trip.
- `src/os/win32/input/` — mouse/keyboard →
  selection/caret wiring (presentation only).
- `docs/a11y/selection.md` — keyboard + UIA
  selection pattern.

**Is not:**
- Not D-2 forms, D-3 redaction, D-5 full a11y
  editor — Epics 7+.
- Not Skia — only if M1 kill fired (did not).
- Not new dep — no ICU/regex lib without ADR +
  row in `docs/dependency-policy.md` (R-M9).

## Architecture impact

```
src/core/selection/  # owns hit_test, reconcile
src/core/search/     # owns index, highlights
src/core/annot/      # owns txn commands for annot
src/os/win32/input/  # owns mouse → selection
src/render/tiles/    # reads highlights (R-M8)
```

**Arrow:** `os/render -> core(selection/search/annot)
-> backends` — `grep windows.h src/core` 0,
`grep fz_ src/render` 0,
`layering-check --strict` ≤0.07.

## Acceptance criteria (grounded)

1. **5 stories have pasted evidence in
   `epic-6-dod.md`.** Selection without headless
   `tynypdf-cli` repro is not a result.
2. **Highlight budget held.** 1000 hits on 1000p
   stays ≤250MB (same harness `peak_rss_kib`).
3. **Sidecar round-trip.** Every annot survives
   `pc_txn_to_json/from_json` + `sidecar-fmt.py`
   with `diff 0`.
4. **Layering held.** `layering-check --strict`
   `≤0.07` while selection exists (R-M10).
5. **SPEC with first file.** Each
   `src/features/<cap>/SPEC.md` written with first
   file in that dir, never before, `spec-check`
   0 orphans.
6. **A11y.** Selection announced via UIA
   `TextPattern` for Narrator/NVDA, script in
   `docs/a11y/` diffable.

**Traceability:**

| Story | Ref | Aspect |
|-------|-----|--------|
| 6.1 | kickoff D-1, ADR-0011 R-M10 | Selection hit-test |
| 6.2 | kickoff D-4, R-M8 | Caret + input wiring |
| 6.3 | kickoff D-1, R-M8 | Search + highlights |
| 6.4 | kickoff D-6, ADR-0007 | Annotations as txn |
| 6.5 | kickoff D-1 + kill | Re-anchoring + verdict |

---

*Next: stories 6.1-6.5 in order. Tasks in
`epic-6-technical-tasks.md`.*
