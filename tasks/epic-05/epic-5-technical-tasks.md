# Epic 5 – Technical Tasks [grounded]

This is the only up-to-date plan. `[x]` filled during
execution; evidence in `epic-5-dod.md`.

> AI-Guard: allowlist/denylist + exact command per task
> + diff ceiling ≤350. Follow the list.

## 0. Pre-flight

- [ ] Read `AGENTS.md` R1-R12, `docs/coding-standards`
  §2, `adr/0011` R-M10/R-M11, `epic-5-overview.md`
- [ ] Read `docs/lessons.md` 2026-09-20/21
- [ ] Measure baseline in `epic-5-dod.md` §5.0:
  `git rev-parse HEAD`, `git ls-tree -r --name-only
  HEAD | wc -l`, `sh tools/check.sh`,
  `python3 tools/spec-check.py`,
  `sh tools/layering-check.sh --strict`,
  `ctest --preset linux-core | tail -n 20`
- [ ] Branch `feat/epic5-presentation` from `main`,
  stack `feat/5.1-measure` etc, each ≤350,
  squash-merge one at a time

---

## 5.1 Entry + measure spine — three refusals

**Allowlist:** `tools/bench-measure.sh` (extend),
`tests/bench/harness/*`, `docs/a11y/` (seed),
`epic-5-dod.md` §5.1
**Denylist:** `src/core/*` (no change), `src/render/*`

- [ ] Entry 1: `include/pdfcore/backend.h` + null
  backend exist — `sh tools/layering-check.sh`
  not `undefined`, paste. Else record as blocked.
- [ ] Entry 2: `cmake --preset win-cross-x64 &&
  cmake --build --preset win-cross-x64` produces
  `tynypdf.exe` — `ls -lh` pasted. Else record.
- [ ] Entry 3: `tools/win-probe/build.sh --probe gpu`
  on Windows session — hardware LUID not WARP
  pasted. Else WARP noted per ADR-0010.
- [ ] Harness: extend `bench-measure.sh` to read
  `tynypdf.ui` per-frame log (`frame_ms`,
  `input_ts`, `present_ts`) and `peak_rss_kib`.
  Run `--target tynypdf --binary <path> --runs 2
  --record-machine` twice within 10% (TOLERANCE
  0.10), both JSONs committed.
- [ ] Three refusals pasted: missing binary
  (`--binary /nope` → exit 2), non-PDF in corpus
  (`echo notpdf > corpus/bad.pdf` → exit 3),
  cross-machine compare (diff machine_spec → exit
  4).
- [ ] Gate: `check.sh` green.

## 5.2 Window, swapchain, blit at budget

**Allowlist:** `src/os/win32/window/*`,
`src/render/swapchain/*`,
`src/features/render/SPEC.md` (new),
`spikes/epic2-win32/*` (seed, read only)
**Denylist:** `src/core/*` (no parse), `src/backends/*`

- [ ] `src/os/win32/window/window.cc` — Per-Monitor
  V2 manifest, `WM_CREATE` DComp visual,
  `WM_SIZE` swapchain resize, `WM_PAINT` blit.
  No `hit_test`/`annot` here (R-M10).
- [ ] `src/render/swapchain/swapchain.cc` — DXGI
  swapchain + D2D target, `present()` called from
  window loop. `grep -rn "fitz\|mupdf" src/render`
  0 pasted.
- [ ] Blit: `pc_page_render` bytes → `ID2D1Bitmap`
  → `DrawBitmap` → `Present(1,0)`. Hash CLI
  `render --region 4000x3000` vs window `present`
  bytes equal — `sha256sum` both pasted side by
  side.
- [ ] Frame table: harness `frame_ms` p50/p99 +
  sample count, `p99 ≤33ms` pasted. If `p99 >33ms`
  after tiles+DComp, record as kill candidate.
- [ ] SPEC: `src/features/render/SPEC.md` with first
  file, `R15.1` "swapchain presents at 60fps" with
  `Verification: harness frame_ms p99` — `spec-check`
  0 orphans.
- [ ] Gate: `layering-check --strict` ≤0.07,
  `check.sh` green; diff ≤350.

## 5.3 Tiles, cachemap, budget in core

**Allowlist:** `src/render/tiles/*`,
`src/render/cachemap/*`,
`src/core/budget/*` (read only)
**Denylist:** no new dep without `dependency-policy`
  row, no `free(` of engine mem in render

- [ ] `src/render/tiles/tiles.cc` — tile `256x256`,
  `get(x,y,zoom)` + `put` LRU. `cachemap.cc` —
  `query(visible rect)` → tile set.
- [ ] Budget wiring: `pc_budget` from
  `src/core/budget` read in `tiles.cc` (`max_tiles`,
  `max_rss_mb`). Eviction `if (tiles > budget
  .max_tiles) evict_lru()` — decision in core,
  mechanism in render (R-M8).
- [ ] Test `test_budget.cc` on Linux headless:
  `max_tiles=1`, insert 2 → evict 1, `peak_rss`
  not grow on second pass. `ctest -R budget`
  green, no GPU.
- [ ] RSS proof: harness `peak_rss_kib` forward vs
  return pasted from one run, corpus `sha256sum`
  pasted, `grep "free(" src/render` 0 (R-M6).
- [ ] Gate: `check.sh` green; diff ≤350.

## 5.4 Cold start, DPI, gesture, caret

**Allowlist:** `src/os/win32/dpi/*`,
`tests/approvals/dpi-*`,
`tests/unit/test_caret.cc` (extend)
**Denylist:** `src/backends/*`

- [ ] Cold start: `QueryPerformanceCounter` from
  `WinMain` to first `Present` — 3 numbers +
  machine block pasted. Relative vs Sumatra
  recorded as open with `corpus-check rc 3` if
  baseline missing (do not reword).
- [ ] DPI: `SetProcessDpiAwarenessContext` Per-Monitor
  V2, `dpi.cc` `get_scale_for_monitor`. Test
  `150%`/`200%` rendered vs asked size byte-for-byte,
  `tests/approvals/dpi-150.png` with accept flow.
- [ ] Gesture: `WM_POINTER`/`WM_MOUSEWHEEL` ts vs
  `present_ts` from `tynypdf.ui` log — distribution
  `p50/p99` + sample count, `p99 ≤16ms` pasted.
- [ ] Caret: `src/core/text/caret.cc` already headless;
  add `tests/unit/test_caret.cc` ABNT2 + combining
  marks `e+U+0301` one step — `ctest -R caret` green
  on linux-core; IME composition only on Windows job
  (ADR-0010).
- [ ] Gate: `check.sh` green; diff ≤350.

## 5.5 A11y + verdict — keep or kill

**Allowlist:** `src/os/win32/uia/*`,
`docs/a11y/*`, `adr/*` (if kill)
**Denylist:** keep spike code only if keep verdict

- [ ] `src/os/win32/uia/uia.cc` — `IRawElementProvider`
  for window, `Navigate` page/zoom/focus, `GetPattern`
  `Value`. No engine handle.
- [ ] `docs/a11y/keyboard.md` + `narrator-nvda.md` —
  two scripts with exact announced text
  (`"Page 1 of 5, zoom 150%"` diffable).
  `ctest -R uia` on Windows job or manual paste.
- [ ] Verdict: one of two shapes pasted in `dod.md`
  §5.5: (a) 7 numbers table + spike kept as seed;
  (b) kill — new `adr/0012-...md` revisits D3 with
  frame table attached, spike code deleted in same PR,
  `kickoff §12` Skia row amended.
- [ ] `docs/lessons.md` updated in same PR as verdict
  (kickoff §9 step 6).
- [ ] Gate: `check.sh` green on last commit.

## 5.x Final epic gates

- [ ] `sh tools/check.sh` 14/14 on clean clone
  and PR head
- [ ] `gates-selftest 13/13`
- [ ] `ctest --preset linux-core` 0 failed,
  contract both backends, budget/caret/uia
- [ ] `docs-check` 0, `lang-check` 0, `naming 0`,
  `canonical 0`
- [ ] `spec-check` 0 orphans, pending 0 (all M1
  R14/R15 closed) or kill ADR + pending stays 4
  with reason
- [ ] `layering-check --strict` ≤0.07
- [ ] `grep windows.h src/core` 0,
  `grep fz_try | grep -v bridge` 0

**Checklist (for `epic-5-dod.md` §5.6):**

- [ ] 5.1: entry + 2 runs + 3 refusals
- [ ] 5.2: window/swapchain/blit p99 ≤33ms +
  byte identity + SPEC
- [ ] 5.3: tiles/cachemap 250MB + return pass +
  eviction headless
- [ ] 5.4: cold 3 numbers + DPI byte-equal +
  gesture p99 ≤16ms + caret headless
- [ ] 5.5: keyboard + Narrator/NVDA scripts +
  verdict keep/kill + ADR if kill
- [ ] Final gates green on `main`

---

*Next: do 5.1 first — harness not trusted, numbers not
trusted.*
