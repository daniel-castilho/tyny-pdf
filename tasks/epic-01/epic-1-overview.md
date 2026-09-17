# Epic 1: Foundation - the repository becomes real (M0)

**Project:** tyny-pdf
**Context:** C++20 core with a C ABI (`pdfcore`), MuPDF behind `include/pdfcore/backend.h`,
Win32 + Direct2D presentation, LLVM-MinGW cross build from Linux, AGPL-3.0-or-later, hexagonal
seams enforced by `tools/` gates (ADR-0001, ADR-0002, ADR-0011).
**Goal:** turn the published repository into a tree that can be built, tested and measured, retire
the assumptions ADR-0010 states as unverified, and establish the baseline against SumatraPDF - so
that every later delta (D-1..D-6) starts from a green `main` with a working CI, not from a folder of
documents.

---

## Repo state (measured at execution time, not remembered)

Everything here was re-measured on the day Story 1.1 executes. The planning baseline measured a
different tree - `origin/main` at `9fc2d5291` with 24 files - and planning-time numbers are not
carried forward. What matters is the tree as it is today.

- **Tree parity holds exactly.** `git ls-tree -r --name-only origin/main` prints 48 paths, and a
  recursive `diff` between a clean clone of `main` (`0d64999c`) and the maintained workspace is
  empty in both directions. The published tree and the maintained tree are the same tree, which is
  the property Story 1.1 exists to lock (`docs/kickoff.md` section 11).
- **The gate is green in a clean clone of `main`, exit 0.** `sh tools/check.sh` ran inside that
  clone and printed all seven sections OK: lang 42 files; naming 33 keys / 2 generated files /
  1 retired token guarded; sidecar 1 checked / 0 problems; canonical 46 text files /
  0 problems; spec 1 spec / 14 requirements / 10 pending artefacts; docs 30 markdown files /
  0 problems; `check: all gates green`, `exit=0`.
- **CI has run and is green.** `GET /repos/daniel-castilho/tyny-pdf/actions/runs` lists run
  `35170901622` (head `94f1950`, event `push`) and run `35171544840` (head `0d64999`, event
  `push`), both workflow `gates`, conclusion `success`. "CI has never run" is no longer true of the
  docs-gates job; the build-matrix jobs arrive with PR #2.
- **No PR has merged yet.** `GET /repos/.../pulls?state=all` -> `[]`. The seed commits
  (`9fc2d52`, `94f1950`, `0d64999`) went straight to `main`, which bypassed ADR-0009. That
  deviation is recorded in `docs/lessons.md`, and Story 1.1 is this PR - the first under the flow.
- **`main` is not protected.** `GET /repos/.../branches/main/protection` returns HTTP 404
  (`Branch not protected`). Story 1.1 applies the six settings of `docs/git-workflow.md`
  `Branch protection` and pastes the JSON.
- **The five files the planning baseline called truncated.** That baseline compared `main` against a
  fuller kickoff copy and recorded `.gitignore` 364 B, `CHANGELOG.md` 3281, `docs/lessons.md`
  10201, `docs/coding-standards.md` 26777, `docs/kickoff.md` 23489. That copy is outside this
  repository, has no `git` history shared with `main`, and is not the tree this epic maintains. The
  maintained tree carries the sizes that are on `main` today (147, 1624, 6939, 26684, 23452), they
  are byte-identical to the published tree, and Story 1.1's criterion is parity with the maintained
  workspace - which now holds by measurement, not by recollection.

## Why this epic now?

- **The tripwire in `docs/kickoff.md` section 11 fired, and Story 1.1 defuses it.** Its PR #1 exit
  criterion says the copy must include the dotfiles, "because `cp -r kickoff/* .` does not,
  `rsync -a` does". The push that landed `main` first lost exactly those files plus the ADRs, so
  the public repository briefly advertised decisions it did not contain and a gate it failed to run
  (`docs/lessons.md` records the mechanism and the fix). Parity now holds by measurement.
- **Nothing downstream can be evidenced without a green, protected `main`.** The first merged PR is
  this story's evidence that the ADR-0009 flow works; the `gates` job is a required check before
  PR #2's build-matrix jobs can be trusted to run at all; and every later M0 story pastes its CI
  run id into `epic-1-dod.md`.
- **The baseline has an expiry date.** SumatraPDF 3.7 is in pre-release now; measuring 3.6.1 only
  (the original plan) would have set the bar at what users have rather than what they will get. M0.4
  measures both channels, so this epic is the last moment the comparison is cheap.

## Acceptance Criteria (grounded)

1. **`main` is green on its own terms:** `sh tools/check.sh` exit 0 on the merge commit of this
   epic, not only in the workspace, and `.github/workflows/gates.yml` present on `main`.
2. **Tree parity:** `git ls-tree -r --name-only main` contains all 48 maintained paths,
   byte-matching the maintained workspace; `docs-check` and `spec-check` report 0 problems.
3. **The first green CI runs exist and are pasted** (run ids `35170901622`, `35171544840` in
   `epic-1-dod.md`), and the `gates` job is a required check on `main` from this PR on.
4. **A deliberately broken fixture turns the docs job red** and is then reverted - the proof that
   the gate bites, demonstrated on a throwaway branch in this story.
5. **The flow is PR-only from this story on.** The seed's direct pushes are recorded as a deviation
   in `docs/lessons.md`; the six branch-protection settings of `docs/git-workflow.md` are applied,
   and `pulls?state=all` returns at least one merged PR after Story 1.1 lands.
6. **The build system exists and builds on both toolchains:** `cmake --preset linux-core` +
   `ctest --preset linux-core` green with ASan/UBSan, `cmake --preset win-cross-x64` produces
   `tynypdf.exe`, and the MSVC job is required, not optional (ADR-0010).
7. **ADR-0010's four assumptions are retired by evidence** (`tools/win-probe/`, planned, PR #2 per
   the debt list in `AGENTS.md`):
   D2D/DWrite/D3D11/DXGI headers compile, the static runtime links only system DLLs, one target
   builds under both toolchains, WSL interop reports a hardware adapter LUID - or the ADR records a
   downgrade with the failing output pasted.
8. **The bar is measured:** `tests/baseline.json` holds open/first-paint/scroll/search/RSS
   numbers for SumatraPDF 3.6.1 **and** 3.7 pre-release on the pinned corpus and the reference
   machine spec, reproducible within 10 % on a second run; `tests/conformance/` is a submodule
   pinned at a commit hash with the intake plan written (D13).
9. **Rule zero:** every number, sha or count in `epic-1-dod.md` is pasted from a command output
   included in that same document. A figure with no command above it is a hypothesis, labelled.

**Quick traceability:**

| Story | Reference doc                     | Key aspect                                              |
|-------|-----------------------------------|---------------------------------------------------------|
| 1.1   | `docs/kickoff.md` M0.1/M0.1b      | `main` parity, first green CI run, protection, PR flow  |
| 1.2   | ADR-0010, `docs/dev-environment.md` | build system, presets, both toolchains, win probes    |
| 1.3   | ADR-0004, ADR-0011 R-M10/R-M11    | vendored engine pinned, supply-chain and layering gates |
| 1.4   | ADR-0002, ADR-0003 section 1      | `pdfcore` C API, `null` backend, page 1 twice           |
| 1.5   | `docs/kickoff.md` M0.4/M0.5, D12/D13 | Sumatra baseline on both channels, corpus pinned     |

---

*Next step: run stories 1.1-1.5 (definitions in `epic-1-stories.md`) in the task order of
`epic-1-technical-tasks.md`, pasting evidence into `epic-1-dod.md` as each gate goes green.*
