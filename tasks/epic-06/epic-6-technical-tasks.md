# Epic 6 – Technical Tasks [grounded]

This is the only up-to-date plan. `[x]` filled during
execution; evidence in `epic-6-dod.md`.

> AI-Guard: allowlist/denylist + exact command per task
> + diff ceiling ≤350. Follow the list.

**Decisions locked at planning (2026-09-24):**

1. **6.1 appends the text-layout ABI** (abi 1.2):
   `PC_CAP_TEXT_LAYOUT`, `page_text_layout` +
   free, `pc_quad` value type. The engine's text
   geometry crosses the seam once, copied out
   into our value types (ADR-0011 R-M1..R-M4);
   null backend answers `PC_ERR_CAPABILITY`
   (R-M5). Each ABI event ships the version bump,
   the migration note and the golden error-code
   test.
2. **6.3 regenerates the corpus with text**: the
   committed `corpus-1000p.pdf` is rect-only, so
   the "1000 hits <100ms" acceptance cannot run
   on it. The generator gains one text line per
   page (base-14 Helvetica, no new dependency),
   the sha moves, `test_corpus_contract.cc` is
   updated in the same commit, and the viewer
   bench is re-run once to re-verify R30.2/R15.1
   on the new corpus (1.5's recorded rows stay
   historical).
3. **6.4 extends `pc_command` in place** (v2 via
   the size field: quad + color + page after a
   size check), new `PC_CMD_*` values appended;
   `pc_txn_to_json/from_json` round-trips `diff 0`.

## 0. Pre-flight

- [x] Read `AGENTS.md` R1-R12, `docs/coding-standards`
  §2, `adr/0011` R-M10/R-M11, `epic-6-overview.md`
- [x] Read `docs/lessons.md` 2026-09-24
- [x] Measure baseline in `epic-6-dod.md` §6.0:
  `git rev-parse HEAD`, `git ls-tree -r --name-only
  HEAD | wc -l`, `sh tools/check.sh`,
  `python3 tools/spec-check.py`,
  `sh tools/layering-check.sh --strict`,
  `ctest --preset linux-core | tail -n 20`
- [ ] Branch `feat/epic6-interaction` from `main`,
  stack `feat/6.1-selection` etc, each ≤350,
  squash-merge one at a time

---

## 6.1 Selection hit-test + geometry reconcile

**Allowlist:** `src/core/selection/*`,
`src/features/selection/SPEC.md` (new),
`src/cli/*` (new `select` subcommand),
`include/pdfcore/backend.h` (abi 1.2 append:
`PC_CAP_TEXT_LAYOUT`, `page_text_layout` + free),
`include/pdfcore/geom.h` (`pc_quad`),
`include/pdfcore/selection.h` (new, core API),
`src/backends/mupdf/*`, `src/backends/null/*`
(vtable append + capability answer),
`src/os/win32/window/*` + `include/pdfcore/window.h`
(`TYNYPDF_CLICKS` selftest hook, the `TYNYPDF_WHEELS`
precedent), `tools/win32-ui-selftest.sh` (click
parity check), `tests/unit/test_selection.cc` (new),
`tests/contract/*` (capability contract),
`tests/golden/*` (abi event goldens)
**Denylist:** `src/render/*` (no hit_test),
`src/os/*` (no geometry)

- [ ] `src/core/selection/selection.cc` —
  `pc_selection_hit_test(page,x,y)` → `pc_quad[]`
  via `pc_doc` IR. Reconcile engine quads
  (`fz_quad` from backends) to IR coords in
  `selection.cc` only — `grep -rn "fitz\|fz_"
  src/render src/os` 0 pasted.
- [ ] CLI: `tynypdf-cli select --page 0 --rect
  x,y,w,h --out quads.json` — headless repro,
  `sha256sum quads.json` vs window click pasted
  equal.
- [ ] SPEC: `src/features/selection/SPEC.md` with
  first file, `R32.1` "hit-test reconciles" with
  `Verification: tynypdf-cli select` —
  `spec-check` 0 orphans.
- [ ] Gate: `layering-check --strict` ≤0.07,
  `check.sh` green; diff ≤350.

## 6.2 Caret + input wiring

**Allowlist:** `src/os/win32/input/*` (new),
`src/core/text/caret.cc` (extend only if a defect
appears — the headless caret is done, R23.x),
`include/pdfcore/window.h` (input callbacks
append — ABI event with migration note + golden),
`src/os/win32/window/*` (dispatch to the
callbacks), `src/os/win32/uia/uia_state.cc` +
`include/pdfcore/uia.h` (selection in the
announced text), `tests/unit/test_uia.cc` (golden
update), `docs/a11y/selection.md` (new),
`tools/win32-ui-selftest.sh` (click/caret checks)
**Denylist:** `src/backends/*`

- [ ] `src/os/win32/input/input.cc` —
  `WM_LBUTTONDOWN/MOUSEMOVE` drag →
  `pc_selection_set`, `WM_KEYDOWN` arrows →
  `pc_caret_move`, `Present(0,0)` headless.
- [ ] Caret: ABNT2 + `e+U+0301` one step,
  per-run fallback no tofu — `ctest -R caret`
  + `selection` green on linux-core.
- [ ] A11y: `docs/a11y/selection.md` with exact
  announced text `Selected Page 1 line 2`
  diffable.
- [ ] Gate: `check.sh` green; diff ≤350.

## 6.3 Search + highlight quads

**Allowlist:** `src/core/search/*`,
`src/render/tiles/*` (read only),
`src/features/search/SPEC.md` (new),
`tests/bench/corpus/gen_corpus.py` +
`corpus-1000p.pdf` + `manifest.*`
(regeneration with text — decision 2),
`tests/unit/test_corpus_contract.cc` (sha update),
`tests/unit/test_search.cc` (new),
`tools/bench-measure.sh` +
`tests/bench/harness/run_benchmark.py` (`--search`
mode), `src/core/sidecar/*` + sidecar schema test
if the highlights serialization needs it
**Denylist:** no ICU/regex without ADR row

- [ ] `src/core/search/search.cc` — index the
  per-page text 6.1's `page_text_layout` bridge
  copies out of the engine (the IR holds no text
  today; search builds its own value-type index),
  `search(query)` → `pc_hit[]` (page + quads).
  No engine `fz_search` call outside
  `src/backends`.
- [ ] Budget: `pc_budget` read in `search.cc`
  for `max_highlights`; evict oldest highlight
  if over budget — `ctest -R search` headless.
- [ ] Perf: `bench-measure.sh --search "lorem"
  --corpus tests/bench/corpus/corpus-1000p.pdf` —
  `p50/p99 <100ms` + `peak_rss_kib ≤250MB`
  pasted, corpus `sha256sum` pasted (the
  regenerated text corpus).
- [ ] Serialize: highlights in sidecar
  `highlights` array via `pc_txn_to_json`
  round-trip `diff 0`.
- [ ] Corpus: regenerate with one text line per
  page; `test_corpus_contract.cc` sha updated;
  viewer bench re-run once on the new corpus —
  R30.2/R15.1 re-verified, pasted.
- [ ] Gate: `check.sh` green; diff ≤350.

## 6.4 Annotations as undoable txn

**Allowlist:** `src/core/annot/*`,
`tests/unit/test_annot.cc` (new),
`src/features/annot/SPEC.md` (new),
`include/pdfcore/transaction.h` (`pc_command`
v2 in place — decision 3),
`src/core/doc/transaction.{h,cc}` (apply/undo/
redo/hash/JSON for the v2 fields),
`src/core/json/canonical.cc` (if the canonical
shape grows), `src/cli/txn.cc` (replay of v2),
`tests/unit/test_txn_*.cc` (extend for v2)
**Denylist:** `src/render/*` (no IR mutation)

- [ ] `src/core/annot/annot.cc` —
  `pc_command` variants `ANNOT_ADD/EDIT/DELETE`
  with `quad + color + page`; `pc_txn_undo`
  restores — `test_annot.cc` 5/5 + hash
  pasted.
- [ ] Sidecar: `pc_txn_to_json/from_json`
  round-trip + `sidecar-fmt.py` `diff 0` —
  `sha256sum` both pasted.
- [ ] `grep -rn "fz_try" src/core/annot`
  0 outside `bridge` pasted.
- [ ] Gate: `check.sh` green; diff ≤350.

## 6.5 Re-anchoring + verdict

**Allowlist:** `src/core/annot/reanchor.cc`,
`tests/unit/test_reanchor.cc` (new),
`docs/sidecar-anchoring.md` (new),
`docs/lessons.md` (verdict entry, same PR),
`adr/*` (if kill)
**Denylist:** keep annot code only if keep

- [ ] `reanchor.cc` — quads re-anchor after
  text insert/delete before annot via IR
  offsets (not page xy) — `test_reanchor.cc`
  headless pasted.
- [ ] Verdict: (a) 5 numbers table +
  interaction kept; (b) kill — new ADR
  revisits D-1 with table, annot code deleted
  in same PR, `kickoff §12` amended.
- [ ] `docs/lessons.md` updated in same PR.
- [ ] Gate: `check.sh` green on last commit.

## 6.x Final epic gates

- [ ] `sh tools/check.sh` 14/14 on clean clone
  and PR head
- [ ] `gates-selftest 13/13`
- [ ] `ctest --preset linux-core` 0 failed,
  contract both backends, selection/search/annot
- [ ] `docs-check` 0, `lang-check` 0, `naming 0`,
  `canonical 0`
- [ ] `spec-check` 0 orphans, 0 pending
- [ ] `layering-check --strict` ≤0.07
- [ ] `grep windows.h src/core` 0,
  `grep fz_try | grep -v bridge` 0

**Checklist (for `epic-6-dod.md` §6.6):**

- [ ] 6.1: hit-test headless + SPEC
- [ ] 6.2: caret ABNT2 + input wiring
- [ ] 6.3: search <100ms + RSS 250MB
- [ ] 6.4: annot txn undo/redo + diff 0
- [ ] 6.5: re-anchor + verdict
- [ ] Final gates green on `main`

---

*Next: do 6.1 first — geometry not trusted.*
