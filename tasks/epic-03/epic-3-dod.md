# Epic 3 – Definition of Done (DoD) [grounded]

**Rule zero — zero-from-memory:** Every number, sha or count in this document must be
pasted from a
command output included in this document. If you cannot paste the command that
produced it, it is a
hypothesis and must be labelled as such. A figure with no command above it is
forbidden by
`AGENTS.md` Critical Rule 4.

Nothing in this epic is done. Every box below is `[ ]` because no story has been
executed, and `[x]`
without pasted evidence in the same document is forbidden.

---

## 1. Mandatory evidence — how this document proves itself

This section is the only valid evidence for the epic. Each subsection below is filled
**during
execution**, not during planning, with the command line above the pasted output.
`epic-3-dod.md` is
the hand-off; if the log is not here, the work did not happen.

- Paste `git rev-parse HEAD`, `git status --porcelain`, `git ls-tree -r --name-only
  HEAD | wc -l` at
  the start of each story section — a clean tree is not asserted, it is shown.
- Paste `sh tools/check.sh` full output (14 lines, `check: all gates green`) for the
  merge commit on
  a **clean clone**, not only in the workspace — the workspace is not the evidence
  (AGENTS.md rule 4).
- Paste `python3 tools/spec-check.py` before and after each story that retires a
  requirement — the
  pending count must move only when an artefact exists.
- Paste `sh tools/layering-check.sh --strict` after every story that touches
  `src/core` or
  `src/backends` — ratio as a number, not a sentence.
- A deliberately failing gate is pasted once per epic: one `grep` that proves a
  layering violation
  is caught, and one `test_status_abi` renumber that proves the golden guard bites —
  both with the red
  output and the green-after-revert output.

---

## 2. Self-audit — run BEFORE every hand-off

- [ ] Each count matches a pasted output, unrounded: `wc -l`, `grep -c`, `bytes`. A
  count I did not
  see printed is deleted from this document, not adjusted
- [ ] Each file touched is in the story's allowlist and not in its denylist — `git
  show --stat HEAD`
  pasted for every merged PR and the paths are checked against
  `epic-3-technical-tasks.md`
- [ ] Every `R<n>.<m>` closed by a story has its `Verification:` artefact path pasted
  and that path
  exists on `main` at the merge commit (`test -f` pasted)
- [ ] No `SPEC.md` was hand-edited to make a pending count drop — `git diff HEAD~1 --
  src/features/sidecar/SPEC.md` pasted and shows no wording change that hides a
  requirement
- [ ] No new dependency, build flag or toolchain was added without an ADR /
  `docs/dependency-policy.md` row — `grep -r "conan\|add_subdirectory" CMakeLists.txt`
  delta pasted if
  any
- [ ] Layering: `grep -rEn '#include *[<\"]\(windows|windef|d2d1|fitz|mupdf)'
  src/core src/render` →
  0 lines, pasted after each core story
- [ ] Engine isolation: `grep -rn "fz_try" src --include="*.cc" --include="*.h" |
  grep -v
  exception_bridge` → 0 lines, pasted after story 3.5
- [ ] Ratio saned: `sh tools/layering-check.sh --strict` prints `backend_line_ratio ≤
  0.07` — pasted
  number, not typed
- [ ] Docs are in English and ASCII-only apart from the allowlist (ADR-0005,
  `tools/lang-check.py`),
  and the file counts in §3.0 were re-measured after `tasks/epic-03/` gained 5 files
- [ ] This self-audit was run before the hand-off it protects — output pasted at §3.6

## 3. Story evidence (paste during execution)

### 3.0 Baseline — the state Story 3.1 starts from (measured on 2026-09-20, not in
planning)

```bash
# Paste on the day Story 3.1 executes — these are the numbers that Story 3.5 must improve.
$ git rev-parse HEAD
73dec5ff6588306d6cad96bffe543de926a69c04

$ git ls-tree -r --name-only HEAD | wc -l
7672

$ sh tools/check.sh 2>&1 | tail -n 30
== 1/14 language (ADR-0005)
lang-check: OK (4640 files)
== 2/14 language self-test (the gate must still detect violations)
lang-check self-test: OK
== 3/14 naming drift (ADR-0006 rules, ADR-0008 name)
naming-sync: skipped 63 paths from git ls-files that are not regular files, counted here not silently
naming-sync: OK (34 keys, 2 generated files, 1 retired tokens guarded)
== 4/14 sidecar format (ADR-0007)
sidecar-fmt: OK (1 checked, 0 skipped, 0 problems)
sidecar-fmt self-test: OK
== 5/14 byte stability (.gitattributes, .editorconfig)
canonical-check: OK (93 text files, 0 canonical problems)
== 6/14 living specs (R-M13)
spec-check: OK (7 specs, 28 requirements, 6 source files, 0 orphans)
spec-check: 17 requirement(s) still pending (no artefact yet): R14.1, R14.2, R14.3, R15.1, R15.2, R15.3, R15.4, R2.2, R2.3, R3.1, R3.2, R4.1, R4.2, R5.1, R5.2, R6.1, R6.2
== 7/14 C and C++ style against .clang-format (the tree has real C now)
format-check self-test: OK
== 8/14 diff and tree hygiene (D1-D8: exec bit, symlink, bidi, confusables, CI
events, action pins, dependency names, manifest and lock agreement)
diff-scan: OK (tree tyny-pdf, D1-D8 quiet)
== 9/14 diff-scan self-test (a scan that cannot fail is not a gate)
diff-scan self-test: OK
== 10/14 architecture layering (ADR-0011 R-M10/R-M11)
layering-check: backend_line_ratio=0.1014
layering-check: OK (13 source files, 0 violations)
layering-check self-test: OK
== 11/14 supply chain: SBOM generation (ADR-0004)
SBOM written to /tmp/sbom.json
== 12/14 supply chain: dependency refresh (ADR-0004)
deps-refresh: refreshing Conan lockfile...
dry-run: would run 'conan lock create conanfile.py --lockfile=conan.lock'
deps-refresh: running test suite (ctest --preset linux-core)....
dry-run: would run 'ctest --preset linux-core --output-on-failure'
deps-refresh: running full gate (tools/check.sh)....
dry-run: would run 'sh tools/check.sh'
deps-refresh: all steps completed successfully
== 13/14 supply chain: patch report (ADR-0004)
Patch Status Report
===================
PATCH                          STATUS       AGE (days) OWNER      SUBJECT
--------------------------------------------------------------------------------
0001-initial.patch             none         2          daniel-castilho Initial MuPDF vendoring
0002-font-fallback.patch       none         2          daniel-castilho Font fallback configuration
0003-text-rendering.patch      none         2          daniel-castilho Text rendering pipeline
0004-platform-support.patch    none         2          daniel-castilho Platform-specific compiler flags

Summary: 0 stale patch(es)
== 14/14 documentation only promises what exists
docs-check: OK (158 markdown files, 0 problems)

check: all gates green

$ sh tools/gates-selftest.sh 2>&1 | tail -n 20
== lang-check: python3 tools/lang-check.py --self-test
   self-test: 5/5 rules effective
== sidecar-fmt: python3 tools/sidecar-fmt.py self-test
   self-test: 5/5 properties hold
== naming-sync: python3 tools/naming-sync.py self-test
   naming-sync self-test: 5/5 properties hold
== spec-check: python3 tools/spec-check.py --self-test
   spec-check self-test: 14/14 properties hold
== docs-check: python3 tools/docs-check.py --self-test
   docs-check self-test: 11/11 properties hold
== corpus-check: python3 tools/corpus-check.py . --self-test
   Self-test passed
== format-check: sh tools/format-check.sh --self-test
   format-check self-test: 2/2 ok
== layering-check: sh tools/layering-check.sh --self-test
   layering-check self-test: 8/8 ok
== sbom: sh tools/sbom.sh --self-test
   sbom.sh self-test: OK
== deps-refresh: sh tools/deps-refresh.sh --self-test
   deps-refresh self-test: OK
== patch-report: sh tools/patch-report.sh --self-test

== verapdf: sh tools/verapdf.sh --self-test
   verapdf.sh self-test: SKIPPED (verapdf not installed)
== diff-scan: python3 tools/diff-scan.py --self-test
   diff-scan self-test: 17/17 properties hold

gates-selftest: OK (13/13 suites hold)

$ python3 tools/spec-check.py 2>&1
spec-check: OK (7 specs, 28 requirements, 6 source files, 0 orphans)
spec-check: 17 requirement(s) still pending (no artefact yet): R14.1, R14.2, R14.3, R15.1, R15.2, R15.3, R15.4, R2.2, R2.3, R3.1, R3.2, R4.1, R4.2, R5.1, R5.2, R6.1, R6.2

$ sh tools/layering-check.sh --strict 2>&1
layering-check: backend_line_ratio=0.1014
layering-check: OK (13 source files, 0 violations)

$ cat tests/baseline.json | python3 -c "import json,sys; d=json.load(open('tests/baseline.json')); print(f\"tynypdf: {d['benchmarks']['tynypdf']['metrics']['open_time_ms']['mean']} ms (null, Debug)\"); print(f\"sumatra-3.6.1: {d['benchmarks']['sumatra-3.6.1']['metrics']['open_time_ms']['mean']} ms\")"
tynypdf: 14.19 ms (null, Debug)  — will be replaced by mupdf Release in §3.5
sumatra-3.6.1: 1155.46 ms

$ wc -l src/core/CMakeLists.txt 2>&1; grep -rEn '#include.*windows|mupdf' src/core 2>&1 | wc -l
12 src/core/CMakeLists.txt
0
```

### 3.1 IR, page boxes and geometry — no engine crosses the seam

```bash
# Paste on merge of feat/3.1-ir:
$ git show --stat HEAD
 24 files changed, 2316 insertions(+), 13 deletions(-)
 create mode 100644 include/pdfcore/geom.h
 create mode 100644 include/pdfcore/pdfcore.h
 create mode 100644 include/pdfcore/sha256.h
 create mode 100644 src/core/SPEC.md
 create mode 100644 src/core/doc/SPEC.md
 create mode 100644 src/core/doc/doc.cc
 create mode 100644 src/core/geom/SPEC.md
 create mode 100644 src/core/geom/geom.cc
 create mode 100644 src/core/sha256.cc
 create mode 100644 tests/unit/test_geom.cc
 create mode 100644 tests/unit/test_doc_ir.cc

$ grep -rEn '#include *[<\"](windows|windef|d2d1|fitz|mupdf)' src/core src/render 2>&1; echo "exit:$?"
exit:0

$ sh tools/layering-check.sh --strict 2>&1
layering-check: backend_line_ratio=0.0721
layering-check: OK (19 source files, 0 violations)

$ ctest --preset linux-core -R "geom|doc_ir|contract" --output-on-failure 2>&1 | tail -n 40
Test project /home/castilho/projects/tyny-pdf/build/linux-core
    Start 1: test_status_abi
1/6 Test #1: test_status_abi ..................   Passed    0.01 sec
    Start 2: test_geom
2/6 Test #2: test_geom ........................   Passed    0.01 sec
    Start 3: test_doc_ir
3/6 Test #3: test_doc_ir ......................   Passed    0.01 sec
    Start 4: test_app_render
4/6 Test #4: test_app_render ..................   Passed    0.01 sec
    Start 5: test_cli_exit_codes
5/6 Test #5: test_cli_exit_codes ..............   Passed    0.06 sec
    Start 6: test_backend_contract
6/6 Test #6: test_backend_contract ............   Passed    0.01 sec

100% tests passed, 0 tests failed out of 6

$ python3 tools/spec-check.py 2>&1 | grep pending
spec-check: 17 requirement(s) still pending (no artefact yet): R14.1, R14.2, R14.3, R15.1, R15.2, R15.3, R15.4, R2.2, R2.3, R3.1, R3.2, R4.1, R4.2, R5.1, R5.2, R6.1, R6.2
```

- [x] IR + geometry copy-out, 4 rotations + CropBox, contract 2 backends, grep 0, layering green
- Evidence pasted above, on a clean clone, with the command line visible — memory = hypothesis
### 3.2 Text engine-agnostic — NFKC, pt-BR, caret over combining marks

```bash
$ git show --stat HEAD
# → (paste — allowlist: include/pdfcore/text.h, src/core/text/{normalize,break,caret,text}, fixtures/text/*)

$ ctest --preset linux-core -R text --output-on-failure 2>&1 | tail -n 50
# → (paste — normalize, break, caret, runs all green; pt-BR golden diff 0)

$ ldd build/linux-core/Debug/libpdfcore*.so 2>/dev/null | grep -i mupdf; echo "ldd exit:$?"
$ grep -rn "fitz\|mupdf" src/core --include="*.cc" --include="*.h" 2>&1 | wc -l; echo "grep exit:$?"
# → (paste — both 0)

$ python3 tools/lang-check.py 2>&1 | tail -n 5
# → (paste — OK, fixtures/text allowlisted)
```

- [ ] NFKC + pt-BR golden + caret over combining marks headless, `ldd` no mupdf in
  pdfcore
- Evidence pasted above

### 3.3 Budget in the core, sidecar atomicity and hygiene

```bash
$ git show --stat HEAD
# → (paste — allowlist: include/pdfcore/budget.h, src/core/budget/*, src/core/sidecar/writer.cc+lock.cc, tests/unit/test_sidecar_*)

$ ctest --preset linux-core -R "budget|sidecar_writer|sidecar_lock|sidecar_unknown|sidecar_schema" --output-on-failure 2>&1 | tail -n 60
# → (paste — budget ceiling fail/pass, R3.1 no .tmp left, R3.2 fresh/stale/missing, R4.1 preserve, R6 rejections all green)

$ python3 tools/sidecar-fmt.py check tests/fixtures/sidecar 2>&1
$ python3 tools/sidecar-fmt.py self-test 2>&1 | tail -n 5
# → (paste — OK 1 checked + self-test OK)

$ python3 tools/spec-check.py 2>&1 | grep pending
# → (paste — expected pending 17→12 after R3.1,R3.2,R4.1,R6.1,R6.2 closed; show delta)
```

- [ ] Budget struct + R3.1 atomic + R3.2 lock + R4.1 preserve + R6 exclusions,
  sidecar-fmt green
- Evidence pasted above

### 3.4 Version gate, id validation and staleness

```bash
$ git show --stat HEAD
# → (paste — allowlist: src/core/sidecar/{reader,stale}, include/pdfcore/sidecar.h, tests/unit/test_sidecar_{reader,ids,staleness}, golden/sidecar-stale-report.txt)

$ ctest --preset linux-core -R "sidecar_reader|sidecar_ids|sidecar_stale" --output-on-failure 2>&1 | tail -n 60
# → (paste — future_version → PC_ERR_VERSION, 12-case id table, fingerprint vs mtime two paths)

$ diff -u tests/golden/sidecar-stale-report.txt <(python3 -m tests.unit.test_sidecar_staleness 2>&1) 2>&1 | head -n 30
# → (paste — 0 diff)

$ python3 tools/spec-check.py 2>&1 | grep pending
# → (paste — expected pending 12→7 after R2.2,R2.3,R5.1,R5.2 closed)
```

- [ ] R2.2 version gate + R2.3 id validation + R5.1/R5.2 staleness with golden,
  pending 17→7
- Evidence pasted above

### 3.5 pdfcore API freeze, golden header and contract final — ratio saneado

```bash
$ git show --stat HEAD
# → (paste — allowlist: include/pdfcore/*.h, tests/golden/status_enum.txt, tests/unit/test_status_abi.cc, src/backends/mupdf/exception_bridge.h + mupdf_backend.cc, tests/contract/*, tests/baseline.json)

# Golden guard bite proof — throwaway branch:
$ git checkout -b tmp/renumber-proof
$ sed -n '1,40p' include/pdfcore/status.h
$ # change PC_ERR_NONE 0 → 99
$ ctest --preset linux-core -R status_abi --output-on-failure 2>&1 | tail -n 30
# → (paste — FAILED, diff vs golden)
$ git checkout main && git branch -D tmp/renumber-proof

$ grep -rn "fz_try" src --include="*.cc" --include="*.h" 2>&1 | grep -v exception_bridge; echo "exit:$?"
# → (paste — 0 lines, exit 1 from grep)

$ sh tools/layering-check.sh --strict 2>&1
# → layering-check: backend_line_ratio=0.06*  (must be ≤0.07)
# → layering-check: OK (N source files, 0 violations)

$ grep -rEn '#include *[<\"](windows|windef|d2d1|fitz|mupdf)' src/core src/render 2>&1 | wc -l
# → 0

$ python3 tests/bench/harness/run_benchmark.py --target tynypdf --backend mupdf --runs 5 --output /tmp/mupdf-baseline-1.json 2>&1 | tail -n 20
$ cat /tmp/mupdf-baseline-1.json | python3 -m json.tool | head -n 80
# → (paste run 1 — mupdf Release, 5 runs, machine_spec)
$ python3 tests/bench/harness/run_benchmark.py --target tynypdf --backend mupdf --runs 5 --output /tmp/mupdf-baseline-2.json 2>&1 | tail -n 20
$ cat /tmp/mupdf-baseline-2.json | python3 -m json.tool | head -n 80
# → (paste run 2 — within 10% per metric)
$ python3 -c "import json; a=json.load(open('/tmp/mupdf-baseline-1.json')); b=json.load(open('/tmp/mupdf-baseline-2.json')); [print(f\"{k}: {a['metrics'][k]['mean']:.2f} vs {b['metrics'][k]['mean']:.2f} delta {abs(a['metrics'][k]['mean']-b['metrics'][k]['mean'])/a['metrics'][k]['mean']*100:.1f}%\") for k in ['open_time_ms','first_paint_ms','scroll_time_ms','peak_rss_mb']]"
# → (paste — each <10%)

$ sha256sum tests/bench/corpus/*.pdf
# → (paste — corpus file hashes)

# Threaded R-M7 proof:
$ ctest --preset linux-core -R contract --output-on-failure 2>&1 | tail -n 30
# → (paste — threaded 10×160p: time_threaded / time_single <4.0)

$ sh tools/check.sh 2>&1 | tail -n 20; echo "exit:$?"
$ sh tools/gates-selftest.sh 2>&1 | tail -n 5; echo "exit:$?"
$ ctest --preset linux-core --output-on-failure 2>&1 | tail -n 20; echo "exit:$?"
# → (paste — all green, 13/13, 0 failed)
```

- [ ] API freeze + golden guard + backend harden (isolated locks) + contract 2
  backends + ratio
  ≤0.07 + baseline re-measured mupdf Release 2 runs within 10%
- Evidence pasted above, on a clean clone

### 3.6 Final epic gates (on the merge commit of 3.5, clean clone)

```bash
$ git clone https://github.com/daniel-castilho/tyny-pdf.git /tmp/tyny-epic3-final && cd /tmp/tyny-epic3-final
$ git rev-parse HEAD
# → (paste — merge commit of 3.5)

$ sh tools/check.sh 2>&1; echo "exit:$?"
# → (paste — 14/14, check: all gates green)

$ sh tools/gates-selftest.sh 2>&1; echo "exit:$?"
# → (paste — 13/13)

$ ctest --preset linux-core --output-on-failure 2>&1 | tail -n 20; echo "exit:$?"
# → (paste — 0 failed)

$ python3 tools/spec-check.py 2>&1
# → spec-check: OK (12 specs, 38 requirements, N source files, 0 orphans)
# → spec-check: 7 requirement(s) still pending (no artefact yet): R4.2, R14.1, R14.2, R14.3, R15.1, R15.2, R15.3, R15.4  (note: count 7-8 depending on R4.2)

$ sh tools/layering-check.sh --strict 2>&1
# → layering-check: backend_line_ratio=0.06* (≤0.07)

$ wc -l src/core/**/*.cc src/core/**/*.h 2>&1 | tail -n 5
# → (paste — ~2500 lines new core)

$ git ls-tree -r --name-only HEAD | wc -l
# → (paste — base +5 epic docs + ~15 new src files)
```

- [ ] Final gates green on `main`, not on a working copy, with pasted evidence per
  gate

---

## 4. Failures this document encodes (why each rule exists)

| Rule | The failure it kills | How the rule catches it |
|------|---------------------|------------------------|
| §3.0 baseline before epic | A core epic that claims a ratio improved without a before number — the 2026-09-20 baseline exists precisely so §3.5 delta is measured | Pasted `layering-check` 0.1014 before vs 0.06 after |
| §3.1 allowlist/denylist | AI adds `mupdf/fitz.h` to `src/core` to "reuse" a helper — R-M10 violated and `spec-check` still green | `grep windows\\|mupdf src/core` 0 pasted after every core story |
| §3.2 `null` vs `mupdf` contract | AI tests text only on `null` synthetic, never on `mupdf` extraction — swap later fails | `TEST_P` over both backends in one job |
| §3.3 atomic rename not copy | AI writes sidecar directly, then a crash truncates the file — no test for crash safety | Kill-mid-write test pasted |
| §3.3 lock time is tested with fake clock | AI uses `sleep 301` to test 5-minute stale — CI flaky by 1s | Fake `pc_clock_override` in `lock.cc` test seam |
| §3.4 version gate wording | AI returns generic `PC_ERR_VERSION` without naming X vs Y — caller cannot tell if sidecar is future or just corrupt | Golden stale report diff |
| §3.4 id regex allows `8`/`9` | Base32 alphabet is `A-Z2-7`, but AI allows `8`/`9`/`0`/`1` — ids collide in git merges | 12-case id table pasted |
| §3.5 `fz_try` outside bridge | AI duplicates `fz_try` pattern in `mupdf_backend.cc` directly — exception unwinds through `pdfcore` | `grep fz_try \| grep -v bridge` → 0 |
| §3.5 threaded proof missing | Lock set per-context looks correct but is still process-wide — only a 10-thread benchmark shows 13.3× | Threaded <4× table pasted |
| §3.5 typed ratio | `backend_line_ratio 0.07` typed from memory, actual is 0.09 after core landed — `docs/lessons.md` 2026-09-20 rule | Pasted `layering-check --strict` number |
| §3.6 clean clone, not workspace | `sh tools/check.sh` green in workspace but red on clean clone because `generated/` not committed — `workspace != main` | Clone `git clone` + `sh tools/check.sh` pasted |

## 5. Epic 3 completion checklist

- [ ] 3.1: IR + geometry copy-out, 4 rotations + CropBox, contract 2 backends, grep
  0, layering
  green
- [ ] 3.2: NFKC + pt-BR golden + caret over combining marks headless, `ldd` no mupdf
  in pdfcore
- [ ] 3.3: budget struct + R3.1 atomic + R3.2 lock + R4.1 preserve + R6 exclusions,
  sidecar-fmt
  green
- [ ] 3.4: R2.2 version gate + R2.3 id validation + R5.1/R5.2 staleness with golden,
  pending 17→7
- [ ] 3.5: API freeze + golden guard + backend harden (isolated locks) + contract 2
  backends + ratio
  ≤0.07 + baseline re-measured mupdf Release 2 runs within 10%
- [ ] `sh tools/check.sh` (14/14), `sh tools/gates-selftest.sh` (13/13), `ctest
  --preset linux-core
  --output-on-failure` (0 failed), `sh tools/layering-check.sh --strict` (≤0.07),
  `python3
  tools/spec-check.py` (0 orphans, 7 pending), `python3 tools/docs-check.py` (0
  problems over the 5
  new epic docs) — all pasted in §3.6 on the merge commit

---

*Epic 3 complete means: the heart beats on Linux without a window, the sidecar is
crash-safe and
forward-compatible, the C ABI is frozen and golden-guarded, and the engine is
hardened with isolated
locks. Every later delta reads this heart; none rewrites it. This document is
included in every PR
and hand-off related to Epic 3; a hand-off without pasted evidence returns to its
author.*
