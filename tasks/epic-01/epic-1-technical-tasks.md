# Epic 1 – Technical Tasks [grounded]

This document is the only complete and up-to-date version of the plan. `[x]` marks are filled in
during execution; evidence is pasted in `epic-1-dod.md`. All boxes are unchecked on purpose: nothing
in this epic has been executed yet.

## 1.1 `main` carries the seed and CI proves it

- [x] Started from a real git repo, not a directory copy: `git init -b main`, `git config
  core.hooksPath .githooks`, then `git add -A` and **verify the staged list against `git
  ls-files` expectations before committing** - the seed loss was a copy that dropped dotfiles,
  and `git add` drops nothing if it is run from inside the tree. The tree this produced is the
  one Story 1.1 measures (48/48 parity, gate green in a clean clone - `epic-1-dod.md` §1.0)
- [x] Pushed `main` containing the maintained seed (48 files, not the planning-era 42); confirmed
  from the API, not from memory: `GET /repos/daniel-castilho/tyny-pdf/git/trees/main?recursive=1`
  blob count and `GET /repos/.../contents/.github/workflows/gates.yml` both in
  `epic-1-dod.md` §1.0
- [x] The branch `chore/seed-gates-ci` carried Story 1.1 and was merged as `1e5d9f0` (PR #1,
  squash); the seed commits remain retroactive context (owner decision D-3,
  `analysis/decision-log.md`), and every change after this point is a PR (ADR-0009)
- [x] The six branch-protection settings of `docs/git-workflow.md` "Branch protection" are applied
  (ticked after the API call succeeded - JSON pasted in `epic-1-dod.md` §1.1; personal-repo note:
  the `users/teams/apps` restrictions form is org-only, so push restriction is enforced by
  require-PR + enforce_admins + required status checks); `GET /repos/.../branches/main/protection`
  JSON pasted in `epic-1-dod.md` §1.1
- [x] Toolchain pins added to `docs/dev-environment.md` "Toolchain pins (measured, Story 1.1)":
  CMake/Ninja/Clang/`g++`/Python versions measured; Clang-Tidy/Clang-Format/Conan/Doxygen/
  LLVM-MinGW and the Windows SDK explicitly "not installed" and each opens its install task in
  PR #2
- [x] First CI iteration on the PR is green: run `35174756434` (`gates` success, head `6dc7a07`),
  recorded in `epic-1-dod.md` §1.1; the job matrix is the single `gates` job (ubuntu-24.04)
- [x] Gate proven to bite: on the throwaway branch `chore/demo-gate-bites2` (PR #3, closed unmerged)
  a one-space break in `tests/fixtures/sidecar/example.tynypdf.json` turned run `35175102894` red at
  section 4/7 (exit 1); the green-after-revert run is PR #1's own `35174756434` on the same tree -
      both logs pasted in `epic-1-dod.md` §1.1
- [x] A direct push to `main` is rejected by the branch-protection hook (the admin-side proof,
      `enforce_admins`, pasted in `epic-1-dod.md` §1.1). Repeating the same from a non-admin
      identity remains OWNER-PENDING: no second identity or token available (owner decision D-6,
      `analysis/decision-log.md`)
- [x] `docs/lessons.md` now records the exact loss mechanism (a filesystem copy that drops dotfiles)
  and the ADR-0009 deviation of the three direct pushes, with the rules that prevent both
- [x] Corrected `README.md` "Current State" and `AGENTS.md`: the seed is no longer "PR #1 pending",
  the state on the date of the fix is pasted (`git log -1 --format=%H`, blob count, run count,
  `pulls?state=all` length) in `epic-1-dod.md` §1.0-§1.1

## 1.2 Build system and toolchain probes

- [ ] `CMakeLists.txt` with `src/CMakeLists.txt`, `src/core/CMakeLists.txt`,
      `src/backend/null/CMakeLists.txt`, `src/apps/CMakeLists.txt`, `tests/CMakeLists.txt`,
`docs/CMakeLists.txt` (layout from ADR-0002, no file outside `src/backends/` may name an engine)
- [ ] `CMakePresets.json` with `linux-core` (host, GCC or Clang, `-fsanitize=address,undefined`,
      `CMAKE_CXX_FLAGS=-fno-exceptions -fno-rtti`) and `win-cross-x64`
      (`-DCMAKE_TOOLCHAIN_FILE=build/cmake/toolchain-llvm-mingw.cmake`,
      `-DMUPDF_BUILD_DIR=build/mupdf-windows-x64`, `-G Ninja`); the `win-msvc/` presets of
      `docs/dev-environment.md` section 2 are added as a separate configure dir
- [ ] `build/cmake/toolchain-llvm-mingw.cmake`, with the compiler obtained the way
      `docs/dev-environment.md` already prescribes: a pinned tarball whose SHA-256 lives in
      `third_party/toolchains/llvm-mingw.sha256`, unpacked under `~/.toolchains/llvm-mingw-<ver>/`,
      version pinned in `CMakePresets.json`. No download script exists in `tools/` today, so this
      task writes the fetch-and-verify step as a command in the document, not as an invented tool
- [ ] `.clang-format`, `.clang-tidy`, `.clangd` written **in this order, all three at once**, so
      editor and CI cannot disagree (this repo has no precedent for a style file; the LLVM-MinGW
      build is the only one that consumes them today)
- [ ] `conan.lock` produced by `conan lock create` against a real graph, not hand-written
- [ ] `tools/win-probe/` (planned, PR #2 per the debt list in `AGENTS.md`): four tiny programs
  plus a `build.sh`
      - [ ] `d2d_probe.c` including `d2d1_3.h`, `dwrite_3.h`, `d3d11.h`, `dxgi1_6.h`, linking the
        four import libs from ADR-0010's list, then a build of the full engine over the same
        headers
      - [ ] `runtime_probe.c` with `-static` + `/MT`, then `objdump -p tynypdf.exe` (LLVM-MinGW) and
            `dumpbin /dependents tynypdf.exe` (MSVC) -> both lists pasted, only `*.dll` from
            `System32` allowed
      - [ ] `abi_probe.cc` with the `pc_*` surface from 1.4 compiled under both toolchains; the two
            `nm -C --defined-only` outputs diffed
      - [ ] `gpu_probe.cpp` using `DXGIFactory1::EnumAdapters1` + `CheckFeatureSupport` (WSL
        interop); it must print an adapter LUID and a `D3D_FEATURE_LEVEL` with no `LUID` of the
        Microsoft software rasteriser, otherwise the GPU story in ADR-0010 changes
- [ ] Record each probe result in `adr/0010-build-environment-wsl2.md`: delete the corresponding
      "Unverified" item or replace it with the measured downgrade plus the failing output
- [ ] Update `docs/coding-standards.md`: the "no `src/` yet" sentence goes away with the first real
      file; `tools/format-check.sh` (planned, PR #2) is listed as a step in
      `.github/workflows/gates.yml` once it exists

## 1.3 The engine is vendored and the supply-chain gates have teeth

- [ ] `git submodule add` MuPDF at a **40-hex commit**: ADR-0004 section 1 pins "an exact upstream
      commit recorded in `third_party/UPSTREAM.toml`" and `AGENTS.md` restates it, so a branch name
      is not a pin. Keys of the file: `upstream_url`, `upstream_commit`, `vendored_on`,
      `patch_series`; each patch file carries `Subject`, `Reason`, `Upstream-status`
      (`none`, `proposed:<url>`, `merged:<commit>`) and `Owner`, which is why a patch can be audited
      without reading the engine
- [ ] `third_party/README.md` written as the procedure - `deps-refresh.sh`, where the patch series
      lives, the `UPSTREAM.toml` rules, and the verification command from
      `docs/dev-environment.md` (`sha256sum` of the toolchain against
      `third_party/toolchains/llvm-mingw.sha256`). Nothing in the tree describes this procedure
      today, which is why the file is a deliverable of this task rather than an existing reference
- [ ] `third_party/patches/0001-*.patch` ... as the only place upstream is touched (ADR-0004
      section 1: an ordered series, never commits on a private branch); a patch
      applied by hand is an incident, not a workflow
- [ ] `tools/layering-check.sh`, `tools/deps-refresh.sh`, `tools/patch-report.sh`, `tools/sbom.sh`
      (planned, PR #4) with the semantics ADR-0011 R-M6/R-M11 and M0's exit criterion 2 (the
      `layering` row of `docs/kickoff.md`) already define; `layering-check.sh` additionally reports
      `src/backends/mupdf/** / (src/** + include/**)` as a number and **wires the ratio check while
      it is high**, per the instruction in ADR-0011 R-M6
- [ ] Define the empty-tree behaviour explicitly in the script: if `src/core` does not exist, the
gate prints that it has nothing to check and exits non-zero until 1.4 lands. A green run over a
      directory that does not exist is the failure mode this repo has already written down twice
- [ ] `NOTICE` with engine, fonts, third parties, licences; `LICENSE` stays the verbatim
      `gnu.org/licenses/agpl-3.0.txt` text already in the tree (34523 bytes, sha256
      `0d96a4ff68ad6d4b...`) - never a "summary"
- [ ] `docs/references.md` seeded from the URLs in ADR-0004, ADR-0010, `docs/dev-environment.md`,
      `docs/release-runbook.md` - a list is cheaper to verify than memory

## 1.4 Page 1 renders twice from one API

- [ ] `include/pdfcore/status.h` with `pc_status` exactly as ADR-0003 section 1 and
      `src/features/sidecar/SPEC.md` state it, `PC_ERR_*` codes in an append-only numbering,
      `detail` pointing into static storage
- [ ] `tests/golden/status_enum.txt` + `tests/unit/test_status_abi.cc`: the enum values are compared
      against the golden file so a renumbering is a red build (R-M12), and the enum in
      `docs/coding-standards.md` and the header carry the same integers (the naming-sync rule exists
      for this class of drift)
- [ ] `include/pdfcore/{doc.h,page.h,render.h,backend.h,import.h}` and `pdfcore.h` as the aggregate;
      `pc_doc_open`, `pc_doc_page_count`, `pc_page_render` only - `pc_doc_save` is M1, `pc_doc_sign`
      is M2/D-2, `pc_txn_*` is D-6, `pc_text_*` is M1 (ADR-0002 section 5 says "declared" for
      `pc_text_*`/`pc_doc_save`; this epic does not declare what it cannot implement)
- [ ] `src/backend/null/null_backend.cc` implementing the `pc_backend_api` vtable against a fixed
      pattern, no engine headers anywhere near it
- [ ] `src/backends/mupdf/mupdf_backend.cc` + `src/backends/mupdf/exception_bridge.h` with its
      `PC_TRY`/`PC_CATCH` pair (ADR-0003 section 2); the only TU that includes an `mupdf/` header
- [ ] `tests/contract/backend_contract.cc` run twice - once per backend - with the `null` result as
      the pixel-stable reference, plus `tests/render/ref/page1-linux.png` and
      `tests/render/ref/page1-win32.png` (ADR-0002 sections 5-6)
- [ ] `src/apps/tynypdf/main.cc` CLI: `tynypdf render in.pdf --page N [--dpi D] [--out F]
      [--backend null|mupdf]`; exit codes 0/1/2/3 per ADR-0003 section 6 with
      `tests/unit/test_cli_exit_codes.cc` asserting corrupt-before-unsupported precedence
- [ ] `src/win32/render/window.cc`: create a window, blit the `pc_page_render` bytes, and compare
  the captured frame hash with the PNG written by the CLI on Linux (M0.3)
- [ ] `src/features/render/` gets a `SPEC.md` at the same time as its first file, never before
      (spec-check enforces the pairing both ways)

## 1.5 The bar is measured before we clear it

- [ ] `tools/bench-measure.sh` + `tests/bench/harness/` + `tests/baseline.json` (planned, PR #3):
      measure M0.4's five metrics with `--target sumatra-3.6.1`, `--target sumatra-3.7pre` and
      `--target tynypdf` on the same corpus and the same machine
- [ ] Record the reference machine spec (CPU model, RAM, GPU, `os-release`, compiler version) in the
      baseline file itself, not in prose
- [ ] Download both SumatraPDF channels from `sumatrapdfreader.org`, pin each installer's sha256,
  and note the acquisition date and channel - `docs/release-runbook.md` section 3 already says
  where 3.7 pre-release comes from
- [ ] Run the harness twice; both outputs pasted; every metric within 10 % of the first run
- [ ] Express the acceptance bar relatively and write it into the M0 exit criteria list in
      `docs/kickoff.md` (numbers, not "competitive"); no absolute milliseconds
- [ ] Create the conformance corpus repo, protect its default branch with the same six settings, and
      pin it here as a submodule at a commit hash (ADR-0010's rule: "never by branch name");
      `tests/conformance/` stays absent until that exists - an empty directory is not a gate
- [ ] Write the intake plan for the corpus: sources, licences, who screens each file, what
      "problematic" means; today D13 in `docs/kickoff.md` records that screening has not started
- [ ] `tools/corpus-check.py` (planned, PR #3) validates the corpus `manifest.txt` before the
  numbers from it are trusted

## 1.x Final epic gates

- [ ] `sh tools/check.sh` (exit 0) - all seven gates, on a clean clone of `main` and on the PR head
- [ ] `sh tools/check.sh --help` prints a self-describing help and exits 0; running it from a
  foreign CWD still finds the repo root
- [ ] `python3 tools/docs-check.py` reports 0 problems - the five files in `tasks/epic-01/` and
      every relative link resolving, including `../../docs/kickoff.md` and `../../adr/...`
- [ ] `python3 tools/spec-check.py` reports 0 orphans: every `R<Module>.<n>` cited anywhere
  exists in a `SPEC.md`; new modules introduced by 1.4 arrive with their spec
- [ ] `python3 tools/naming-sync.py` reports 0 problems (the exe and installer names in CMake match
      `docs/naming.md`)
- [ ] `python3 tools/lang-check.py` reports 0 problems over the whole repo
- [x] CI green on the merge commit of 1.1 (run `35175247781`, success, head `1e5d9f0`, pasted in
  `epic-1-dod.md` §1.1); 1.2-1.5 still pending
- [x] The `epic-1-dod.md` self-audit was run for the Story 1.1 hand-off, its output pasted at
  §2.1 (2026-09-17)

**Epic 1 completion checklist:**

- [x] 1.1: `main` parity (48 at parity time, 49 on the merge commit) + first green CI run
  (35174756434) + six protection settings pasted + the gate that bites (runs 35174885710,
  35175102894); non-admin push rejection: OWNER-PENDING (D-6)
- [ ] 1.2: build system on both toolchains + ADR-0010's assumptions retired by evidence
- [ ] 1.3: engine pinned, patch series real, four new gates with defined empty-tree behaviour
- [ ] 1.4: `pdfcore` M0 surface, two backends, one contract suite, page 1 twice
- [ ] 1.5: baseline measured on both channels and reproducible within 10 %, corpus pinned, intake
  plan
- [ ] Final gates green, evidence pasted in `epic-1-dod.md`

---

*Next step: execute tasks 1.1-1.5 in order; 1.1 gates everything else because a red `main` cannot
evidence anything. Definitions in `epic-1-stories.md`, testing in `epic-1-testing.md`, and the
completion evidence in `epic-1-dod.md`.*
