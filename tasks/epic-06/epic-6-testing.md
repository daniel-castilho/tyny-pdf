# Epic 6 – Testing Strategy [grounded]

Levels from `docs/testing-playbook.md` §2.
Everything names command and artefact.

> AI-Guard: each level has guard.

## Gates (every PR)

- Repo: `sh tools/check.sh` 14/14;
  `gates-selftest 13/13`. Guard: rewrap 11->14
  → `docs-check` catches.
- Style: `sh tools/format-check.sh`. Guard: engine
  include in `src/core` → `layering` red.
- Language: `python3 tools/lang-check.py` 0.
- Naming: `python3 tools/naming-sync.py check` 0.
- Spec: `python3 tools/spec-check.py` 0 orphans.

## 6.1 Selection hit-test

- **Goal:** headless CLI = window click.
- **Unit:** `test_selection.cc` `hit_test`
  with known quad, `ctest -R selection`.
- **Contract:** `tynypdf-cli select --rect`
  `sha256` vs window click equal.
- **Guard:** `grep windows.h src/core` 0;
  `grep fz_ src/render` 0.
- **Cmd:** `check.sh` + `ctest` + `layering`.

## 6.2 Caret + input

- **Goal:** drag + arrows + ABNT2 one step.
- **Unit:** `test_caret.cc` + `test_selection.cc`
  headless `e+U+0301` one logical caret.
- **A11y:** `docs/a11y/selection.md` exact text
  diffable.
- **Guard:** `present(0,0)` headless still works.
- **Cmd:** `ctest -R caret` + `check.sh`.

## 6.3 Search + highlights

- **Goal:** <100ms 1000 hits + RSS 250MB.
- **Perf:** `bench-measure.sh --search` `p50/p99`
  + `peak_rss_kib` forward/return, corpus `sha256`.
- **Budget:** `pc_budget` evicts oldest
  highlight if over max.
- **Guard:** no ICU without ADR row.
- **Cmd:** harness + `ctest -R search`.

## 6.4 Annotations txn

- **Goal:** undo/redo + sidecar diff 0.
- **Unit:** `test_annot.cc` 5 undo/5 redo +
  hash equality, `ctest -R annot`.
- **Round-trip:** `pc_txn_to_json/from_json`
  + `sidecar-fmt.py` `diff 0`, `sha256` both.
- **Guard:** `fz_try` only in `bridge`.
- **Cmd:** `check.sh` green.

## 6.5 Re-anchoring + verdict

- **Goal:** quads survive text edit + verdict.
- **Unit:** `test_reanchor.cc` insert before
  annot → quad offset moves, headless.
- **Verdict:** keep (5 numbers) or kill (ADR +
  table, code deleted).
- **Cmd:** `check.sh` green on last commit.

## Regression (per story + end)

- [ ] `check.sh` after every story, on clean clone
- [ ] `docs-check` 0 with 5 new epic docs
- [ ] `spec-check` 0 orphans, 0 pending
- [ ] `naming-sync` 0, `lang-check` 0
- [ ] `layering --strict` ≤0.07
- [ ] `ctest 0 failed`, contract both backends

**Checklist:**

- [ ] 6.1 hit-test headless
- [ ] 6.2 caret + input
- [ ] 6.3 search <100ms + RSS
- [ ] 6.4 annot txn
- [ ] 6.5 re-anchor + verdict
- [ ] Final gates green on `main`

---

*Run as each story executes, paste into `epic-6-dod.md`.*
