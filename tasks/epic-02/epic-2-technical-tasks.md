# Epic 2 – Technical Tasks [grounded]

This document is the task list; the acceptance wording lives in `epic-2-stories.md` and the
evidence in `epic-2-dod.md`. `[x]` marks are filled in by whoever ran the command, with the output
pasted in `epic-2-dod.md` first - `docs/lessons.md` and `AGENTS.md` Critical Rule 4, and rule zero
below.

Rule zero for this epic: no number, hash or count is written from memory. `docs/kickoff.md`
section 9 step 2 also says to write the failing test first and *see it fail*; in a spike that
means the harness must print a wrong number before it is allowed to print a right one (story 2.1,
task 3).

Branch naming per `docs/git-workflow.md`: `feat/m1-<slug>`, one branch per story, diffs kept near
400 lines (ADR-0009 rule 2). A spike is not an exemption from that; it is where an exemption is
usually requested and refused.

## 2.1 The measurement spine, before any pixels

- [ ] Run the four entry conditions from `epic-2-overview.md` and paste their output, including
  the ones that fail. A failing entry condition is not a reason to skip it.
- [ ] `cmake --preset win-cross-x64 && cmake --build --preset win-cross-x64 && file
  build/win-cross-x64/Release/tynypdf.exe` - if `tools/cmake/toolchain-llvm-mingw.cmake` refuses
  for want of the pinned tarball, that refusal is the answer for Epic 1 story 1.2's remaining
  half, and this epic's timing work waits rather than substituting a system compiler
- [ ] `sh tools/win-probe/build.sh --probe gpu` on the Windows session; paste the adapter LUID and
  the `D3D_FEATURE_LEVEL`. If it reports the software rasteriser, write one line into
  `epic-2-dod.md` naming which machine every timing number came from, because that is the
  condition ADR-0010 attaches to M1 timing
- [ ] `python3 tools/bench-measure.sh --target tynypdf --binary
  build/win-cross-x64/Release/tynypdf.exe --corpus <pinned corpus dir> --runs 2 --record-machine
  --out tests/spike-tynypdf.json`; commit the JSON, not a summary of it
- [ ] Deliberately feed the harness a binary that is not there, and a corpus with a non-PDF in it,
  and paste both refusals (`bench-measure: ... does not exist`). The harness is trusted for one
  reason: it says no. Proving that is the "see it fail" step applied to the instrument
- [ ] Add the frame-interval source to the CLI's JSON reporter output only if the existing metrics
  do not already carry per-frame data: `tools/bench-measure.sh` derives `scroll_frame_p99_ms` from
  frames, so a second p99 implementation in the spike is a duplicate that will disagree
- [ ] Open `spike/day-1.md` .. `spike/day-3.md` under `tasks/epic-02/` **only if** the day's outcome
  changes what the next day attempts; otherwise the sessions are recorded in the PR descriptions
  and `epic-2-dod.md` section 2. A diary is not an artefact when the PR body already is
- [ ] `sh tools/check.sh` green at each session boundary

## 2.2 The surface

- [ ] `src/os/win32/window/` - window class, create, resize, message loop, and the frame-present
  notification; no document type name appears in this directory (R-M10)
- [ ] `src/render/swapchain/` - DComp visual + DXGI swapchain, `DXGI_SWAP_CHAIN_DESC1` style setup
  as the `tools/win-probe/d2d_probe.c` probe already proves compiles under both toolchains; reuse
  the probe's header set (`d2d1_1.h`, `dwrite_2.h`, `d3d11_1.h`, `dxgi1_4.h`) rather than reaching
  for `d2d1_3.h`/`dxgi1_6.h` and discovering the pin does not ship them
- [ ] `src/render/cachemap/` and `src/render/tiles/` skeletons land in 2.3, not here: a swapchain
  with no cache is the thing criterion 1 needs, and building both at once makes a 60 fps failure
  ambiguous
- [ ] Blit entry: the same `pc_page_render` output the CLI writes to PNG, presented unscaled into
  the swapchain back buffer; pixel hash equality between the two paths is the test, not a
  screenshot a human compares
- [ ] `src/features/render/SPEC.md` in the same commit as the first file under
  `src/features/render/`, EARS one-liners with ids and `Verification:` targets (ADR-0011 R-M13),
  carrying the frame budget as requirements - `python3 tools/spec-check.py` must report 0 orphans
  for them
- [ ] `sh tools/layering-check.sh --strict` exits 0; `backend_line_ratio` is a number at or
  below 0.15 and the value is pasted even when it is comfortable
- [ ] `sh tools/format-check.sh` clean on the new files (`.clang-format` is the decision, and the
  hook is what enforces it per commit)

## 2.3 Tiles and the budget

- [ ] `src/core/budget/` owns: how many tiles are resident, which one is dropped, and what the
  ceiling is in bytes. One `SPEC.md` per capability directory (ADR-0002 rule 1), so `budget` gets
  its spec with its first file, never before it
- [ ] `src/render/tiles/` implements residency against that budget, reading it;
  `src/render/cachemap/` maps page-plus-region to a resident tile. Neither directory may decide
  eviction policy - that is the sentence `sh tools/layering-check.sh` cannot check, so the
  code review says it and the unit tests make it obvious
- [ ] Allocation ownership: the backend that allocates the pixel buffer frees it (R-M6); the
  render layer holds a view, and the spike does not "temporarily" call `free()` on engine memory
- [ ] Measurement task: 1000 pages open, three tiles each; forward scroll then return scroll;
  paste both `peak_rss_kib` values from one harness run, and state the corpus path and its sha256
  list next to them
- [ ] Unit tests for the eviction rule run headless in `linux-core`; if a test needs a window to
  exercise the policy, the policy is in the wrong layer - move it to `src/core/budget/` instead of
  adding a display to CI
- [ ] No new third-party dependency, including header-only ones. If caching tempts one, it is an
  `docs/dependency-policy.md` register row in the same PR as the first `#include`, with the ADR
  that states its cost (R-M9)

## 2.4 Cold start

- [ ] One timestamp pair written by `tynypdf` at startup: process start (from the OS) and the
  first frame that presents page 1, on the `tynypdf.ui` log subsystem named in `docs/naming.md`.
  That line is the measurement; a stopwatch around a double-click is not
- [ ] Absolute target from `docs/kickoff.md` M1 row 3 (`<= 300 ms` on the reference machine);
  report the number with the machine block from `--record-machine`
- [ ] The relative target ("vs measured Sumatra") is written as an open row with its cause:
  `tests/baseline.json` absent, `python3 tools/corpus-check.py --root .` exits 3, Epic 1 story 1.5
  is owner debt. **Do not** redefine M1 row 3 to "absolute only" in this document or in the DoD;
  the brief's wording is the criterion and a milestone may only be downgraded through the
  downgrade gate (`docs/kickoff.md` section 7, gate 3), which edits an `Out of scope` section in
  the relevant `SPEC.md` in the same PR
- [ ] If the number is over 300 ms, record it as over. A spike that reports a miss is information;
  a spike that reports a miss as a pass is how the framework decision gets made blind

## 2.5 DPI, composition, caret, gesture

- [ ] `src/os/win32/dpi/`: Per-Monitor V2 awareness, and the scale applied to layout at 150 %/200
  % with no bitmap stretch. The check is a pair of renderings of the same page - asked at the
  scaled size versus asked at 100 % and upscaled - compared byte-wise, with the two files stored
  under `tests/approvals/` and the documented accept flow (ADR-0002)
- [ ] `src/os/win32/window/`: wheel and pinch handling emits one log line per frame carrying the
  input timestamp and the presenting timestamp; report p50 and p99 of the difference against the
  16 ms ceiling from M1 row 7, never an average alone
- [ ] `src/core/text`: caret movement across combining marks is arithmetic on the IR and gets unit
  tests there; `src/os/win32/window/` only forwards composition messages. The split is the point:
  it is what lets the pt-BR composition rule be tested by the Linux job at all
- [ ] IME composition in a text field, ABNT2 layout: Windows-side test only, run on the Windows
  job, never inferred from the Linux side (ADR-0010)
- [ ] No MSVC-only extension in anything under `src/core`, and a MinGW-only warning failure is
  fixed by dropping the vendor extension rather than adding an `#ifdef` (ADR-0010 consequences)

## 2.6 Narration and the verdict

- [ ] `src/os/win32/uia/`: expose the tree a screen reader consumes for the spike's controls -
  page indicator, zoom, focus rectangle - and assert it from the Windows job where it can run
- [ ] `docs/a11y/` gains its first two files: a Narrator script and an NVDA script, each with the
  exact expected announced text, because "announces correctly" has to be a diff (M5 owns the
  complete delta; say so in the file header so nobody reads these two as the whole requirement)
- [ ] Every control reachable from the keyboard, with the traversal order recorded; a control that
  needs the mouse is a defect in the spike, not a follow-up
- [ ] Verdict commit, one of:
- [ ] pass: the seven criteria with their numbers in `epic-2-dod.md`, and a written note that the
  spike's code is the viewer's seed (which is when the "composition root stays thin" rule of
  ADR-0002 rule 3 starts being enforced against this code)
- [ ] kill: a new ADR under `adr/`, numbered at creation so `docs-check.py` indexes it, revisiting
  D3 with the frame table and the swapchain numbers; `docs/kickoff.md` section 12's Skia row then
  points at it; and the spike's rendering code is deleted in the same PR, not kept as a reference
- [ ] `docs/lessons.md` gains the entry the spike taught, in the same PR (kickoff section 9 step
  6: "not in a retrospective nobody reads")

## 2.x Final epic gates

- [ ] `sh tools/check.sh` (exit 0) on a clean clone of the branch and on the merge commit
- [ ] `sh tools/gates-selftest.sh` -> `gates-selftest: OK (8/8 suites hold)`
- [ ] `python3 tools/diff-scan.py --tree` -> `D1-D8 quiet`; `python3 tools/diff-scan.py --self-test`
  -> `17/17 properties hold`
- [ ] `sh tools/layering-check.sh --strict` -> exit 0, `backend_line_ratio` printed as a
  number
- [ ] `python3 tools/spec-check.py` -> 0 orphans and no pending item for `src/features/render`
- [ ] `python3 tools/docs-check.py` -> 0 problems, including these five files and every path they
  cite under `tools/`
- [ ] `python3 tools/bench-measure.sh --compare <run1.json> <run2.json>` -> within tolerance,
  output pasted; the CPU of both runs identical, because a machine change between runs is a
  failure in that tool by design
- [ ] `sh tools/format-check.sh` clean; `sh tools/win-probe/build.sh --self-test` still 5/5
- [ ] `cmake --preset linux-core && cmake --build --preset linux-core && ctest --preset
  linux-core` green, with more than the two invariant tests that exist today, and the count
  reported
- [ ] `epic-2-dod.md` self-audit run before any hand-off, with its output pasted

**Epic 2 completion checklist:**

- [ ] 2.1: entry conditions run and pasted; harness proven to refuse; two runs within 10 %
- [ ] 2.2: one window, one swapchain, 4000x3000 at the frame budget, identical bytes to the CLI
  path, render `SPEC.md` written with its first file
- [ ] 2.3: tiles and cachemap reading a core-owned budget; RSS ceiling met and no growth on scroll
  back, both passes pasted
- [ ] 2.4: absolute cold start recorded with its machine block; the relative half recorded as open
  with its cause, not rewritten
- [ ] 2.5: Per-Monitor V2 with pixel evidence, gesture p99 under 16 ms, composition and caret
  tested in the layer where they can be tested
- [ ] 2.6: narration scripts with exact expected text, keyboard traversal complete, and a verdict
  - numbers as the contract, or the kill criterion's ADR with the code deleted
- [ ] Final gates green, evidence pasted in `epic-2-dod.md`

---

*Next step: 2.1 alone first. Stories in `epic-2-stories.md`, test levels in `epic-2-testing.md`,
and the evidence file is `epic-2-dod.md`. Three days is the box; the gate is the seven rows of
M1.*
