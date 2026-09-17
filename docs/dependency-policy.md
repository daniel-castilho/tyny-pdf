# Dependency Policy — Tyny PDF

**Owner of this file:** the project owner. ADR-0004 is the decision; this file is the operating
procedure that ADR-0004 section 6 points at: *"a new Rust, C++ or Python dependency requires an
explicit human decision, recorded as `docs/dependency-policy.md` table row (name, version, license,
size cost, why not local code, removal cost)"*. Nothing here re-decides what ADR-0004 decided. It
makes those decisions executable, and it names the places where the tree does not yet match them.

## 0. Executable status

Measured in this tree on 2026-09-17, so that the document cannot be mistaken for a description of
running infrastructure:

```
$ for p in conanfile.py conan.lock third_party NOTICE docs/security docs/references.md
> do printf '%-24s %s\n' "$p" "$(test -e "$p" && echo present || echo absent)"; done
conanfile.py             absent
conan.lock               absent
third_party              absent
NOTICE                   absent
docs/security            absent
docs/references.md       absent
CMakeLists.txt           absent
```

Three consequences follow from that block rather than from taste:

- **There is no dependency graph yet.** No third-party library is present, so every row of section
   2 is a decision about a future PR, not an inventory. A row that reads `planned` must not be
   quoted as installed anywhere else in the repository.
- **No rule here is enforced by a tool yet.** The four enforcing scripts (`tools/deps-refresh.sh`,
   `tools/patch-report.sh`, `tools/sbom.sh`, `tools/layering-check.sh`) are planned with PR #4, per
   `docs/kickoff.md` section 11 item 4; the build matrix that would run them arrives with PR #2,
   which owns `CMakeLists.txt`, the presets and the style configs.
- **The one dependency present today lives in CI, not in the product**, and section 8 lists it as
   an open exception with its line number.

## 1. The rule

1. **A dependency enters with a row in section 2, in the same PR as the first code that consumes
    it.** The row is the decision record: name, version or pin, licence, size cost, why not local
    code, removal cost. A PR that adds a `#include <something/>` without that row is closed rather
    than argued with: ADR-0004 section 6 and `AGENTS.md` rule 7 (ADR-0011 R-M9) both require a human
    decision for a dependency, a build flag or a toolchain, and this file is where it is written.
2. **If the row cannot be filled, the answer is no.** "We did not measure the removal cost" is not
    a pending state; it is a rejection with extra steps.
3. **Vendored code arrives with its licence file**, unchanged text, beside the source it covers
    (ADR-0004 section 6, last sentence). A vendored tree without a licence file is a red gate, not a
    missing nicety.
4. **Nothing floats.** Section 4 says what "pinned" means for each kind of artefact. A version
    range, a branch name, a tag without a hash or a `latest` is an unpinned dependency wearing a
    costume.
5. **Build-time network is off** (ADR-0004 section 2). The audit job is the only one allowed to
    reach the network, and it runs a pinned tool version.
6. **Mixing package managers in one project is prohibited** (ADR-0004 section 1). Conan 2 with a
    committed lockfile is the source of truth; vcpkg is acceptable locally and irrelevant to CI.
7. **An unassigned gap is a decision, not a default.** Where a document references a file that no
    PR owns, the file is either assigned here (section 8) or the reference is removed. Silent
    "someone will do it" is how `third_party/toolchains/` and `NOTICE` stand today.

## 2. The register

The columns are the ones ADR-0004 section 6 names. `Size cost` is what a dependency adds to the
shipped artefact, in the units the baseline harness will measure (exe bytes, peak-RSS delta); `n/m`
means not measurable before the build exists, which is honest where an estimate would be
decorative.

| Name | Version / pin | Licence | Size cost | Why not local code | Removal cost | Status, and where it is enforced |
|---|---|---|---|---|---|---|
| MuPDF (the engine) | exact upstream commit in `third_party/UPSTREAM.toml` (`upstream_url`, `upstream_commit`, `vendored_on`, `patch_series`) | AGPL-3.0-or-later, licence text shipped | n/m | A PDF parser is the one component where "write it ourselves" is the expensive option; the engine also carries rendering, layout and form code that would take years to reproduce | High, and priced in: the seam is `include/pdfcore/backend.h`, so the cost is a second backend rather than a rewrite (ADR-0011 R-M10) | planned, PR #4 (vendored tree); ADR-0004 section 1 |
| zlib, libjpeg-turbo, freetype, harfbuzz, a crypto provider | whatever one `conan.lock` records per release | zlib / IJG / FTL-BSD / MIT / per provider | n/m | they arrive transitively through the engine's own build (ADR-0004 context item 2), so the lockfile is the record, not a decision | Low per library; medium as a set, because the engine's build script names them | not present; enters only as rows produced by `tools/deps-refresh.sh` (planned, PR #4) |
| GoogleTest | a version the lockfile or a vendored release tag pins, settled with PR #2 | BSD-3-Clause | none in shipped artefacts (test-only) | an assertion framework is a distraction we can afford exactly once, and not now | Low - the suites are portable; `tests/contract` is plain C++ with its own main | planned: the job is PR #2's matrix (`docs/kickoff.md` M0 exit criterion 2 names gtest inside `linux-core`), the first target it executes is PR #5's |
| LLVM-MinGW (cross toolchain) | tarball, SHA-256 recorded in `third_party/toolchains/llvm-mingw.sha256`, version pinned in `CMakePresets.json` | Apache-2.0 with LLVM exception; a build tool, nothing linked into the product | none | a pinned compiler is the only way two machines can agree about a binary, which is what the unsigned-release posture buys us (ADR-0004 section 5, ADR-0010) | Medium - a toolchain bump rewrites every object file | planned; the pin file has no owner yet, exception E-4 in section 8 |
| MSVC v143 + Windows SDK | whatever `windows-latest` carries, recorded per run rather than pinned by us | proprietary, runner-supplied | none | parity is the requirement: the product must build with the compiler our users' toolchains expect (ADR-0010) | n/a - a second configuration, not a dependency | runner available today; the job is planned with PR #2 |
| `d2d1`, `dwrite`, `d3d11`, `dxgi` import libs | OS version, recorded in `buildinfo.json` | system, not redistributed | none | they are the operating system, not a library; linking them is the presentation design (ADR-0010) | n/a | ADR-0010; the probe that shows the headers are usable at our pin is `tools/win-probe/` (planned, PR #1 per the debt list in `AGENTS.md`) |
| Python `jsonschema` (CI only) | **not pinned today**: `.github/workflows/gates.yml:26` runs `pip install --quiet jsonschema` | MIT | none (not shipped) | validating `docs/sidecar.schema.json` and the SBOM is a five-line call over a several-hundred-line spec | Low - a hand-written subset check replaces it if the pin ever hurts | present in CI; exception E-1 |
| `actions/checkout@v4`, `actions/setup-python@v5` | tags, not full commit shas | MIT / BSD-3-Clause | none | runner plumbing; writing our own checkout is not a saving | Low | present at `.github/workflows/gates.yml:19` and `:21`; exception E-2 |
| `osv-scanner` | a pinned version in a scheduled job (ADR-0004 section 4) | Apache-2.0 | none | advisories have to be looked up by someone, and re-implementing a vulnerability feed is not our edge | Low - the SBOM stays valid and the job can be swapped | planned, PR #4, and deliberately **not** in the PR gate, so an unrelated advisory cannot block a documentation fix |
| veraPDF (conformance tooling) | a pinned release, decided when the corpus repository exists | MPL-2.0 | n/a - not linked into the product | a validity claim about ISO 32000 needs an independent implementation or it is marketing | Low | planned; `tests/conformance/` is absent and stays absent until the corpus exists (`docs/kickoff.md` M0.5) |
| A code-signing certificate | none | n/a | n/a | decision D7b: publish unsigned binaries with the gap stated, instead of a certificate that changes what users must trust | n/a | ADR-0004 section 5; `docs/release-runbook.md` repeats that the release body must say the binaries are not Authenticode-signed |

## 3. Licences, and what "compatible" means here

The repository is AGPL-3.0-or-later: `LICENSE` is the FSF text verbatim, 34523 bytes, sha256
`0d96a4ff68ad6d4b...`. Because the shipped artefact is a single-file executable with a static
runtime, "we only link it dynamically" is not available as an argument, so the table below is
stricter than a shared-library project would need to be.

| Upstream licence | Default answer | Why |
|---|---|---|
| MIT, BSD-2/3-Clause, ISC, zlib, IJG JPEG, SGI Free Software B | **yes** | permissive, compatible, and the licence text is a paragraph to ship |
| Apache-2.0 | **yes**, with its NOTICE and attribution obligations discharged in `NOTICE` | compatible with AGPL-3.0; the obligation is paperwork, not a code change |
| MPL-2.0 | **yes** at file scope, with a written note whenever a file is modified | file-level copyleft; the MPL clause that makes it GPL-compatible covers our licence |
| LGPL-2.1, LGPL-3.0 | **no** by default | a static single-file artefact cannot offer relinking, which is what those licences ask for |
| GPL-2.0-only | **no** by default | it lacks the "or later" that would let an AGPL-3.0 combined work satisfy it |
| GPL-3.0, AGPL-3.0 | **yes**, with vendoring, the licence file and a `NOTICE` entry | compatible; the cost is disclosure hygiene, and this project sells disclosure, so it must be exemplary |
| Field-of-use or competitive restrictions (SSPL, BUSL, Elastic-2.0, "non-commercial" clauses) | **no** | a limit on who may use the code contradicts the licence promise in `README.md` |
| A commercial arrangement for the engine, if one is ever taken | owner decision, recorded as a row here and as an ADR | it changes what the project may promise about licensing, so it cannot arrive in a chore commit |

Three mechanics follow from the table:

- Every vendored path gets its licence text in the same commit as its source, and `NOTICE` gains a
   paragraph naming the component, the licence and the upstream commit read from `UPSTREAM.toml`.
- A licence that is "probably fine" gets a note in the row saying who decided, on what date and on
   what reading - the alternative is a future reader re-litigating it from memory.
- A bundled font or data file, whether it arrives with the engine or with us, carries a
   distribution licence: the text goes in the tree and a `NOTICE` paragraph names it. Checking a
   font like a library is cheap; explaining a takedown is not.

## 4. What "pinned" means, by kind

| Kind | Accepted pin | Never accepted |
|---|---|---|
| Git submodule (the engine) | a 40-hex commit sha recorded in `third_party/UPSTREAM.toml` | a branch name, a tag name, a date |
| Source tarball (toolchain, fonts, data) | SHA-256 recorded in the tree beside the URL | size, filename, "the one from the mirror" |
| Support libraries (Conan) | a committed `conan.lock`, one per release | ranges in `conanfile.py`, `[>1.2]`, `latest` |
| CI actions | the full commit sha, with the human-readable version in a trailing comment | `@v4` alone, `@main` |
| Python in CI | `package==X.Y.Z`, and `--require-hashes` where the extra upkeep is accepted | a bare `pip install package` |
| Compiler and SDK | recorded per run in `buildinfo.json`, with the runner image tag | silence |
| Engine patches | one file per patch in `third_party/patches/`, in order | hand edits inside the submodule, squash-commits to a private branch |

The ADR's enforcement line stays a grep until the build exists: `grep -E
"version=\"[^\"]*(latest|\[)" conanfile.py` must return nothing (ADR-0004, Enforcement table). Note
what this table also settles for prose elsewhere in the repository: a claim about a version that
does not name its source is a floating version in a different medium.

## 5. Refresh and upgrade

- `tools/deps-refresh.sh` (planned, PR #4) is the **only** path that changes `conan.lock`. It opens
   a branch, runs the whole test and conformance suite, and may not be merged red. Re-running it
   must leave `git status` clean, which is the lockfile-currency check in ADR-0004's Enforcement
   table.
- A refresh PR that also touches product code is split in two. A reviewer needs "the version moved"
   and "the behaviour moved" as separate facts, because only the first one is this policy's
   business.
- The engine upgrade is the scripted sequence ADR-0004 lists as a positive consequence: bump the
   pin, re-apply the patch series, run conformance, update the patch report. An upgrade that needs
   hand edits in `third_party/mupdf/` is an incident, not a technique: it means the series is not
   the record.
- A security-only bump travels the same pipeline with one addition: the advisory reference in the
   PR body, so the release notes can cite it without archaeology.

## 6. Patches to upstream

A patch is one `git am`-applicable file in `third_party/patches/*.patch` carrying the header
ADR-0004 section 1 names: `Subject`, `Reason`, `Upstream-status` (`none`, `proposed:<url>`,
`merged:<commit>`), `Owner`. `tools/patch-report.sh` (planned, PR #4) prints the table at release
time and fails on an entry older than 180 days without a status change.

The part that is easy to get wrong, written out: **`Upstream-status: none` is a timer, not a
state.** A patch still `none` at 180 days is sent upstream, dropped, or - if it is permanently ours
- moved out of the engine into `src/`, where its tests and its ownership are visible. "It is small,
we keep it" is how a project becomes a private fork of a PDF parser, which is the outcome decision
D11 exists to prevent.

## 7. Not dependencies, treated as dependencies anyway

R-M9 in ADR-0011 covers build flags and toolchains for the same reason it covers libraries: they
change what ships, and nobody notices.

- `-fno-exceptions -fno-rtti` on `src/core` and `src/backends/**`, and the `PC_TRY`/`PC_CATCH`
   bridge in `src/backends/mupdf/exception_bridge.h` (ADR-0003 section 2). Changing one is a
   dependency change and needs a row.
- `~/.wslconfig` on the development host (`memory=12GB`, `processors=8`, `swap=0`, recorded in
   `docs/dev-environment.md`) and `ninja`'s parallelism: these decide whether a sanitizer link
   finishes, so a report that says "the build fails" without them is incomplete.
- The CI interpreter (`python-version: '3.12'`) and the runner image: recorded per run, and
   required next to any number in a document (`docs/testing-playbook.md` section 3.8 asks for the
   machine spec beside the measurement).
- A header-only helper counts too. Three vendored headers are still a licence, a pin and a removal
   cost.

## 8. Open exceptions in this tree

Each row cites the file and line that show the gap, and the mechanism that closes it. Section 2's
`planned` rows plus these exceptions are the whole honest state of the graph today.

| Id | The gap, as measured | Closes with |
|---|---|---|
| E-1 | `python3 -m pip install --quiet jsonschema` installs an unpinned version on every run (`.github/workflows/gates.yml:26`) | PR #2, which rewrites this workflow around the build matrix: `jsonschema==<X.Y.Z>`, plus `--require-hashes` if the upkeep is accepted |
| E-2 | CI actions are pinned by tag at `:19` and `:21`, so a force-push upstream changes what our gate builds | PR #2, same edit: full commit shas with the version as a comment |
| E-3 | `docs/dev-environment.md:27` bootstraps with `pipx install conan \|\| python3 -m pip install --user conan`, no version | PR #2, once a `conan.lock` exists to pin against: `conan==<X.Y.Z>` |
| E-4 | `third_party/toolchains/llvm-mingw.sha256` is the comparison target of the documented verification command (`docs/dev-environment.md:40`) and does not exist; no item in `docs/kickoff.md` section 11 owns it | an owner decision recorded here. PR #2 is where `CMakePresets.json` pins the version, so the hash file belongs with it. Until it is assigned, that documented command has nothing to compare against and must never be reported as a pass |
| E-5 | `docs/security/advisories.md` is referenced by ADR-0004 section 4 and is not in the tree | PR #4, with the scheduled `osv-scanner` job; until then the sentence about where advisory responses are tracked describes an intention |
| E-6 | `NOTICE` and `docs/references.md` do not exist; they are deliverables of Epic 1 story 1.3 | Epic 1, story 1.3 (`docs/epics/epic-1-technical-tasks.md`) |
| E-7 | ADR-0004 cites decision ids (D11, D13, D18) that live in the maintainer's log outside this repository, where a reader of the public tree cannot resolve them | not a defect to repair: it is why a row here must be self-contained and restate the reason instead of quoting an id |

## 9. Adding a dependency: the form to copy

```markdown
| <name> | <pin: 40-hex / sha256 / ==X.Y.Z> | <SPDX id> | <exe bytes, RSS delta> |
| <why not local code, in one sentence a reviewer can disagree with> |
| <removal cost: what breaks, and how many files> |
| <PR, and the owner decision with its date> |
```

In the same PR: the licence text beside the vendored source, the `NOTICE` paragraph, the
`UPSTREAM.toml` or lockfile entry, and a line in `CHANGELOG.md`. If any of those five is missing
the PR is not ready, and "the gate does not check that yet" is a statement about the gate - section
0 already conceded it - not a permission.

## 10. Removal

- A dependency whose row nobody can defend in review is removed rather than re-documented. The
   register keeps a tombstone row for one release, so the next person to propose it reads why the
   last one stopped.
- Removal cost is estimated at intake precisely so that removal stays possible; a row that says
   "unknown" has not done its job (rule 2).
- Dropping a transitive dependency is a lockfile change, so it travels `tools/deps-refresh.sh`
   (planned, PR #4) and the full suite exactly like an addition.

---

*Sources: ADR-0004 (the decision this file executes), ADR-0010 (toolchain pins), ADR-0011 R-M9 and
R-M10/R-M11, ADR-0003 section 2 (the bridge that keeps flags honest), `docs/kickoff.md` section 11
(what each PR owns), `docs/dev-environment.md` (host pins), `docs/release-runbook.md` (what a
release must publish), `docs/epics/epic-1-overview.md` (where this lands in the schedule).*
