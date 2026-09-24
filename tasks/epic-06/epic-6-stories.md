# Epic 6 – Stories (Acceptance) [grounded]

Each AC cites real file it comes from. Last bullets are
executable checks. `epic-6-dod.md` requires pasted output.

| # | Story | Acceptance Criteria (grounded) | Real Reference |
|---|-------|--------------------------------|----------------|
| 6.1 | **Selection hit-test + geometry reconcile** | <br>- `src/core/selection/selection.cc` owns `pc_selection_hit_test(x,y,page)` → IR quads, reconciles engine geometry to IR via `pc_doc` (no `fitz` in `src/render`/`src/os`)<br>- Headless `tynypdf-cli select --page 0 --rect x,y,w,h --out` reproduces same quads as window click — `sha256` CLI vs window pasted equal<br>- `src/features/selection/SPEC.md` with first file, `R32.1` with `Verification:` and `spec-check` 0 orphans<br>- `layering-check --strict` 0, `grep windows.h src/core` 0; `sh tools/check.sh` green | kickoff D-1, ADR-0011 R-M8/R-M10 |
| 6.2 | **Caret + input wiring (no tofu, ABNT2)** | <br>- `src/core/text/caret.cc` already headless; `src/os/win32/input/input.cc` maps `WM_LBUTTONDOWN/MOUSEMOVE/KEYDOWN` → `pc_selection_*` + caret, `Present(0,0)` headless still works<br>- ABNT2 + combining `e+U+0301` one logical step, selection respects per-run fallback (no tofu) — `ctest -R caret,selection` on linux-core<br>- `docs/a11y/selection.md` script with exact announced text diffable; `sh tools/check.sh` green | kickoff D-4, ADR-0010 |
| 6.3 | **Search + highlight quads (budget-aware)** | <br>- `src/core/search/search.cc` indexed search over IR text, returns quads + page list; `src/render/tiles` reads highlights via `pc_budget` (no new dep, R-M9)<br>- 1000 hits on `tests/bench/corpus/1000p.pdf` in <100ms headless (`bench-measure.sh --search` `p50/p99` pasted) and `peak_rss_kib` stays ≤250MB (same corpus `sha256` pasted)<br>- Highlights survive `pc_txn_to_json/from_json` round-trip (search is not txn, but quads serialize via sidecar `highlights` array with `diff 0`)<br>- `sh tools/check.sh` green | kickoff D-1, ADR-0011 R-M8 |
| 6.4 | **Annotations as undoable txn** | <br>- `src/core/annot/annot.cc` — `highlight/underline/strike` create/edit/delete are `pc_command` variants; `pc_txn_undo/redo` restores quads + page — `test_annot.cc` 5 undo/5 redo + hash equality pasted<br>- Every annot survives `pc_txn_to_json/from_json` + `sidecar-fmt.py` canonical with `diff 0` — `tynypdf-cli txn replay --out` hash pasted equal<br>- `grep fz_try src/core/annot` 0 outside `bridge`; `sh tools/check.sh` green | kickoff D-6, ADR-0007, ADR-0003 §6 |
| 6.5 | **Re-anchoring + verdict** | <br>- Ladder `docs/sidecar-anchoring.md` — annot quads re-anchor after text edit (insert/delete before annot) via IR offsets, not page coords — `test_reanchor.cc` headless pasted<br>- Verdict is one of two shapes: (a) 5 numbers in `dod.md` and interaction kept; (b) kill fired (hit-test cannot reconcile without engine in render) and new ADR revisits D-1 with table, interaction code deleted in same PR<br>- `docs/lessons.md` updated in same PR as verdict (kickoff §9 step 6)<br>- `sh tools/check.sh` + `gates-selftest` green on last commit | kickoff D-1 + kill, docs/testing-playbook.md |

**Traceability:**

| Story | Ref |
|-------|-----|
| 6.1 | Selection hit-test |
| 6.2 | Caret + input |
| 6.3 | Search highlights |
| 6.4 | Annot txn |
| 6.5 | Re-anchor + verdict |

---

*Story 6.1 first — geometry not trusted, no highlight.*
