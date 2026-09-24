# Epic 6 – Definition of Done (DoD) [grounded]

**Rule zero:** Every number, sha or count must be pasted
from command output in this doc. `[x]` without pasted
evidence is forbidden by `AGENTS.md` R4.

Nothing is done. Every box is `[ ]`.

---

## 1. Mandatory evidence

Filled during execution, not planning, with command above
pasted output.

- Paste `git rev-parse HEAD`, `git status --porcelain`,
  `git ls-tree -r --name-only HEAD | wc -l` at start of
  each story — clean tree shown.
- Paste `sh tools/check.sh` full output (14 lines) for
  merge commit on **clean clone**, not only workspace.
- Paste `python3 tools/spec-check.py` before/after —
  pending moves only when artefact exists.
- Paste `sh tools/layering-check.sh --strict` after
  every `src/core`/`src/render` story — ratio as number.
- Paste one failing gate: `grep` layering violation and
  `search p99 >100ms` red proof.

---

## 2. Self-audit — run BEFORE every hand-off

- [ ] Each count matches pasted output: `wc -l`,
  `grep -c`, `bytes`.
- [ ] Each file in allowlist, not deny — `git show
  --stat HEAD` pasted vs `epic-6-technical-tasks.md`.
- [ ] Every `R<n>.<m>` closed has `Verification:`
  artefact pasted and `test -f` pasted.
- [ ] No `SPEC.md` hand-edited to drop pending — `git
  diff HEAD~1` pasted shows no hiding.
- [ ] No new dep without ADR/row — `grep` delta pasted.
- [ ] Layering: `grep -rEn
  '#include.*windows|mupdf|fitz' src/core` 0 pasted.
- [ ] Engine isolate: `grep -rn "fz_try" src
  | grep -v bridge` 0 pasted.
- [ ] Ratio: `layering-check --strict` `≤0.07` pasted.
- [ ] Docs English ASCII allowlist, file counts in
  §6.0 re-measured after `tasks/epic-06/` +5.
- [ ] This audit run before hand-off — pasted at §6.6.
- [ ] **Tests vs Gates matrix** pasted:
  `check.sh 14/14` + `gates-selftest 13/13` +
  `spec-check 0 errors` + `0 pending` + `layering
  ≤0.07` + `ctest 0 failed` all pasted. Missing one
  = not done.
- [ ] **Pending vs Error** pasted: `pending` ok,
  `has no SPEC.md` = blocker.
- [ ] **Clean clone, not workspace** — `git clone` +
  `check.sh` pasted for §6.6.

## 3. Story evidence (paste during execution)

### 6.0 Baseline — state Story 6.1 starts from

Executed on a clean tree at `main @ 5863926` (post story 1.5,
PRs #69/#70), before any epic 6 code:

```bash
$ git rev-parse HEAD
586392646de6e030e1c0fde9a8e92f5d337a7c18

$ git ls-tree -r --name-only HEAD | wc -l
7805

$ sh tools/check.sh 2>&1 | tail -n 14
== 14/14 documentation only promises what exists
docs-check: OK (190 markdown files, 0 problems)

check: all gates green

$ python3 tools/spec-check.py 2>&1
spec-check: OK (21 specs, 80 requirements, 42 source files, 0 orphans)
spec-check: 0 requirement(s) still pending (no artefact yet): -

$ sh tools/layering-check.sh --strict 2>&1
layering-check: backend_line_ratio=0.0254
layering-check: OK (61 source files, 0 violations)

$ ctest --preset linux-core 2>&1 | tail -n 3
100% tests passed, 0 tests failed out of 31

Total Test time (real) =   1.72 sec
```

`gates-selftest` same session: `gates-selftest: OK (13/13 suites hold)`.
Baseline noted for this epic: the committed corpus is rect-only
(no text streams), so 6.3 regenerates it with a text line per page;
the 1.5 bench rows in `tasks/epic-01/story-1.5-content-viewer.md`
stay historical (recorded, not re-baselined).

### 6.1 Selection hit-test + geometry reconcile

```bash
$ git show --stat HEAD
# -> (paste allowlist)

$ grep -rn "fitz\|fz_" src/render src/os; echo $?
# -> (paste 0)

$ ctest --preset linux-core -R selection 2>&1 | tail
# -> (paste headless)

$ tynypdf-cli select --page 0 --rect 10,10,100,20
# --out /tmp/sel.json && sha256sum /tmp/sel.json
# -> (paste CLI sha)

$ # window click same rect → /tmp/win.json
$ sha256sum /tmp/win.json
# -> (paste equal to CLI)

$ cat src/features/selection/SPEC.md | head -n 20
# -> (paste R32.1 with Verification:)

$ python3 tools/spec-check.py 2>&1 | grep pending
# -> (paste 0 pending after R32.1)
```

- [ ] hit-test headless + SPEC

### 6.2 Caret + input wiring

```bash
$ git show --stat HEAD
# -> (paste)

$ ctest --preset linux-core -R caret 2>&1 | tail
# -> (paste ABNT2 + combining)

$ cat docs/a11y/selection.md | head -n 20
# -> (paste script)

$ python3 tools/check.sh 2>&1 | tail -n 5
# -> (paste 14/14)
```

- [ ] caret + input

### 6.3 Search + highlight quads

```bash
$ git show --stat HEAD
# -> (paste)

$ ctest --preset linux-core -R search 2>&1 | tail
# -> (paste headless)

$ python3 tools/bench-measure.sh --search "lorem"
# --corpus tests/bench/corpus/corpus-1000p.pdf 2>&1 | grep -E "p50|p99|peak_rss"
# -> (paste p99 <100ms + peak ≤250MB; runs on the corpus REGENERATED with
#    text by this story - see the regeneration note in epic-6-technical-tasks.md)

$ sha256sum tests/bench/corpus/corpus-1000p.pdf
# -> (paste; differs from the 1.5 pin - test_corpus_contract.cc updated
#    in the same commit, argued in the diff)

$ python3 tools/check.sh 2>&1 | tail -n 5
# -> (paste 14/14)
```

- [ ] <100ms + RSS 250MB

### 6.4 Annotations as undoable txn

```bash
$ git show --stat HEAD
# -> (paste)

$ ctest --preset linux-core -R annot 2>&1 | tail
# -> (paste 5/5 undo/redo)

$ python3 tools/bench-measure.sh --help 2>&1 | head
# -> (paste)

$ sha256sum /tmp/txn.json /tmp/txn-round.json
# -> (paste both equal, diff 0)

$ grep -rn "fz_try" src/core/annot; echo $?
# -> (paste 0 outside bridge)
```

- [ ] annot txn

### 6.5 Re-anchoring + verdict

```bash
$ git show --stat HEAD
# -> (paste)

$ ctest --preset linux-core -R reanchor 2>&1 | tail
# -> (paste headless)

$ cat docs/sidecar-anchoring.md | head -n 30
# -> (paste ladder)

$ cat epic-6-dod.md | grep -A10 "Verdict:"
# -> (paste keep/kill with 5 numbers)

$ cat docs/lessons.md | tail -n 20
# -> (paste)
```

- [ ] re-anchor + verdict

### 6.6 Final epic gates (on merge of 6.5, clean clone)

```bash
$ git clone https://github.com/daniel-castilho/tyny-pdf.git
# /tmp/tyny-epic6-final && cd /tmp/tyny-epic6-final
$ git rev-parse HEAD
# -> (paste merge of 6.5)

$ sh tools/check.sh 2>&1; echo $?
# -> (paste 14/14)

$ sh tools/gates-selftest.sh 2>&1; echo $?
# -> (paste 13/13)

$ ctest --preset linux-core --output-on-failure
# -> (paste 0 failed, 31+ incl selection)

$ python3 tools/spec-check.py 2>&1
# -> (paste actual; the plan lands 3 new feature SPECs - selection,
#    search, annot - so ~24 specs, requirements per what the stories
#    genuinely need, 0 orphans, 0 pending)

$ sh tools/layering-check.sh --strict 2>&1
# -> layering-check: backend_line_ratio=0.02*
# (≤0.07)

$ wc -l src/core/selection/*.cc src/core/search/*.cc
# | tail
# -> (~400 selection + ~300 search)

$ git ls-tree -r --name-only HEAD | wc -l
# -> (paste base +5 docs + ~10 new files)
```

- [ ] Final gates green on `main`, pasted per gate

---

## 4. Failures this doc encodes

| Rule | Failure it kills |
|------|------------------|
| §6.0 baseline | Ratio claimed without before |
| §6.1 headless | Selection works in window but not CLI |
| §6.3 RSS | Only forward search, not return |
| §6.4 txn | Annot not undoable |
| §6.5 re-anchor | Quad at page xy, not IR offset |
| §6.6 clean clone | `check.sh` green in workspace but red on clone |

## 5. Epic 6 completion checklist

- [ ] 6.1: hit-test headless + SPEC
- [ ] 6.2: caret + input
- [ ] 6.3: <100ms + RSS 250MB
- [ ] 6.4: annot txn
- [ ] 6.5: re-anchor + verdict
- [ ] `check.sh` 14/14, `gates-selftest` 13/13,
  `ctest` 0 failed, `layering ≤0.07`,
  `spec-check` 0 orphans, `docs-check` 0 — all pasted
  in §6.6 on merge

---

*Epic 6 complete = selection is IR, search is budget-aware,
annot is txn, re-anchor is offset-based. Hand-off without
pasted evidence returns.*
