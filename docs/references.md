# Engineering reference list

This file collects the engineering URLs that support the project's ADRs and
release process. It is intentionally short — a list is cheaper to verify than
memory (ADR-0011 R-M13).

## ADR-0004: Dependency management and supply chain

- upstream URL: `https://gitlab.freedesktop.org/mupdf/mupdf`
- upstream commit format: 40-hex recorded in `third_party/UPSTREAM.toml`
- patch series: each header carries `Subject`, `Reason`, `Upstream-status`, `Owner`
- decision D11: patch-report.sh fails on stale entries (>180 days)
- decision D13: screening not started (conformance corpus intake)
- Conan lockfile: `conanfile.py` + `conan.lock`

## ADR-0010: Build environment (WSL2)

- four toolchain claims: headers create
- toolchain: LLVM-MinGW 20260812, hash in third_party/toolchains/llvm-mingw.sha256
- installed at `~/.toolchains/llvm-mingw-20260812-ucrt-ubuntu-22.04-x86_64/`
- `tools/win-probe/` records the four ADR-0010 results

## docs/dev-environment.md

- toolchain pins: cmake 3.31.6 (pipx), clang-format/clang-tidy 18.1.3, conan 2.32.0
- LLVM-MinGW fetch+verify commands (curl+tar+sha256)
- one-time setup: `pipx install cmake==3.31.6 && pipx install conan==2.32.0`

## docs/release-runbook.md

- release process: publish SHA256SUMS, SHA256SUMS.asc (detached), SBOM, NOTICE
- binaries are not Authenticode-signed (decision D7b)
- release body states plainly that binaries are not Authenticode-signed

## tools/

- `tools/layering-check.sh` reports `backend_line_ratio` (ADR-0011 R-M6); fails >0.15
- `tools/deps-refresh.sh` refreshes lockfile and runs full suite (planned, PR #4)
- `tools/patch-report.sh` prints patch status table; fails on stale entries (D11)
- `tools/sbom.sh` generates CycloneDX SBOM from three signals (planned, PR #4)
- `tools/win-probe/` (ADR-0010) four probes + build.sh
