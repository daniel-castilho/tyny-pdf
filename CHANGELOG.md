# Changelog

All notable changes to this project are documented in this file (AGENTS.md rule 11).
The format follows Keep a Changelog, and the versioning follows
CalVer-compatible SemVer as declared in [docs/release-runbook.md](docs/release-runbook.md).

## [Unreleased]

### Added (epic 4 story 4.3 - font fallback per run, no tofu)

- **Public text-fallback API**: `include/pdfcore/text.h` exposes `pc_text_fallback_runs` /
  `pc_text_run_free`. `src/core/text/fallback.cc` (own UTF-8 decoder, no engine include) splits a
  string into maximal runs covered by one backend face, choosing the first face in declaration
  order; a codepoint no face draws fails all-or-nothing with `PC_ERR_LIMIT` and detail
  `"missing glyph U+XXXX"` - never tofu. `tests/golden/text-fallback-faces.txt` pins the
  per-face run report against the pt-BR fixture (`U+0301`/`U+0327`/`U+0303`, em dash `U+2014`,
  `U+2211`).
- **Vtable append (R-M3, ABI minor 0 -> 1)**: `face_count` + `face_coverage(face, cp, &has)`
  appended to `pc_backend_api` alongside capability `PC_CAP_FACE_COVERAGE = 2`, negotiated via
  `doc_has_capability`. MuPDF exposes no face-enumeration API, so the probe list is a curated
  ~8-10-face table in `src/backends/mupdf`, each face loaded per query with
  `fz_lookup_builtin_font` + `fz_encode_character` and dropped (R-M6); the null backend declares
  the capability off and answers capability not supported (R-M5).
- **Layering stays healthy**: `backend_line_ratio` measured 0.0429 before this story; the new
  core lines and the small adapter probe keep it under the 0.07 epic target and the 0.15 hard
  gate.

### Added (epic 4 story 4.2 - replay + CLI byte-identical log)

- **Canonical txn JSON**: `pc_txn_to_json` / `pc_txn_from_json` round-trip the undo/redo stacks
  and budget through `src/core/json/canonical.cc` (keys sorted, 2-space, LF, 3 decimal places).
  Unknown keys are preserved. `from_json(to_json(x))` is byte-identical for 20 generated logs.
- **CLI replay**: `tynypdf-cli txn replay <log.json> --out <out.json>` exits 0/1/2 per ADR-0003
  §6; CLI output bytes equal core `pc_txn_to_json`.

### Added (epic 4 story 4.1 - transaction log core)

- **Undoable IR, byte-identical**: `src/core/doc/transaction.cc` holds the undo/redo stacks as
  command value types (type, annotation id, before/after rect) that mutate only the IR and never
  touch an engine (R-M4/R-M8). `pc_doc_hash` hashes the IR deterministically, and
  `tests/unit/test_txn_core.cc` proves `apply; undo; redo` is byte-identical to `apply` over a
  5-page null-backend synthetic document.
- **Budget gates undo (R18.4)**: `pc_budget { max_tiles, max_bytes }` ceilings the undo stack;
  pushing past either returns `PC_ERR_LIMIT` with detail `"undo budget exceeded"` and leaves the
  IR untouched. The unit test fails on a tight ceiling and passes after raising it.
- **Minimal annotation IR substrate**: the document IR now carries `{ id, rect }` annotation
  elements (base32 id per R2.3, sidecar mirror) as the mutation target for `pc_txn_apply`;
  form rules, anchoring and rendering extras stay D-2 (Epics 5/6).
- **Layering improved**: `backend_line_ratio` dropped from 0.0686 to 0.0601 with the new core
  lines; `src/core` grew from 1090 to 1388 lines.

### Added (epic 3 story 3.5 - pdfcore API freeze and backend harden)

- **Append-only C ABI, golden-guarded**: `include/pdfcore/status.h` is frozen at 17 codes with
  `tests/golden/status_enum.txt` as the committed dump and `tests/unit/test_status_abi.cc` failing
  on any renumber (proven red on a throwaway renumber branch). `pc_backend_api` grew to ABI 1.1
  with `doc_has_capability`/`doc_find_tables` appended, not reordered.
- **Capability contract (R16.1)**: `PC_CAP_TABLES` is declared unsupported by both backends, so
  `pc_doc_find_tables` returns `PC_ERR_CAPABILITY` - never an empty list (R-M5).
- **Isolated per-document locks (R-M7)**: each `MupdfDoc` allocates its own `FZ_LOCK_MAX`
  recursive mutexes instead of sharing MuPDF's process-wide default;
  `tests/contract/threaded_contract.cc` proves 10 threads x 160 pages at 5.70x single-threaded
  (4.0x target). The lock gotcha is documented in `docs/lessons.md`, not re-invented in the
  header.
- **Engine exceptions funnel through one bridge**: `src/backends/mupdf/exception_bridge.h` is the
  only file allowed to contain `fz_try`/`fz_catch` (enforced by grep), mapping MuPDF exceptions to
  `pc_status` at one point.
- **Backend swap budget under target**: `sh tools/layering-check.sh --strict` now prints
  `backend_line_ratio=0.0683` (target ≤ 0.07), down from 0.1014 at the epic 3 baseline.
- **Baseline re-measured**: `tests/baseline.json` replaces the CLI entry with
  `tynypdf (mupdf backend)` Release measurements; two consecutive 10-run invocations stay within
  10% on open/first-paint/scroll. Corpus hashes pasted in `tasks/epic-03/epic-3-dod.md` §3.5.
- **Release build is `-Werror` clean again**: `test_sidecar_lock.cc`/`test_sidecar_writer.cc`
  now use their `write`/`fread` results (unused-result was failing the O3 build), and
  `src/core/sidecar/stale.cc` no longer truncates through a caller buffer under
  `-Werror=format-truncation`.

### Fixed

- `tests/baseline.json` gains an explicit note for `peak_rss_mb`: the CLI render exits in ~3 ms and
  the 1 ms monitor samples through the peak, so RSS is below the plottable floor; timing metrics
  are the DoD gate.

### Changed (completing what PR #30 claimed and did not carry)

- **`docs/release-runbook.md` no longer denies the tree**: it said "there is no code, no build
  system, no CI history". `CMakeLists.txt`, `src/` and the vendored engine exist and four CI jobs
  are green on the commit the file was read from; what is still true is that no artefact has been
  published, and the file says exactly that.
- **The three surviving counts are gone**: `docs/git-workflow.md`, `docs/testing-playbook.md` and
  `docs/release-runbook.md` still told the reader "the eight gates", "nine checks, eleven sections"
  and "ten sections" about a runner that prints fourteen. They now say "every section it has".
- **`docs/lessons.md` keeps the rule** (a count a command prints is quoted with its command or not
  quoted at all) and records that a merged PR is not proof a change landed.
- **`conan.lock.bak` removed**, and `.gitignore` now ignores `*.bak`, `*.orig` and `*~`: the backup
  landed in `main` because nothing refused it.

### Fixed

- `tests/baseline.json` stays as PR #28/#29 left it. The numbers are not touched here; the
  cross-machine comparison it encodes and the absent validator are tracked as their own item, per
  `docs/lessons.md` (a claim needs the command that produces it).

### Added (supply chain gates, ADR-0004)

- **CycloneDX SBOM**: `tools/sbom.sh` generates a valid SBOM from `conan.lock`, linker inputs, and
  the vendored MuPDF fingerprint; emits JSON with components, hashes, and purls.
- **Dependency refresh**: `tools/deps-refresh.sh` runs `conan lock create`, the full test suite
  (`ctest --preset linux-core`), and the full gate (`tools/check.sh`); `--dry-run` for preview.
- **Patch report**: `tools/patch-report.sh` prints a status table for `third_party/patches/*.patch`
  and fails if any `Upstream-status: none` entry exceeds 180 days (`--fail-on-stale`).
- All three tools ship `--self-test` and are wired into `tools/check.sh` (sections 11-13) and
  `tools/gates-selftest.sh` (now 12/12 suites).

### Added (CLI honesty, R12.x)

- **Real PNG output**: `tynypdf-cli render --out` now writes valid PNG files (RGBA8, stored-deflate,
  CRC32/Adler-32) via a new dependency-free `src/cli/png_writer.{h,cc}` instead of PPM.
- **Correct exit codes** (R12.2): page out-of-range → 2, non-numeric page → 2, unknown backend → 2,
  corrupt file → 1, success → 0. Removed unimplemented `info` command from usage.
- **Real CLI tests**: `test_cli_exit_codes` invokes the CLI binary and asserts all four exit codes
  plus PNG validity; `test_app_render` opens a document and renders page 1 via the `pdfcore` API
  (null backend), satisfying R10.1 honestly.

### Added (layering gate, ADR-0011 R-M10/R-M11)

- **Architecture boundary gate**: `tools/layering-check.sh` now enforces R-M10 (no Windows or
  DirectX header in `src/core`/`src/render`, no engine header outside `src/backends`, no engine
  symbol or IR mutation in `src/render`/`src/os`) and R-M11 (`backend_line_ratio`, reported and
  failed above 0.15). Ships with an 8-case `--self-test`, wired into `tools/check.sh` (section 10)
  and `tools/gates-selftest.sh` (9/9 suites).
- **R-M11 is now exact**: ADR-0011 and ADR-0002 carry corrections fixing the numerator (backend
  lines outside a function assigned in the `pc_backend_api` initializer) and the denominator (all
  lines under `src/` plus `include/`); the measured ratio on this tree is 0.1418.

### Changed (layering gate)

- Stale `(planned, PR #4)` markers and `layering-check.py` references updated across `AGENTS.md`,
  `adr/`, `docs/` and `tasks/`; the `README.md` Quality Gates table now reads eleven sections and
  9/9 suites.

### Changed (distribution)

- **Windows Store / MSIX is not pursued**: the portable Windows ZIP attached to a GitHub Release
  is the only distribution channel, so nothing re-signs the binary or weakens the no-installer
  promise (`docs/kickoff.md` section 12, `docs/release-runbook.md` section 9).

### Added (diff-scan gate and the unified self-test runner)

- **Change-hygiene gate**: `tools/diff-scan.py` reads a unified diff or the working tree and
  reports the eight classes of surprise D1-D8 - exec bit outside the tools allowlist, symlink,
  bidi/zero-width controls, confusable codepoints, PR-shaped workflow triggers, unpinned action
  refs, dependency-name similarity against the seed list, and manifest/lock drift. Ships with a
  17-case `--self-test`; `.githooks/pre-commit` runs it on the staged diff.
- **`tools/check.sh` grew from seven sections to ten**: the format gate (section 7, now
  `--staged`-aware) and the diff-scan tree scan plus its self-test (sections 8-9) run inside the
  same gate a reviewer reads.
- **`tools/gates-selftest.sh`**: one entry point runs every tool's own self-test (8/8 suites);
  `gates.yml` calls it instead of listing the self-tests inline.
- **PR template**: the claims ledger table maps every assertion in a PR body to a command and its
  printed output, and the checklist asks for a `diff-scan` read of the patch.

### Changed (diff-scan gate)

- `AGENTS.md`: commands matrix gained the gates-selftest and diff-scan rows; "six checks" became
  "eight checks".
- `docs/coding-standards.md` and `docs/testing-playbook.md` document the eight-checks tree and the
  exact self-test counts (rationale: a check that cannot detect its own violation is decoration).

### Added (Story 1.2, PR #2 - build system, both toolchains, ADR-0010 probes)

- **Build system**: `CMakeLists.txt` tree (root, `src/core`, `src/backends/null`,
  `src/app`, `src/cli`, `tests`, `docs`) consuming `generated/naming.cmake`;
  `CMakePresets.json` with `linux-core` (ASan/UBSan, `-Werror`), `win-cross-x64`
  (LLVM-MinGW, static runtime) and `windows-msvc` (CI parity) presets plus matching
  `build` and `test` presets.
- **Cross toolchain pinned**: `build/cmake/toolchain-llvm-mingw.cmake`,
  `third_party/toolchains/llvm-mingw.sha256` and the documented fetch-and-verify
  procedure in `docs/dev-environment.md`.
- **Style configuration**: `.clang-format`, `.clang-tidy` and `.clangd`, plus
  `tools/format-check.sh` (whole tree, `--self-test`) wired into CI and the hooks.
- **ADR-0010 probes**: `tools/win-probe/` with `d2d_probe`, `runtime_probe`, `abi_probe`
  and `gpu_probe`, and a `winprobe_test` aggregate target for CI.
- **Supply chain**: `conanfile.py` + `conan.lock` produced by `conan lock create`
  (ADR-0004); CI actions pinned by full commit sha (E-2); `jsonschema==4.10.3` and
  `cmake==3.31.6` pinned in CI (E-1).
- **CI matrix**: `gates` kept, plus `core-linux`, `windows-mingw-cross` and
  `windows-msvc`, all four jobs required on `main`-bound PRs.

### Changed (Story 1.2)

- `ADR-0010` assumption table retired in favour of measured results from the four probes.
- `docs/dev-environment.md` toolchain pins table now records cmake 3.31.6, clang-format
  and clang-tidy 18.1.3, Conan 2.32.0 and the installed LLVM-MinGW 20260812 build.
- `docs/kickoff.md` M0 exit criteria describe the four-job matrix.
- `AGENTS.md` debt rows 1 and 2 close: build system and toolchain-pinned CMake presets
  are in the tree.

### Fixed (Story 1.2, found on PR #5)

- `.gitignore` no longer swallows the checked-in `build/cmake/toolchain-llvm-mingw.cmake`
  (the source module is re-included; build outputs stay ignored).
- `windows-msvc` uses Ninja + the Developer PowerShell environment instead of the
  `Visual Studio 17 2022` generator, which `windows-latest` (now VS 2026) no longer matches;
  the `dumpbin` temp file moved off `/tmp` to `$env:RUNNER_TEMP`.
- `.clang-format`: `EmptyLineBeforeAccessModifier` set to `Always` (the `LogicalSection`
  value needs a newer clang-format than the pinned 18.1.3).

### Added (Story 1.1, PR #1 and PR #4)

- Gate suite: `tools/check.sh` with six gates and their self-tests, wired into the
  `gates` workflow; naming, sidecar, spec, language and docs checks, each with a
  self-test. Branch protection on `main` (required checks, linear history, no force
  push). The four toolchain pins below are named and blamed in `docs/dev-environment.md`.
