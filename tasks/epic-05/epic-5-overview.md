# Epic 5: Presentation — Window, Swapchain, Tiles and DPI (M1)

**Project:** tyny-pdf
**Context:** C++20 core behind C ABI (`pdfcore`),
Win32 + Direct2D over DComp/D3D11 swapchain,
LLVM-MinGW cross build from Linux,
`tynypdf-cli` as CI transport
(ADR-0001, ADR-0002, ADR-0010, ADR-0011).
**Goal:** answer with numbers if hand-written
Win32/D2D holds the floor without a framework.
After this epic, a 4000x3000 region blits at 60fps
p99 <33ms, 1000 pages at 3 tiles stays ≤250MB,
Per-Monitor V2 has no stretch, and every control
is keyboard + Narrator/NVDA reachable.

---

## Repo state (measured 2026-09-22, execution time)

Measured on `main @ a363b40` (post Epic 4) — tree
this epic starts from. Every number pasted from
command output in `epic-5-dod.md` §5.0.

- **14/14 gates green.** `sh tools/check.sh` — 14
  sections, `gates-selftest 13/13`, `spec-check
  14 specs / 62 reqs / 0 orphans / 4 pending
  (R15.1-R15.4)`, `layering 0.0429` on 33 files,
  0 violations.
- **Semantics done.** `src/core` ~1990 lines (doc,
  txn, text fallback/break/caret, budget, sidecar)
  with `pc_txn_*` replay byte-identical and fallback
  per run no tofu.
- **Presentation is spec-only.** `src/render/SPEC.md`
  R15.1-R15.4 and `src/os/win32/SPEC.md` R14.1-R14.3
  are `manual: implementation in progress`;
  `src/app/main.cc` still 10 lines stub;
  `spikes/epic2-win32/` holds `window.cc`,
  `swapchain.cc`, `tile_cache.cc` as spike seed only.
- **Toolchain proven.** `win-cross-x64` + `windows-msvc`
  both build `tynypdf.exe` via `build/win-probe`
  (headers compile, static runtime, LUID not WARP).
- **Baseline honest.** `tests/baseline.json`
  MuPDF native Release (open ~3.3ms) + Sumatra
  3.6.1/3.7pre dual channel.

## Why this epic now

- **Tripwire from kickoff §10.** "Hand-written UI
  costs more than predicted" — M1 kill criterion is
  the tripwire. Every later delta (forms, a11y editor,
  sidecar re-anchoring) spends on this surface.
- **Scheduled in parallel, now sequential.** Kickoff
  §11 said M1 runs in parallel with Epic 1 PRs 3/4
  because cheap while nobody blocked. Now core is
  done, M1 is the blocker for v1 — cheap to fail now,
  expensive after 6 UI PRs.
- **Open question with deadline.** `kickoff §12`
  "Skia as second render backend — no, revisit only
  if M1 kill fires, after M1". This epic is the only
  thing that can answer it.
- **Budget now testable.** `src/core/budget` from Epic
  4 is the lever `kickoff §10` names. Without it, tile
  sizing would be guessed in `src/render`.

## What this epic is and is not

**Is:**
- `src/os/win32/window/` + `src/render/swapchain/` —
  Per-Monitor V2 window, DComp visual, DXGI swapchain,
  blit of `pc_page_render` bytes via D2D.
- `src/render/tiles/` + `cachemap/` — deterministic
  LRU eviction reading `pc_budget` (R-M8).
- `src/os/win32/dpi/` — no bitmap stretch proof by
  bytes, pinch/wheel 16ms distribution, ABNT2 + caret
  headless + IME on Windows job only.
- `src/os/win32/uia/` — keyboard + UIA tree for
  Narrator/NVDA, first two scripts in `docs/a11y/`.

**Is not:**
- Not D-2 forms, D-5 a11y editor full, D-1 re-anchoring
  ladder, D-3 redaction — Epics 6+.
- Not Skia — only if kill fires, then new ADR with
  frame table attached.
- Not new dep — no cache lib without ADR + row in
  `docs/dependency-policy.md` (R-M9).

## Architecture impact

```
src/os/win32/       # owns window, dpi, uia, input
  window/  dpi/  uia/  input/
src/render/         # owns swapchain, tiles, cachemap
  swapchain/  tiles/  cachemap/
src/core/budget/    # READ by render, DECIDED here (R-M8)
spikes/epic2-win32/  # seed only, not shipped
```

**Arrow:** `render/os -> core(budget) -> backends`
— `grep windows.h src/core` 0,
`grep fz_ src/render` 0,
`layering-check --strict` ≤0.07.

## Acceptance criteria (grounded)

1. **7 M1 rows have numbers + commands pasted in
   `epic-5-dod.md`.** "60fps" without frame table is
   not a result.
2. **Numbers from reference box** (`~/.wslconfig`
   12GB/8cpu, Windows session if WARP) via
   `tools/bench-measure.sh --record-machine`, not hand-typed.
3. **Repro within 10%** — second consecutive run
   within `TOLERANCE=0.10`, both JSONs pasted.
4. **Layering held.** `layering-check --strict`
   `≤0.07` while surface exists (R-M10).
5. **SPEC with first file.** `src/features/render/
   SPEC.md` written with first file in that dir,
   never before, `spec-check` 0 orphans.
6. **Verdict written.** 7 numbers and code kept, or
   kill fired and new ADR revisits D3 with table.

**Traceability:**

| Story | Ref | Aspect |
|-------|-----|--------|
| 5.1 | kickoff M1.1, ADR-0010 | Entry + measure spine |
| 5.2 | kickoff M1.1, R-M10 | Window/swapchain/blit |
| 5.3 | kickoff M1.2, R-M8 | Tiles/cachemap/budget |
| 5.4 | kickoff M1.3-4,6,7 | Cold start, DPI, gesture |
| 5.5 | kickoff M1.5 + kill | A11y + verdict |

---

*Next: stories 5.1-5.5 in order. Tasks in
`epic-5-technical-tasks.md`.*
