# third_party - dependency procedure

## Engine (MuPDF)

- The vendored engine lives in `third_party/mupdf/` as a git submodule.
- The exact upstream commit is recorded in `third_party/UPSTREAM.toml` (keys: `upstream_url`, `upstream_commit`, `vendored_on`, `patch_series`).
- Patches that modify the engine live in `third_party/patches/` as an ordered series. Each patch file must carry a header:
  - `Subject`: a short description
  - `Reason`: why this patch is needed (e.g. `proposed:<url>`, `merged:<commit>`)
  - `Upstream-status`: one of `none`, `proposed:<url>`, `merged:<commit>`
  - `Owner`: who applied or reviewed the patch
- The procedure to refresh or audit the dependencies is `tools/deps-refresh.sh` (planned, PR #4). It verifies the toolchain hash against `third_party/toolchains/llvm-mingw.sha256`, checks `UPSTREAM.toml`, and runs the full test and conformance suite.

## Support libraries

- `conanfile.py` plus `conan.lock` (produced by `conan lock create`) pins support-library versions.
- `tools/deps-refresh.sh` (planned, PR #4) refreshes the lockfile and runs the full suite before merge.

## Verification

- The build is reproducible only with the pinned toolchain: `third_party/toolchains/llvm-mingw.sha256` records the `bin/clang` hash.
- The `tools/patch-report.sh` (planned, PR #4) prints the patch status table and fails the build when an entry is older than 180 days without a status change (decision D11).