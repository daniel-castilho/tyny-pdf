# Epic 1 – Testing Strategy [grounded]

Test levels are the taxonomy of `docs/testing-playbook.md` section 2 (Repo gates, Unit, Contract,
Approval, Conformance, Performance, Privacy, Windows-specific). Everything here names the command
that executes it and the artefact the output goes into. A test that cannot be run yet is labelled
with the PR that introduces it, exactly as `docs/testing-playbook.md` section 3 does.

## 1.1 `main` carries the seed and CI proves it

- **Goal:** the published tree and the workspace tree agree, and the repository's own gate is
  proven to run and to be able to fail on `main`, not only in a working copy.
- **Action:**
  - Run `git ls-tree -r --name-only main | wc -l` and the byte sizes of the five truncated files
    (`git cat-file -s`, `wc -c` on the workspace copy); compare, do not assert
  - Run `sh tools/check.sh` in a **fresh `git clone`** of `main` - the workspace is not the evidence
  - Read the CI job log of the merge commit: which of the seven sections ran, what each printed
  - Deliberately break one fixture on a throwaway branch; capture the red log; revert; capture the
    green log
  - Query `GET /repos/daniel-castilho/tyny-pdf/branches/main/protection` with a token and paste the
    JSON; today the same call unauthenticated returns HTTP 401, which is not a pass
  - Attempt `git push origin HEAD:main` from a non-admin identity; paste the rejection text
- **Acceptance criterion:** the four outputs above (parity counts, clean-clone gate, red job log,
  protection JSON) are pasted in `epic-1-dod.md` section 1, story 1.1. A count that only exists
  in prose is a hypothesis and must be labelled as one (Rule zero).

## 1.2 Build system and toolchain probes

- **Goal:** every ADR-0010 assumption becomes a command with output, and the tree builds with two
  different compilers.
- **Action:**
  - `cmake --preset linux-core && cmake --build --preset linux-core && ctest --preset linux-core`
    (GoogleTest, ASan/UBSan enabled) - PR #5's first target is what makes this meaningful; before
    that the presets are exercised with the probe programs only
  - `cmake --preset win-cross-x64 -DCMAKE_BUILD_TYPE=Release && cmake --build --preset
    win-cross-x64` then `file build/win-cross-x64/Release/tynypdf.exe`
  - `tools/win-probe/build.sh` (planned, PR #1 per `AGENTS.md`) then the four programs, each with
    its own pasted output; `objdump -p` under LLVM-MinGW and `dumpbin /dependents` under MSVC for
    the same exe
  - The MSVC parity job on `windows-latest` runs the same ctest suite
- **Acceptance criterion:** M0.2's criterion, unchanged: "the same code builds and passes its tests
  under both toolchains". A single `PASS` per toolchain is not sufficient for the ABI probe; the two
  `nm -C --defined-only` outputs are diffed and the diff is pasted even when it is empty.

## 1.3 The engine is vendored and the supply-chain gates have teeth

- **Goal:** the engine is pinned to something immutable, and the four new gates print numbers rather
  than assurances.
- **Action:**
  - `git submodule status` -> the line must contain a 40-hex sha, and `grep -rn 'branch'
    third_party/*.toml` must print nothing: ADR-0004 section 1 pins an exact upstream commit and
    `AGENTS.md` restates it, so a branch name is not a pin
  - `sh tools/deps-refresh.sh` (planned, PR #4) twice in a row: idempotence is the property being
    tested, and the second run must change nothing (`git status --porcelain` empty afterwards)
  - `sh tools/patch-report.sh` (planned, PR #4) against the real patch series; the table must match
    `ls third_party/patches/*.patch | wc -l`
  - `sh tools/layering-check.sh` (planned, PR #4) with the ratio printed; then, to prove it
    bites, add an `#include <mupdf/fit.h>` to a file under `src/core/` on a throwaway branch and
    paste the red output
  - `sh tools/sbom.sh` (planned, PR #4) and validate the CycloneDX JSON against its published schema
- **Acceptance criterion:** ADR-0011 R-M6/R-M10/R-M11 hold on a real tree, and every gate has one
  demonstrated failure mode. A gate that has only ever printed OK is untested (this repo's own
  `lang-check` self-test pattern is the precedent: the checker is tested by feeding it a violation).

## 1.4 Page 1 renders twice from one API

- **Goal:** one C surface, two backends, identical pixels on two operating systems.
- **Action:**
  - `ctest --preset linux-core -R 'contract'` run against `null` and `mupdf`; both must pass in the
    same job, and the `unsupported` case must be asserted as `unsupported` (R-M5), never as empty
  - `tests/unit/test_status_abi.cc` compares `include/pdfcore/status.h` to
    `tests/golden/status_enum.txt`; then renumber one enum on a throwaway branch and paste the red
    output (R-M12 proves itself this way or not at all)
  - `tests/unit/test_cli_exit_codes.cc`: four cases for exit codes 0, 1, 2, 3, including the
    corrupt-before-unsupported precedence ADR-0003 section 6 requires
  - Approval flow for the render: `tynypdf render sample.pdf --page 1 --dpi 150 --out
    build/out/page1.png`, compared to `tests/render/ref/page1-linux.png` by sha256 of the pixel
    buffer, not of the PNG container
  - Same bytes captured from the Win32 window on Linux via interop, and on the Windows CI job
- **Acceptance criterion:** M0.3: "the same page-1 pixels come out of the shared core on both
  operating systems, through the same `pc_doc_open` / `pc_page_render` calls". Two different
  pixel outputs is a fail even if both look plausible to a human.

## 1.5 The bar is measured before we clear it

- **Goal:** SumatraPDF's real numbers on a real machine, on two release channels, reproducible.
- **Action:**
  - `sh tools/bench-measure.sh --reference` (planned, PR #3) for `sumatra-3.6.1`, `sumatra-3.7pre`,
    `tynypdf` on the pinned corpus; harness logs the machine spec into `tests/baseline.json`
  - Run the whole thing twice; the criterion is per-metric delta within 10 %, so both outputs are
    pasted side by side
  - `python3 tools/corpus-check.py` (planned, PR #3) validates the corpus `manifest.txt`; a file
    that is not in the manifest is not measured
  - `git -C tests/conformance rev-parse HEAD` -> 40-hex; `git ls-tree HEAD tests/conformance` in the
    superproject shows a gitlink, not a directory of loose files
- **Acceptance criterion:** M0.4/M0.5: the baseline file exists with both channels and the corpus is
  pinned at a commit; the acceptance bar is stated relatively. A number in a document without its
  query, unit and date is deleted, per `docs/testing-playbook.md` section 3.8.

## Regression gates (per story and at the end)

- [ ] `sh tools/check.sh` after every story, and on a clean clone before any merge claim
- [ ] `python3 tools/docs-check.py` - 0 problems, with the five `docs/epics/*.md` files inside its
      scope (24 markdown files become 29 the moment these land)
- [ ] `python3 tools/spec-check.py` - 0 orphans, and it must still list the pending requirement ids;
      a run that stops listing them before the artefacts exist is a broken checker
- [ ] `python3 tools/naming-sync.py` - 0 problems once CMake exists, or it cannot see the names it
      guards and is silently vacuous
- [ ] `python3 tools/lang-check.sh` - 0 problems (it has its own self-test in the gate, which is the
      pattern story 1.3 copies)
- [ ] `sh tools/gates-selftest.sh` (planned, PR #2) if that is where the five self-tests land;
      otherwise the per-tool self-tests as invoked by `.github/workflows/gates.yml`
- [ ] CI: `gates` green on each of the five merge commits; five run ids in `epic-1-dod.md`

**Epic 1 testing checklist:**

- [ ] 1.1 parity + clean-clone gate + red-job proof + protection JSON + push rejection
- [ ] 1.2 two toolchains, same tests, ABI diff pasted
- [ ] 1.3 four new gates, each with one demonstrated failure
- [ ] 1.4 contract on both backends, enum golden red-proof, four CLI exit codes, same pixels twice
- [ ] 1.5 two channels, two runs within 10 %, corpus pinned at a sha
- [ ] Final gates green on `main`, not on a working copy

---

*Next step: run the actions above as each story executes and paste the outputs into
`epic-1-dod.md`; tasks in `epic-1-technical-tasks.md`, criteria in `epic-1-stories.md`.*
