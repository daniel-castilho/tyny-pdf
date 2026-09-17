# Epic 2 – Definition of Done (DoD) [grounded]

**Rule zero — zero-from-memory:** every number, hash or count in this document must be pasted from
a command output included in this document. If you cannot paste the command that produced it, it
is a hypothesis and must be labelled as such. Epic 2 is the spike whose entire purpose is to
replace "should be fine" with a table; a DoD that breaks the rule would make the epic pointless.

Nothing in this epic is done. Every box below is `[ ]`: no story has been executed, and `[x]`
without pasted evidence in the same document is forbidden by `AGENTS.md` Critical Rule 4, "Never
claim a check passed without running it", which also bars marking a requirement done without the
artefact its `Verification:` line names.

This file also records what is **not** measurable yet, and why. A criterion that cannot be run is
not deleted, softened or re-scaled to something runnable; it is written here with the command that
shows it is blocked (`docs/kickoff.md` section 7, gate 3 is the only way a criterion changes).

## 1. Mandatory evidence (real outputs pasted)

### 2.0 Baseline - the state this epic starts from (measured 2026-09-17T16:33Z, in this
workspace, after `docs/epics/` gained this epic's five documents; the planning pass at 15:54Z saw
72 files, 579692 bytes and `OK (30 markdown files, 0 problems)`, and those older numbers are quoted
here only so the delta is visible)

```
$ find . -type f -not -path './build/*' | wc -l
77
$ find . -type f -not -path './build/*' -printf '%s\n' | awk '{s+=$1} END{print s}'
637488
$ sh tools/check.sh | tail -1
check: all gates green
$ sh tools/gates-selftest.sh | tail -1
gates-selftest: OK (12/12 suites hold)
$ python3 tools/docs-check.py | tail -1
docs-check: OK (35 markdown files, 0 problems)
$ python3 tools/spec-check.py | head -1
spec-check: OK (1 specs, 14 requirements, 0 source files, 0 orphans)
$ python3 tools/spec-check.py | tail -1
spec-check: 10 requirement(s) still pending (no artefact yet): R2.2, R2.3, R3.1, R3.2, R4.1, R4.2, R5.1, R5.2, R6.1, R6.2
$ python3 tools/lang-check.py | tail -1
lang-check: OK (70 files)
$ sh tools/canonical-check.sh | tail -1
canonical-check: OK (70 text files, 0 canonical problems)
$ sh tools/format-check.sh | tail -1
format-check: OK (4 file(s) match .clang-format)
$ python3 tools/naming-sync.py check | tail -1
naming-sync: OK (33 keys, 2 generated files, 1 retired tokens guarded)
```

What this tree can and cannot do for this epic, measured with the same tools:

```
$ python3 tools/layering-check.py; echo rc=$?
layering-check: backend_line_ratio=undefined (include/pdfcore/backend.h absent)
layering-check: OK (4 source files, 0 violations)
rc=0
$ python3 tools/layering-check.py --strict; echo rc=$?
layering-check: 0 violation(s), ratio over budget or undefined under --strict
rc=1

$ python3 tools/corpus-check.py --root .; echo rc=$?
corpus-check: NOT A PASS - tests/conformance/manifest.txt does not exist
rc=3
$ python3 tools/bench-measure.py --compare a b; echo rc=$?
bench-measure: a does not exist
rc=2
$ python3 tools/sbom.py --root .; echo rc=$?
sbom: 0 third-party components (conan.lock absent, third_party absent, build dir present);
     accurate for this tree, and not a coverage claim
rc=0
$ python3 tools/sbom.py --root . --strict >/dev/null 2>&1; echo rc=$?
rc=3
$ python3 tools/patch-report.py --dir third_party/patches; echo rc=$?
patch-report: NOT A PASS - third_party/patches does not exist, so there is no engine patch series
rc=3
$ python3 tools/deps-refresh.py --check-current; echo rc=$?
deps-refresh: no conanfile.py in the tree. PR #2 introduces it (docs/kickoff.md section 11)
rc=2

$ sh tools/win-probe/build.sh --self-test | tail -1
win-probe self-test: 5/5 properties hold
$ sh tools/win-probe/build.sh --probe d2d; echo rc=$?
win-probe: no LLVM-MinGW compiler on PATH. ...
rc=2
$ cmake --preset linux-core >/dev/null 2>&1; echo rc=$?
rc=0        (measured 2026-09-17T15:54Z with cmake and ninja from docs/dev-environment.md's
             one-time setup; this container has since lost both, so the line is history here and a
             re-run by whoever executes the epic)
$ cmake --list-presets | grep '"'
  "linux-core"    - Native core and CLI, sanitizers on
  "win-cross-x64" - Cross build with the pinned LLVM-MinGW
$ cd build/linux-core && ctest | tail -2
100% tests passed out of 2   (same caveat: build/ is not part of the workspace and was cleaned,
             so this output came from the earlier session, not from this container)
```

Read that block as the epic's starting conditions, not as a failure list:

- Story 1.2 of Epic 1 is in the tree: the build skeleton configures, `ctest` runs two real
  invariant tests, and the probes' own detection works. The cross preset refuses until the pinned
  toolchain is unpacked, which is the behaviour `tools/cmake/toolchain-llvm-mingw.cmake` was
  written to have. - Epic 1 stories 1.3 and 1.4 have not produced `third_party/`, `conanfile.py`,
  `conan.lock`, `NOTICE`, `include/pdfcore/`, `src/core/`, `src/backends/*`, `src/cli/`, `src/os/`
  or `src/render/`. Every criterion in this epic that needs pixels is therefore blocked on those,
  and `docs/kickoff.md` section 11 says M1 runs in parallel with PRs 3 and 4 - parallel, not ahead
  of the API. - Epic 1 story 1.5 (Sumatra baseline) is owner debt by the owner's decision, so M1
  row 3's relative half has no comparison to make. `tests/baseline.json` does not exist. - This
  workspace has no `.git`, so `main`, commit ids, run ids and branch protection are not verifiable
  from here. Anything in this epic that needs them must be pasted from where they exist.

### 2.1 The measurement spine

```
# paste: the four entry-condition commands from epic-2-overview.md, with their output
# paste: python3 tools/bench-measure.py --target tynypdf --binary ... --runs 2 --record-machine
# paste: python3 tools/bench-measure.py --compare run1.json run2.json (within tolerance, both runs)
# paste: the three refusals from epic-2-testing.md 2.1 (missing binary, non-PDF in corpus,
  cross-machine compare)
# paste: sh tools/win-probe/build.sh --probe gpu on the Windows session (adapter LUID + feature
  level)
```

- [ ] Entry conditions run and pasted, refusals included
- [ ] Two harness runs within 10 % per metric, machine block from the tool, both files committed
- [ ] One pasted refusal per negative control

### 2.2 The surface

```
# paste: the frame table behind "60 fps sustained, no frame over 33 ms p99", with the run length
# paste: python3 tools/layering-check.py --strict (exit 0, ratio printed as a number)
# paste: the two hashes - CLI PNG render of the region, and the window's presented bytes
# paste: python3 tools/format-check.sh and python3 tools/spec-check.py on the new files
```

- [ ] M1 row 1 answered with a table, not a sentence
- [ ] `src/features/render/SPEC.md` exists with its first file, and its requirements are not
  orphans
- [ ] CLI/window byte identity proven by hashes pasted side by side

### 2.3 Tiles and the budget

```
# paste: peak_rss_kib for the forward pass and the return pass, same run, same corpus
# paste: the corpus directory listing with sha256sum output
# paste: grep -n "free(" src/render src/os  -> expected: no engine memory freed here (R-M6)
# paste: the budget unit test failing with the ceiling shrunk, then passing after the fix
```

- [ ] `<= 250 MB` with 1000 pages open at three tiles each
- [ ] No growth on the return pass, with both numbers printed by one invocation
- [ ] Eviction decided and tested in `src/core/budget/`, not in the swapchain

### 2.4 Cold start

```
# paste: three startup pairs from the tynypdf.ui log line, same machine, cache state noted
# paste: python3 tools/corpus-check.py --root . (rc 3) and test -e tests/baseline.json (absent)
# paste: if Epic 1 story 1.5 landed meanwhile: bench-measure.py --compare spike.json
  baseline-derived
```

- [ ] Absolute `<= 300 ms` answered with three numbers and a machine block, or recorded as a miss
- [ ] Relative half recorded as open with the two outputs above; M1 row 3's wording unchanged

### 2.5 DPI, composition, caret, gesture

```
# paste: the approval pairs at 150% and 200% and their comparison result
# paste: gesture delta distribution (p50, p99, sample count) from the per-frame log lines
# paste: src/core/text caret tests run in linux-core, and the Windows job's IME case
```

- [ ] No bitmap stretch, shown by bytes rather than by a screenshot
- [ ] Gesture p99 at or under 16 ms with the sample count printed
- [ ] Caret arithmetic headless; composition asserted on Windows, not assumed from Linux

### 2.6 Narration and the verdict

```
# paste: the two docs/a11y/ scripts, with the exact announced text they expect, and the run output
# paste: the UIA assertions from the Windows job, or the reason they could not run
# paste: the keyboard traversal checklist, each row marked from the run
# paste: the verdict - either the seven-row table, or the new ADR with the frame table attached
```

- [ ] Row 5 answered with script files plus pasted announced text
- [ ] Verdict committed in one of the two shapes; if kill: new ADR under `adr/`, numbered at
  creation so `docs-check.py` indexes it, and the spike's rendering code deleted in the same PR
- [ ] `docs/lessons.md` updated in the same PR as the verdict, per `docs/kickoff.md` section 9
  step 6

## 2. Self-audit — run BEFORE every hand-off

- [ ] Each count matches a pasted output, unrounded: `wc -l`, `grep -c`, `bytes`. A count I did
  not see printed is deleted from this document, not adjusted
- [ ] Each timing number names the machine and the corpus, in the same block as the number; the
  machine block came from `--record-machine`, not from typing the CPU model
- [ ] Every criterion that was **not** measured carries the command that shows it is blocked; an
  unmeasured row with no command above it is a silent downgrade
- [ ] No criterion was rewritten to make it measurable. If `<= 300 ms ... (vs measured Sumatra)`
  is not reachable, that is a recorded open item, and the downgrade gate edits a `SPEC.md` `Out of
  scope` section in a PR - not this file's wording
- [ ] The verdict is not "promising": either the table is here or the ADR is
- [ ] Layering and format gates are pasted from the last commit of each story, not from an earlier
  one
- [ ] Docs are in English and ASCII-only apart from the allowlist (ADR-0005,
  `tools/lang-check.py`), and the file counts in section 2.0 were re-measured after `docs/epics/`
  gained five more files
- [ ] No closure claims: this document reports state plus evidence; "epic complete" is not a
  sentence it is allowed to write

## 3. Standing definitions

- **Measured** = a number printed by a named command, in this document, on the machine the number
  claims to describe. Anything else is an estimate and is labelled estimate.
- **Evidence** = pasted command output, with the command line itself above it. Memory = hypothesis.
- **Blocked** = a criterion whose command cannot run because an artefact does not exist, with the
  refusal pasted. It is not "deferred", not "n/a", and not satisfied by a weaker test.
- **Owner sanction** = a cited message in `analysis/decision-log.md` (a D-id, a date, the answer).
  Nothing else is attribution; inventing attribution is worse than having none.
- **Green** = exit code 0 printed after the command, in this document, on a clean clone. A job that
  never reached a check is not green.
- **Verdict** = one of the two shapes in section 2.6. "Spike ongoing" is not a verdict and does not
  close the epic.

## 4. Failures this document encodes (the E2 ledger — why each rule exists)

| Rule | The failure it kills |
|---|---|
| §2.0 measure the tree, not the plan | A status table claiming four of five stories complete in a tree with no `include/pdfcore/`, no `src/core/` and no `third_party/` (measured here, 2026-09-17: 1.3 and 1.4 have no artefacts) |
| §2.1 refusal before result | A harness trusted with a spike's numbers having never been shown refusing a bad input |
| §2.2 table, not adjective | "60 fps sustained" written down with no frame list, which is how a benchmark becomes a rumour (`docs/testing-playbook.md` names p99 for the same reason) |
| §2.3 two passes | Reporting the forward pass only, which cannot show growth when scrolling back - the second half of M1 row 2 is the whole point of the row |
| §2.4 blocked, not reworded | Redefining "vs measured Sumatra" as an absolute target because the baseline is owner debt; the downgrade gate exists so that happens in a `SPEC.md` in a PR |
| §2.5 bytes over screenshots | "no bitmap stretch" asserted from looking at a window at 200 % |
| §2.6 a verdict is committable | A spike that ends with the code kept "for now" after a negative result, which is how a rejected decision becomes permanent |
| §2 no-closure | Declaring M1 reached because the documents are coherent; `docs/lessons.md` (2026-09-16) records this exact mistake in the sibling project |
| §2 re-measured counts | A `wc -l` or file count copied from an earlier round; `docs/lessons.md` "A citation to a plan has to be read in the plan" is the same failure one step out |

## 5. Epic 2 completion checklist

- [ ] 2.1: entry conditions and three refusals pasted; two runs within tolerance, machine block
  from the tool, both JSON files committed
- [ ] 2.2: 4000x3000 at the frame budget with the frame table, CLI/window byte identity, render
  `SPEC.md` written with its first file, `--strict` layering exit 0
- [ ] 2.3: RSS ceiling met, no growth on the return pass, eviction policy in `src/core/budget`
  with a test that fails when the ceiling shrinks
- [ ] 2.4: absolute cold start with three numbers, relative half recorded open with its cause
- [ ] 2.5: DPI approval pairs, gesture p99 with sample count, caret headless, IME on the Windows
  job
- [ ] 2.6: two `docs/a11y/` scripts with exact announced text, keyboard traversal from the run,
  and a verdict committed as numbers or as a new ADR
- [ ] `sh tools/check.sh`, `sh tools/gates-selftest.sh`, `ctest --preset linux-core`, `python3
  tools/layering-check.py --strict` and `python3 tools/spec-check.py` green on the last commit,
  outputs pasted
- [ ] This self-audit run before any hand-off, with its output pasted

---

*Epic 2 complete means: the seven M1 criteria each have a number or a pasted refusal, the layering
rules survived the spike, and the surface decision is written down as either a contract in
`src/features/render/SPEC.md` or an ADR that revisits D3 with the frame table. This document is
included in every PR and hand-off related to Epic 2; a hand-off without pasted evidence returns to
its author.*
