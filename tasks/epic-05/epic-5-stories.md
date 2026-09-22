# Epic 5 – Stories (Acceptance) [grounded]

Each AC cites real file it comes from. Last bullets are
executable checks. `epic-5-dod.md` requires pasted output.

| # | Story | Acceptance Criteria (grounded) | Real Reference |
|---|-------|--------------------------------|----------------|
| 5.1 | **Entry + measure spine — three refusals** | <br>- Entry 4 checks pasted: `include/pdfcore/backend.h` + `src/backends/null` exists and `layering-check` not `undefined`; `cmake --preset win-cross-x64 && cmake --build --preset win-cross-x64` produces `build/win-cross-x64/Release/tynypdf.exe`; `tools/win-probe/build.sh --probe gpu` on Windows session prints hardware LUID not WARP; `docs/lessons.md` read<br>- Harness `tools/bench-measure.sh --target tynypdf --binary <path> --runs 2 --record-machine` run twice within 10% per metric, machine block from tool pasted, both JSONs committed<br>- Three refusals pasted: missing binary, non-PDF in corpus, cross-machine compare<br>- `sh tools/check.sh` green | kickoff M1, ADR-0010, docs/dev-environment.md, epic-2-overview §Entry |
| 5.2 | **Window, swapchain, blit at budget** | <br>- `src/os/win32/window/` owns WinProc + loop, `src/render/swapchain/` owns DComp visual + DXGI swapchain; `grep -rn "fitz\|mupdf" src/render src/os` 0 and `grep "parse\|hit_test\|annot" src/render` 0 — `layering-check --strict` 0<br>- 4000x3000 region blits at 60fps sustained, p99 ≤33ms with frame table pasted (harness JSON `frame_ms`); bytes equal `tynypdf-cli render --page 0 --region 4000x3000 --out` hash vs window presented bytes hash<br>- `src/features/render/SPEC.md` exists with first file in that dir, one-line EARS `R15.x` with `Verification:` and `spec-check` 0 orphans<br>- `backend_line_ratio` ≤0.07 while code exists; `sh tools/check.sh` green | kickoff M1.1, ADR-0002, ADR-0011 R-M10/R-M11/R-M13 |
| 5.3 | **Tiles, cachemap, budget in core** | <br>- `src/render/tiles/` + `cachemap/` mechanism; decision of what stays resident in `src/core/budget/` — render reads `pc_budget`, never invents (R-M8)<br>- 1000 pages at 3 tiles each ≤250MB peak RSS, return pass does not grow — both peaks from one harness run pasted, corpus `sha256sum` pasted<br>- Eviction LRU+pin unit-tested on Linux with no GPU (`ctest -R budget`); `grep "free(" src/render` no engine free (R-M6)<br>- No new dep for cache; `sh tools/check.sh` green | kickoff M1.2 + §10 response, ADR-0011 R-M6/R-M8 |
| 5.4 | **Cold start, DPI, gesture, caret** | <br>- Cold start absolute ≤300ms from process start to presenting frame — 3 numbers + machine block pasted; relative vs Sumatra recorded as open with `corpus-check` rc 3 if baseline missing (Epic 1 debt, not reworded)<br>- Per-Monitor V2: rendered at 150%/200% vs asked at scaled size compared byte-for-byte, `tests/approvals/dpi-*.png` with accept flow — no stretch<br>- Wheel/pinch 1:1, p99 ≤16ms with sample count + p50/p99 distribution from `tynypdf.ui` per-frame log<br>- ABNT2 + pt-BR caret over combining marks headless in `src/core/text` (`ctest -R caret`); IME composition only on Windows job (ADR-0010)<br>- `sh tools/check.sh` green | kickoff M1.3-4,6,7, ADR-0002, ADR-0010 |
| 5.5 | **A11y + verdict — keep or kill** | <br>- Every control keyboard reachable; Narrator + NVDA announce page/zoom/focus — first two scripts in `docs/a11y/` with exact announced text diffable; UIA tree in `src/os/win32/uia/` with assertions on Windows job<br>- Verdict is one of two shapes: (a) 7 numbers in `dod.md` and spike code kept as viewer seed; (b) kill fired (swapchain/blit cannot hold 60fps after tiles+DComp) and new ADR revisits D3 with frame table attached, spike code deleted in same PR<br>- `docs/kickoff.md` §12 Skia row amended by reference if ADR, `docs/lessons.md` updated in same PR as verdict (kickoff §9 step 6)<br>- `sh tools/check.sh` + `gates-selftest` green on last commit | kickoff M1.5 + kill, docs/testing-playbook.md, ADR-0002 |

**Traceability:**

| Story | Ref |
|-------|-----|
| 5.1 | Entry, measure, refusals |
| 5.2 | Window/swapchain/blit |
| 5.3 | Tiles/budget |
| 5.4 | Cold/DPI/gesture |
| 5.5 | A11y + verdict |

---

*Story 5.1 first — harness not trusted, numbers not trusted.*
