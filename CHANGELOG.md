# Changelog

All notable changes to this project are documented in this file (AGENTS.md rule 11).
The format follows Keep a Changelog, and the versioning follows
CalVer-compatible SemVer as declared in [docs/release-runbook.md](docs/release-runbook.md).

## [Unreleased]

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
