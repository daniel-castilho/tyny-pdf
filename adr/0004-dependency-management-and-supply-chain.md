# ADR-0004: Dependency management and supply chain

- Status: Accepted (ratified by the project owner, 2026-09-16)
- Date: 2026-09-16
- Deciders: project owner
- Related: ADR-0002, ADR-0005; `docs/dependency-policy.md`; decision D11, D13, D18

## Context

The dependency graph is unusual in three ways:

1. The engine, MuPDF, is a C library we will need to patch (build flags, allocator and lock
   overrides, font fallback, and possibly form and appearance fixes) and to upgrade on the
   upstream cadence. Vendoring and patch series matter more than package-manager convenience.
2. Support libraries (zlib, libjpeg-turbo, freetype, harfbuzz, openssl or a crypto provider)
   arrive transitively through the engine's own build, so a package manager that only handles
   direct dependencies will not tell the truth.
3. The trust story is the product. Binaries are published without a code-signing certificate for
   now (decision D7b), which raises the bar on everything else that is provable: pinned sources,
   published digests, SBOM, reproducible-ish builds.

The project is solo, so any process that requires a human to remember a step will not happen.

## Decision

### 1. Two mechanisms, with clear ownership

- **Engine: vendored, pinned, patched by series.** `third_party/mupdf/` is a git submodule
  pinned to an exact upstream commit recorded in `third_party/UPSTREAM.toml`
  (`upstream_url`, `upstream_commit`, `vendored_on`, `patch_series`). Local changes live in
  `third_party/patches/*.patch` as an ordered series, never as commits on a private branch, and
  each patch carries a header: `Subject`, `Reason`, `Upstream-status` (`none`,
  `proposed:<url>`, `merged:<commit>`), `Owner`.
- **Support libraries: Conan 2 with a committed lockfile.** `conanfile.py` plus `conan.lock`,
  one lockfile per release, updated only by `tools/deps-refresh.sh` (planned, PR #4), which opens a
  branch and
  runs the whole test and conformance suite before it may be merged. Conan is chosen over vcpkg
  for this project because it has lockfiles for reproducible pinning and a per-profile binary
  cache; vcpkg remains acceptable for a developer's local convenience but is not the source of
  truth, and mixing both in one project is prohibited.

### 2. Network is off at build time

CI will build with Conan in `--build=missing`-free, cache-first mode and a checksum-pinned fetch for
(added in PR #4, together with the vendored tree; no dependency exists yet, so no job can build):
the vendored engine tarball. No build step may resolve a floating version. The audit job is the
only one allowed to reach the network, and it does so through a pinned tool version.

### 3. Upstream-first, mechanically

A patch series entry is only allowed to remain when `Upstream-status` is `proposed` with a live
URL, or `merged` with a commit. `tools/patch-report.sh` (planned, PR #4) prints the table at release
time and
fails the build when an entry is older than 180 days without a status change. This is decision
D11 turned into a gate: we do not become a private fork of a PDF parser.

### 4. SBOM and vulnerability signal

`tools/sbom.sh` (planned, PR #4) generates CycloneDX at build time from three signals, because none
of them is
sufficient for C and C++ alone: the Conan lockfile, the linker inputs actually consumed
(collecting `-l` and library search paths and static archives), and a content fingerprint of the
vendored source against known upstream releases. The SBOM is attached to the release.
`osv-scanner` runs on the SBOM in a scheduled job, not in the PR gate, so that an unrelated
upstream advisory does not block a documentation fix; the job opens an issue and the response is
tracked in `docs/security/advisories.md`.

### 5. Build determinism aids verification (there is no signing yet)

- No `__DATE__` or `__TIME__` in any shipped binary. The build timestamp comes from a
  `buildinfo.json` written by CI and read at runtime, the pattern used by Sumatra's `BuiltOn`
  field.
- Source paths are canonicalised, file ordering is stable, and the compiler is pinned by hash in
  the container image tag used by CI.
- Published artifacts: `SHA256SUMS`, `SHA256SUMS.asc` (detached signature by the release
  maintainer's key, free), and the SBOM. The release body states plainly that the binaries are
  **not** Authenticode-signed, because an unstated gap is what a trust-oriented product cannot
  afford. See ADR-0005's honesty rule for claims.

### 6. Adding a dependency

`AGENTS.md` keeps the existing rule: a new Rust, C++ or Python dependency requires an explicit
human decision, recorded as `docs/dependency-policy.md` table row (name, version, license, size
cost, why not local code, removal cost). Third-party code is vendored only with its license file
and an `UPSTREAM.toml` entry.

## Enforcement

| Check | Command | Where |
| --- | --- | --- |
| Lockfile is current | regeneration must leave `git status` clean for `conan.lock` | CI |
| Engine pin is exact | `third_party/UPSTREAM.toml` must contain a 40-hex `upstream_commit` | CI |
| Patch discipline | `tools/patch-report.sh` (planned, PR #4) fails on missing or stale `Upstream-status` | CI, nightly |
| No floating versions | `grep -E "version=\"[^\"]*(latest\|\[)" conanfile.py` returns nothing | CI |
| SBOM exists and matches | artifact present in the release directory and hash recorded in `SHA256SUMS` | CI, release |
| No build timestamp in binaries | `strings` on shipped PEs must not contain the build date pattern | CI |

## Consequences

- Positive: an engine upgrade becomes a scripted, reviewable operation (bump pin, re-apply
  series, run conformance, update the patch report) instead of an archaeology project.
- Positive: the repository can state facts about what it contains, which is what the project's
  whole positioning depends on.
- Negative: Conan adds a Python toolchain requirement to onboarding; mitigated by
  `CMakePresets.json` plus a one-command bootstrap script, and by the fact that a contributor can
  build with the vendored engine only, without Conan, for pure-UI work.
- Negative: patch series maintenance is recurring overhead, and it is the price of not forking.
- Negative: an unsynchronised local Conan cache will produce a confusing first build on a
  contributor machine; the error message in the bootstrap script must name the fix.

## Alternatives rejected

- vcpkg manifest with a pinned baseline only. Rejected as source of truth: no lockfile means
  engine and support-library versions cannot be pinned to the granularity this project needs.
- Submodules for everything, no package manager. Rejected: hand-written recipes for zlib and
  friends are a permanent tax with no verification benefit.
- Conan for the engine too. Rejected: we need patches applied by series and an exact upstream
  commit, and a recipe hides that in build metadata.
