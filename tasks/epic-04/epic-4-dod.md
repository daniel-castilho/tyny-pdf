# Epic 4 – Definition of Done (DoD) [grounded]

**Rule zero:** Every number, sha or count must be pasted
from command output in this doc. If you cannot paste the
command that produced it, it is hypothesis and must be
labelled. `[x]` without pasted evidence is forbidden by
`AGENTS.md` R4.

Nothing is done. Every box is `[ ]` because no story has
been executed.

---

## 1. Mandatory evidence

This section is the only valid evidence. Filled during
execution, not planning, with command above pasted output.

- Paste `git rev-parse HEAD`, `git status --porcelain`,
  `git ls-tree -r --name-only HEAD | wc -l` at start of
  each story section — clean tree shown, not asserted.
- Paste `sh tools/check.sh` full output (14 lines) for
  merge commit on **clean clone**, not only workspace.
- Paste `python3 tools/spec-check.py` before and after
  each story — pending must move only when artefact
  exists.
- Paste `sh tools/layering-check.sh --strict` after every
  `src/core` story — ratio as number.
- Paste one failing gate: `grep` layering violation and
  `test_txn_abi` renumber red + green-after-revert.

---

## 2. Self-audit — run BEFORE every hand-off

- [ ] Each count matches pasted output: `wc -l`,
  `grep -c`, `bytes`. Not seen = deleted, not adjusted
- [ ] Each file touched in allowlist, not deny — `git
  show --stat HEAD` pasted and checked vs
  `epic-4-technical-tasks.md`
- [ ] Every `R<n>.<m>` closed has `Verification:` artefact
  pasted and `test -f` pasted
- [ ] No `SPEC.md` hand-edited to drop pending — `git
  diff HEAD~1 -- src/features/*/SPEC.md` pasted shows
  no wording change hiding requirement
- [ ] No new dep/flag/toolchain without ADR /
  `docs/dependency-policy.md` row — `grep` delta pasted
- [ ] Layering: `grep -rEn
  '#include.*windows|mupdf|fitz' src/core` 0 pasted
  after each core story
- [ ] Engine isolate:
  `grep -rn "fz_try" src | grep -v bridge` 0 pasted
  after 4.5
- [ ] Ratio: `layering-check --strict` `≤0.07` pasted
- [ ] Docs English ASCII allowlist, file counts in §4.0
  re-measured after `tasks/epic-04/` +5
- [ ] This audit run before hand-off — output pasted at
  §4.6
- [ ] **Tests vs Gates matrix** pasted: `ctest` green
  != done; `check.sh 14/14` + `gates-selftest 13/13`
  + `spec-check 0 errors` + `pending 4` + `layering
  ≤0.07` + `ctest 0 failed` all pasted. Missing one =
  not done. (Anti-pattern from Epic 3.)
- [ ] **Pending vs Error taxonomy** pasted:
  `pending` (e.g., R15.x) ok, `has no SPEC.md` /
  `does not cite id` = blocker, not pending. Never
  claim "pre-existing" without `git stash` proof that
  fails on base tree.
- [ ] **Clean clone, not workspace** — `git clone` +
  `check.sh` pasted for final gates §4.6.

## 3. Story evidence (paste during execution)

### 4.0 Baseline — state Story 4.1 starts from

Measured on `main @ 9563263` (the commit the epic-04 docs
were committed from), pasted 2026-09-21:

```bash
$ git rev-parse HEAD
95632638355640271870116a5178ddb79f681061

$ git status --porcelain
(untracked tasks/epic-04/ only, before the docs PR)
$ echo $?
0

$ git ls-tree -r --name-only HEAD | wc -l
7713

$ sh tools/check.sh 2>&1
== 1/14 language ... lang-check: OK (4680 files)
== 2/14 language self-test ... OK
== 3/14 naming drift ... naming-sync: OK (34 keys, 2 generated files, 1 retired tokens guarded)
== 4/14 sidecar format ... sidecar-fmt: OK (1 checked, 0 skipped, 0 problems) + self-test OK
== 5/14 byte stability ... canonical-check: OK (109 text files, 0 canonical problems)
== 6/14 living specs ... spec-check: OK (12 specs, 41 requirements, 15 source files, 0 orphans)
== 7/14 C and C++ style ... format-check self-test: OK
== 8/14 diff and tree hygiene ... diff-scan: OK (tree tyny-pdf, D1-D8 quiet)
== 9/14 diff-scan self-test ... OK
== 10/14 architecture layering ... layering-check: OK (27 source files, 0 violations) + self-test OK
== 11/14 supply chain: SBOM ... SBOM written to /tmp/sbom.json
== 12/14 supply chain: dependency refresh ... all steps completed successfully
== 13/14 supply chain: patch report ... Summary: 0 stale patch(es)
== 14/14 documentation ... docs-check: OK (168 markdown files, 0 problems)
check: all gates green  (exit 0)

$ sh tools/gates-selftest.sh 2>&1 | tail -1
gates-selftest: OK (13/13 suites hold)

$ python3 tools/spec-check.py 2>&1
spec-check: OK (12 specs, 41 requirements, 15 source files, 0 orphans)
spec-check: 4 requirement(s) still pending (no artefact yet): R15.1, R15.2, R15.3, R15.4

$ sh tools/layering-check.sh --strict 2>&1
layering-check: backend_line_ratio=0.0686
layering-check: OK (27 source files, 0 violations)

$ ctest --preset linux-core --output-on-failure 2>&1 | tail -3
15/15 Test #15: test_backend_contract_threaded ...   Passed    0.45 sec
100% tests passed, 0 tests failed out of 15
Total Test time (real) =   1.00 sec

$ find src/core \( -name "*.cc" -o -name "*.h" \) | xargs wc -l | tail -1
 1090 total
```

Tree count after the docs PR below: `git ls-tree` on the docs
merge = 7713 + 5 = 7718, re-measured at §4.6.

### 4.1 Transaction log core — undo/redo over IR

Pasted from branch `feat/4.1-txn-core`, working tree staged at
commit before PR (2026-09-21):

```bash
$ git show --stat HEAD   # (merge of PR #44)
# staged diff (git diff --cached --stat):
#  include/pdfcore/transaction.h |  78 +
#  src/core/CMakeLists.txt       |   3 +-
#  src/core/doc/CMakeLists.txt   |   5 +
#  src/core/doc/SPEC.md          |  25 +-
#  src/core/doc/doc.cc           |  15 +-
#  src/core/doc/transaction.cc   | 267 +
#  src/core/doc/transaction.h    |  31 +
#  tests/CMakeLists.txt          |   7 +
#  tests/unit/test_txn_core.cc   | 238 +
#  9 files changed, 667 insertions(+), 10 deletions(-)
# -> allowlist matches; denylist (src/backends/*, src/render/*, src/os/*) untouched.
# DIFF CEILING NOTE: planned <=350; measured 667 (add+del). Overage is the annotation IR
# substrate {id, rect} (owner-approved design) plus dense unit coverage. Flagged to owner
# in PR #44; not silently accepted.

$ grep -rEn '#include.*windows|mupdf|fitz' src/core
# -> 0 matches (exit 1)

$ sh tools/layering-check.sh --strict 2>&1
layering-check: backend_line_ratio=0.0601
layering-check: OK (30 source files, 0 violations)

$ ctest --preset linux-core -R txn_core --output-on-failure 2>&1 | tail -4
    Start 4: test_txn_core
1/1 Test #4: test_txn_core ....................   Passed    0.01 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.01 sec
# test binary itself: Passed: 52, Failed: 0  (hash equality + budget ceiling + error paths)

$ python3 tools/spec-check.py 2>&1
spec-check: OK (12 specs, 45 requirements, 17 source files, 0 orphans)
spec-check: 4 requirement(s) still pending (no artefact yet): R15.1, R15.2, R15.3, R15.4
# R18.1-R18.4 landed this story; pending count unchanged at the four R15.x (Epic 5).

$ sh tools/check.sh 2>&1 | tail -1
check: all gates green   (14/14, exit 0)
```

- [x] Txn core undo/redo + budget ceiling
- Evidence above, pasted from the working tree; a clean-clone
  rerun happens in §4.6.

### 4.2 Replay + CLI — byte-identical log

Pasted from branch `feat/4.2-replay` working tree over `1f2dbb1`
(2026-09-21), before PR #45:

```bash
$ git diff --cached --stat   # (16 files vs main 1f2dbb1)
#  CHANGELOG.md +8, include/pdfcore/json.h +68 (new),
#  include/pdfcore/transaction.h +10, src/cli/CMakeLists.txt +2,
#  src/cli/SPEC.md +7, src/cli/main.cc +7, src/cli/txn.cc +110 (new),
#  src/core/CMakeLists.txt +1, src/core/doc/SPEC.md +2,
#  src/core/doc/transaction.cc +304, src/core/json/CMakeLists.txt +4 (new),
#  src/core/json/SPEC.md +22 (new), src/core/json/canonical.cc +726 (new),
#  tasks/epic-04/epic-4-technical-tasks.md +11, tests/CMakeLists.txt +13,
#  tests/unit/test_txn_replay.cc +326 (new)
#  1610 insertions(+), 11 deletions(-) -> DIFF CEILING NOTE: planned <=300,
#  measured 1621 add+del. The owner-approved canonical JSON module
#  (src/core/json/canonical.cc, "extract to canonical.cc" decision) plus the
#  CLI-integrated test dominate the overage; flagged to owner in PR #45.
#  The sidecar writer was NOT rewired: its bytes are golden-frozen
#  (ADR-0007 trailing-comma format pinned by sidecar-fmt/golden tests);
#  rewiring writer.cc through canonical.cc needs its own PR with re-baselined
#  goldens.

$ ctest --preset linux-core --output-on-failure 2>&1 | tail -2
100% tests passed, 0 tests failed out of 17
Total Test time (real) = 1.23 sec

$ ctest --preset linux-core -R txn_replay -V 2>&1 | grep "Passed:"
5: Passed: 53, Failed: 0
# covers: 5-cmd round-trip (to_json -> from_json -> to_json byte-identical,
# IR hash equality, undo 5 -> redo 5 hash equality), unknown keys preserved
# ("future_flag", "keep-me"), from_json(to_json(x))==x over 20 generated logs,
# CLI replay output == core to_json bytes, CLI exit 0/2/1.

$ ./build/linux-core/Debug/tynypdf-cli txn replay /tmp/txn-empty.json --out /tmp/txn-out.json; echo $?
0
$ diff /tmp/txn-empty.json /tmp/txn-out.json; echo $?
0
$ ./build/linux-core/Debug/tynypdf-cli txn replay; echo $?
Usage: ./build/linux-core/Debug/tynypdf-cli txn replay <log.json> --out <out.json>
2
$ ./build/linux-core/Debug/tynypdf-cli txn replay /tmp/does-not-exist.json --out /tmp/txn-out.json; echo $?
Failed to open log file
1
$ printf '{not json' > /tmp/txn-bad.json && ./build/linux-core/Debug/tynypdf-cli txn replay /tmp/txn-bad.json --out /tmp/txn-out.json; echo $?
1

$ python3 tools/spec-check.py 2>&1
spec-check: OK (13 specs, 49 requirements, 19 source files, 0 orphans)
spec-check: 4 requirement(s) still pending (no artefact yet): R15.1, R15.2, R15.3, R15.4

$ sh tools/layering-check.sh --strict 2>&1
layering-check: backend_line_ratio=0.0429
layering-check: OK (33 source files, 0 violations)

$ sh tools/check.sh 2>&1 | tail -1
check: all gates green   (14/14, exit 0)
```

- [x] Replay byte-identical
- Evidence above; clean-clone rerun pasted at §4.6

Clean clone (the §4.6-style rerun, done pre-merge at the owner's
request — `git clone -b feat/4.2-replay` of the local repo to
`/tmp/tyny-epic4-42-clean`, submodule init, native MuPDF build):

```bash
$ git -C /tmp/tyny-epic4-42-clean rev-parse HEAD
b14712c7dec8afec6f727ff13f0bba4f1e04317d

$ sh tools/build-mupdf-linux.sh; echo $?
0   # libmupdf.a + third + threads built

$ cmake --preset linux-core && cmake --build --preset linux-core; echo $?
0   # [54/54] Linking CXX executable Debug/test_txn_replay

$ ctest --preset linux-core --output-on-failure 2>&1 | tail -2
100% tests passed, 0 tests failed out of 17
Total Test time (real) = 1.36 sec

$ ctest --preset linux-core -R txn_replay -V 2>&1 | grep "Passed:"
5: Passed: 53, Failed: 0

$ sh tools/check.sh; echo $?
check: all gates green   (14/14, exit 0)

$ python3 tools/spec-check.py 2>&1
spec-check: OK (13 specs, 49 requirements, 19 source files, 0 orphans)
spec-check: 4 requirement(s) still pending (no artefact yet): R15.1, R15.2, R15.3, R15.4

$ sh tools/layering-check.sh --strict 2>&1
layering-check: backend_line_ratio=0.0429
layering-check: OK (33 source files, 0 violations)

$ sh tools/gates-selftest.sh 2>&1 | tail -1
gates-selftest: OK (13/13 suites hold)
```

### 4.3 Font fallback per run — no tofu

```bash
$ git show --stat HEAD
# -> (paste)

$ ctest --preset linux-core -R text_fallback
# -> (paste face ids + missing glyph PC_ERR_LIMIT)

$ grep -rn "harfbuzz\|ICU" src/core; echo "exit:$?"
# -> (paste 0)

$ ldd build/linux-core/Debug/libpdfcore* | grep mupdf
# -> (paste 0)
```

- [ ] Fallback per run no tofu
- Evidence above

### 4.4 Break and caret pt-BR — golden

```bash
$ git show --stat HEAD
# -> (paste)

$ ctest --preset linux-core -R "break|caret"
# -> (paste golden diff 0)

$ diff -u tests/golden/text-break-positions.txt
# -> (paste 0 diff)

$ python3 tools/lang-check.py 2>&1 | tail -n 3
# -> (paste OK, fixtures allowlisted)
```

- [ ] Break/caret golden headless
- Evidence above

### 4.5 Freeze, golden and ratio — sane

```bash
$ git show --stat HEAD
# -> (paste)

# Golden bite proof — throwaway:
$ git checkout -b tmp/txn-renumber
$ # change enum
$ ctest -R txn_abi --output-on-failure 2>&1 | tail
# -> (paste FAILED)
$ git checkout main && git branch -D tmp/txn-renumber

$ grep -rn "fz_try" src | grep -v bridge; echo $?
# -> (paste 0)

$ sh tools/layering-check.sh --strict 2>&1
# -> layering-check: backend_line_ratio=0.06* (≤0.07)

$ grep -rEn '#include.*windows' src/core | wc -l
# -> 0

$ sh tools/check.sh 2>&1 | tail -n 5
$ gates-selftest 2>&1 | tail -n 3
$ ctest --preset linux-core 2>&1 | tail -n 5
# -> (paste all green)
```

- [ ] Freeze + golden + ratio ≤0.07
- Evidence above, on clean clone

### 4.6 Final epic gates (on merge of 4.5, clean clone)

```bash
$ git clone https://github.com/daniel-castilho/tyny-pdf.git
# /tmp/tyny-epic4-final && cd /tmp/tyny-epic4-final
$ git rev-parse HEAD
# -> (paste merge of 4.5)

$ sh tools/check.sh 2>&1; echo "exit:$?"
# -> (paste 14/14)

$ sh tools/gates-selftest.sh 2>&1; echo $?
# -> (paste 13/13)

$ ctest --preset linux-core --output-on-failure
# -> (paste 0 failed, 15+ incl threaded)

$ python3 tools/spec-check.py 2>&1
# -> spec-check: OK (14 specs, 48 reqs, 0 orphans)
# -> 4 pending: R15.1-R15.4 (Epic 5)

$ sh tools/layering-check.sh --strict 2>&1
# -> layering-check: backend_line_ratio=0.06* (≤0.07)

$ wc -l src/core/**/*.cc src/core/**/*.h | tail
# -> (~1990 core lines after epic)

$ git ls-tree -r --name-only HEAD | wc -l
# -> (paste base +5 docs + ~10 new files)
```

- [ ] Final gates green on `main`, pasted per gate

---

## 4. Failures this doc encodes (Epic 3 lesson → Epic 4 guard)

| Rule | Failure it kills | How caught |
|------|------------------|------------|
| §4.0 baseline | Ratio improve claimed without before number | Pasted 0.0686 before vs 0.06 after |
| §4.1 allowlist | AI adds `mupdf/fitz.h` to `src/core` | `grep` 0 pasted |
| §4.2 CLI vs core | AI tests replay only on core, not CLI | `diff` core json vs CLI out 0 |
| §4.3 tofu | AI renders tofu box instead of error | `PC_ERR_LIMIT` with U+ code pasted |
| §4.3 new dep | AI adds harfbuzz without ADR | `grep harfbuzz` 0 + `dependency-policy` row |
| §4.4 headless | AI tests caret only with window | `ctest -R caret` headless pasted |
| §4.5 `fz_try` | AI duplicates `fz_try` outside bridge | `grep fz_try \| grep -v bridge` 0 |
| §4.6 tests vs gates | `ctest` green = done (Epic 3 false done) | Matrix `check.sh + gates-selftest + spec-check + layering + ctest` all pasted |
| §4.6 pending vs error | `has no SPEC.md` claimed pre-existing | Taxonomy: pending ok, `has no SPEC.md` = blocker; `git stash` proof required |
| §4.6 clean clone | `check.sh` green in workspace but red on clone (generated/ not committed) | `git clone` + `check.sh` pasted |

## 5. Epic 4 completion checklist

- [x] 4.1: txn core undo/redo + budget ceiling
- [x] 4.2: replay + CLI byte-identical
- [ ] 4.3: fallback per run no tofu
- [ ] 4.4: break + caret pt-BR golden
- [ ] 4.5: freeze + golden + ratio ≤0.07 + gates green
- [ ] `check.sh` 14/14, `gates-selftest` 13/13,
  `ctest` 0 failed, `layering ≤0.07`,
  `spec-check` 0 orphans / 4 pending (R15.x),
  `docs-check` 0 over 5 new docs — all pasted in
  §4.6 on merge commit

---

*Epic 4 complete = semantics undoable and text correct,
both replayable on Linux. Every later delta reads this;
none rewrites it. Hand-off without pasted evidence
returns.*
