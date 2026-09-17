# Epic 1 – Definition of Done (DoD) [grounded]

**Rule zero — zero-from-memory:** Every number, sha or count in this document must be pasted from
a command output included in this document. If you cannot paste the command that produced it, it
is a hypothesis and must be labelled as such. This epic exists because of that rule: the tree we
believed was published was not the tree that was published (see section 1.0).

Nothing in this epic is done. Every box below is `[ ]` because no story has been executed, and
`[x]` without pasted evidence in the same document is forbidden by `AGENTS.md` Critical Rule 4,
"Never claim a check passed without running it", which also bars marking a requirement done
without the artefact its `Verification:` line names.

## 1. Mandatory evidence (real outputs pasted)

### 1.0 Baseline — the state this epic starts from (measured 2026-09-17, planning pass, not
execution)

```
$ curl -s "https://api.github.com/repos/daniel-castilho/tyny-pdf/git/trees/main?recursive=1" | ...
local: 42 | remoto: 24

--- AUSENTES no remoto (18) ---
   .editorconfig
   .gitattributes
   .githooks/pre-commit
   .github/PULL_REQUEST_TEMPLATE.md
   .github/workflows/gates.yml
   adr/0001-engineering-canon.md
   adr/0002-repository-layout.md
   adr/0003-error-model.md
   adr/0004-dependency-management-and-supply-chain.md
   adr/0005-repository-language-is-english.md
   adr/0006-product-name-and-identifier.md
   adr/0007-sidecar-format.md
   adr/0008-product-name-tyny-pdf.md
   adr/0009-git-workflow.md
   adr/0010-build-environment-wsl2.md
   adr/0011-modularity-rules.md
   src/features/sidecar/SPEC.md
   tests/fixtures/sidecar/example.tynypdf.json
--- extras no remoto --- nenhum

$ curl -s "https://api.github.com/repos/daniel-castilho/tyny-pdf" | ...
"private": False | "default_branch": "main" | "pushed_at": "2026-09-17T00:54:57Z" | "license": "AGPL-3.0"
$ curl -s "https://api.github.com/repos/daniel-castilho/tyny-pdf/actions/runs" | ...
"total_count": 0
$ curl -s "https://api.github.com/repos/daniel-castilho/tyny-pdf/pulls?state=all"
[]
$ curl -s -o /dev/null -w '%{http_code}\n' "https://api.github.com/repos/daniel-castilho/tyny-pdf/branches/main/protection"
401
```

The published tree, checked with the tools it does contain (`codeload` tarball of `main`, 24 files):

```
$ sh tools/check.sh; echo "exit=$?"
== 4/7 sidecar format (ADR-0007)
tests/fixtures/sidecar: not a sidecar by name, skipped (see docs/naming.md)
sidecar-fmt: OK (0 checked, 1 skipped, 0 problems)
== 5/7 byte stability (.gitattributes, .editorconfig)
.gitignore: no final newline
canonical-check: 1 problem(s) in 23 file(s)
exit=1

$ python3 tools/docs-check.py | grep -c '^docs-check:'
29
$ python3 tools/spec-check.py | grep -cE 'orphan|defined nowhere'
11
$ od -c .gitignore | tail -2          # remote copy ends mid-content, no final newline
0000120   /
0000121
$ od -c /home/user/kickoff/.gitignore | tail -2
0000540   *   .   p   1   2  \n   *   .   k   e   y  \n
```

Same commands in the workspace seed: 42 files, `check: all gates green`, `docs-check: OK (24
markdown files, 0 problems)`, `spec-check` still listing 10 pending requirement ids, exit 0. The
gap between those two blocks is the epic's starting line. After this epic's five documents were
added to that seed, the same commands print: `find . -type f | wc -l` -> `47`, and `docs-check:
OK (29 markdown files, 0 problems)` - measured 2026-09-17, and every count in this section names
the command that produced it. The `--- AUSENTES no remoto` header is a label the ad-hoc diff
script printed, not a gate's output; the block is kept verbatim, and the repository's own files
stay English (ADR-0005).

### 1.1 `main` parity, first green CI run, protection, gate that bites

```
# paste: git ls-tree -r --name-only main | wc -l   and   wc -c on the five truncated files
# paste: sh tools/check.sh in a fresh clone of main (exit code visible)
# paste: GET /repos/daniel-castilho/tyny-pdf/actions/runs/<id>/jobs (job name, conclusion, head sha)
# paste: GET /repos/daniel-castilho/tyny-pdf/branches/main/protection (JSON)
# paste: the red log from the deliberately broken fixture, then the green log after revert
# paste: the rejection text of a non-admin push to main
```

### 1.2 Build system, both toolchains, ADR-0010 probes

```
# paste: cmake --preset linux-core && cmake --build --preset linux-core && ctest --preset linux-core
# paste: cmake --preset win-cross-x64 ... && file build/win-cross-x64/Release/tynypdf.exe
# paste: objdump -p tynypdf.exe (LLVM-MinGW) and dumpbin /dependents tynypdf.exe (MSVC)
# paste: the two nm -C --defined-only outputs and their diff (empty diff, pasted anyway)
# paste: gpu_probe stdout (adapter LUID + D3D_FEATURE_LEVEL)
# paste: the amended ADR-0010 lines where an assumption became a result
```

### 1.3 Engine pinned, four gates with teeth

```
# paste: git submodule status (40-hex sha) and third_party/UPSTREAM.toml
# paste: sh tools/deps-refresh.sh twice and git status --porcelain after the second run
# paste: sh tools/patch-report.sh and ls third_party/patches/*.patch | wc -l
# paste: sh tools/layering-check.sh with the ratio, then the red output from the injected engine
#        include in src/core/
# paste: sh tools/sbom.sh output and the schema validation result
```

### 1.4 Page 1 renders twice

```
# paste: ctest -R 'contract' output, both backends
# paste: the red output of the renumbered-enum experiment
# paste: ctest -R 'cli_exit_codes' output (0/1/2/3, four cases)
# paste: sha256 of the pixel buffer from the Linux CLI run and from the Win32 capture - they must match
```

### 1.5 Baseline measured on both channels

```
# paste: sh tools/bench-measure.sh --reference output, run 1 and run 2, with the machine spec
# paste: the per-metric delta table, computed by the harness, not by hand
# paste: git -C tests/conformance rev-parse HEAD (40-hex) and git ls-tree HEAD tests/conformance
```

### Epic gates (to be executed)

```
# paste: sh tools/check.sh on the merge commit of each of 1.1-1.5, exit code visible
# paste: the five CI run ids with their head shas
```

## 2. Self-audit — run BEFORE every hand-off

- [ ] Each sha resolves: every sha cited above appears in a pasted `git log`, `submodule status`,
      `rev-parse` or API output in this document
- [ ] Each (run number, sha) pair appears identical in the pasted `actions/runs` or job output —
  never one from the API and one from memory
- [ ] Each count matches a pasted output, unrounded: `wc -l`, `grep -c`, `bytes`. A count I did
  not see printed is deleted from this document
- [ ] Tree parity is proven against `main` (`git ls-tree`, the tree API), never against the
  workspace that produced it — that substitution is what made this epic necessary
- [ ] Every red item is ON the table: which story failed first, on which command, with the error
  line, and what changed
- [ ] Owner decisions are cited as messages from the decision log (`analysis/decision-log.md`,
  D-ids and the date), not paraphrased into agreement
- [ ] Every claim about `main` is true of `main` at the time of writing, including the file count
  and whether CI has run — remote state ages within a single session, so re-measure before you
  paste
- [ ] No closure claims: this document reports state plus evidence; "epic complete" is not a
  sentence it is allowed to write
- [ ] Docs are in English and ASCII-only apart from the allowlist (ADR-0005,
  `tools/lang-check.py`); the Portuguese labels in section 1.0 are inside a pasted output block
  and go away when the block is replaced by a real run
- [ ] Every `tools/...` path mentioned in `docs/epics/` either exists or carries "PR
  #n"/"planned" in the same paragraph (`tools/docs-check.py` enforces it; this repo has been
  caught three times writing a PR number that the plan does not contain)

## 3. Standing definitions

- **Pair** = (test, run number, sha). An id alone rots; a number alone drifts; both come from the
  API output pasted above.
- **Evidence** = pasted command output, with the command line itself above it. Memory =
  hypothesis, and hypotheses are labelled.
- **Owner sanction** = a cited message in `analysis/decision-log.md` (a D-id, a date, the answer).
  Nothing else is attribution; inventing attribution is worse than having none.
- **WORKSPACE prefix** = true in the working copy, not on `main`. Never upgrade WORKSPACE to landed;
  the seed in this epic was exactly that mistake.
- **Green** = exit code 0 printed after the command, in this document, on a clean clone. A job that
  never reached a check is not green (see `docs/release-runbook.md` section 1 on required checks).

## 4. Failures this document encodes (the E1 ledger — why each rule exists)

| Rule | The failure that kills |
|---|---|
| §1.0 tree parity | A push that moved 24 of 42 files, dropping every dotfile and every ADR, while the README said the seed was complete - `docs/kickoff.md` section 11 predicted this exact copy failure |
| §2 "true of main" | `README.md` still claiming "the remote repository currently holds one placeholder commit" after two commits existed; a stale front page is a false front page |
| §2 unrounded counts | Reporting "35 files" when the tool printed 34 (this repo, 2026-09-16, recorded in `docs/lessons.md`) |
| §2 PR attribution | Writing "the build system lands in PR #5" when the committed `gates.yml` header says PR #2 owns `CMakeLists.txt`; a committed artefact beats my reading of the plan |
| §1 evidence-only | `SPEC.md` describing CI as if it ran (`docs/lessons.md`, "a roadmap asserted a capability that belonged to a binding") |
| §1 vacuous pass | `sidecar-fmt: OK (0 checked, 1 skipped)` read as a pass while the fixture was missing; "0 matches" in a non-existent directory is never a pass |
| §2 no-closure | Declaring M0 finished because the documents look coherent; `docs/lessons.md` (2026-09-16) on an agent marking a task verified without writing a test |
| §1 baseline dates | Citing SumatraPDF 3.6.1 only, when the pre-release that users will install is 3.7 (D12: the "nobody else does it" premise checked against the wrong version) |

## 5. Epic 1 completion checklist

- [ ] 1.1: `main` has all 42 paths with matching sizes, clean-clone `sh tools/check.sh` exit 0,
  first green `gates` run pasted, six protection settings pasted, gate proven to bite, non-admin
  push rejected
- [ ] 1.2: presets + CMake + style configs build on GCC/Clang and LLVM-MinGW and MSVC; ADR-0010's
  four unverified items retired with output or amended with a downgrade
- [ ] 1.3: MuPDF pinned at a 40-hex sha, patch series real, four new gates green with a demonstrated
      red case each, `NOTICE` and `docs/references.md` present
- [ ] 1.4: `pdfcore` M0 surface + `null`/`mupdf` backends + contract suite + four CLI exit codes +
      identical page-1 pixel hash on Linux and Win32
- [ ] 1.5: `tests/baseline.json` with 3.6.1 and 3.7 pre-release numbers, two runs within 10 %,
  corpus submodule pinned at a sha, intake plan written
- [ ] Every `[x]` in `epic-1-technical-tasks.md` has its evidence in this file
- [ ] Self-audit above run, output pasted

---

*Epic 1 complete means: `main` is green on its own gate, the toolchain assumptions are results,
the engine is pinned, page 1 renders twice, and the bar is measured. This document must be
included in every PR and merge hand-off related to Epic 1; a hand-off without pasted evidence
returns to its author.*
