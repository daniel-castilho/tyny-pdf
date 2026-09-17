# third_party - dependency procedure

## Engine (MuPDF)
- Submodule in `third_party/mupdf/`.
- Upstream commit in `third_party/UPSTREAM.toml`.

## Support libraries
- `conanfile.py` + `conan.lock` pins versions.
- `tools/deps-refresh.sh` (planned, PR #4) refreshes lockfile and runs suite.

## Verification
- Hash: `third_party/toolchains/llvm-mingw.sha256`.
- `tools/patch-report.sh` (planned, PR #4) fails on stale entries (>180 days, D11).
