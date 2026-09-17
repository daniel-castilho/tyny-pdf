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

## Repo state (pre-existing, not new work)

Measured on 2026-09-17 against `origin/main` at `9fc2d5291` and against the workspace seed.

- **The workspace seed is complete and green:** 42 files (47 with this epic's five documents),
  348747 bytes, `sh tools/check.sh` ->
  `check: all gates green` (exit 0). Eleven ADRs, `docs/{kickoff,naming,git-workflow,
  dev-environment,lessons,coding-standards,testing-playbook,release-runbook}.md`,
  `docs/sidecar.schema.json`, `src/features/sidecar/SPEC.md` (14 requirements, 4 already enforced,
  10 pending artefacts), `tests/fixtures/sidecar/example.tynypdf.json`, `LICENSE` (34523 bytes,
  sha256 `0d96a4ff68ad6d4b...`), and the six checks with their runner.
- **`origin/main` carries 24 files / 253883 bytes.** Missing entirely (18): all eleven `adr/*.md`,
  `src/features/sidecar/SPEC.md`, `tests/fixtures/sidecar/example.tynypdf.json`, and every dotfile -
  `.editorconfig`, `.gitattributes`, `.githooks/pre-commit`, `.github/PULL_REQUEST_TEMPLATE.md`,
  `.github/workflows/gates.yml`.
- **Five more files are stale or truncated on `main`:** `.gitignore` (81 bytes there, 364 here),
  `CHANGELOG.md` (1624 / 3281), `docs/lessons.md` (6939 / 10201), `docs/coding-standards.md`
  (26684 / 26777), `docs/kickoff.md` (23452 / 23489).
- **`main` fails its own gate.** The published tree, checked with the tools it does contain:

  ```
  $ sh tools/check.sh; echo "exit=$?"
  == 4/7 sidecar format (ADR-0007)
  tests/fixtures/sidecar: not a sidecar by name, skipped (see docs/naming.md)
  sidecar-fmt: OK (0 checked, 1 skipped, 0 problems)
  == 5/7 byte stability (.gitattributes, .editorconfig)
  .gitignore: no final newline
  canonical-check: 1 problem(s) in 23 file(s)
  exit=1
  ```

  plus `docs-check: 29 problems` (broken `adr/*.md` links and the unindexed ADR table) and
  `spec-check: 11 problems` ("R2.1 ... is cited outside a SPEC.md but defined nowhere").
- **CI has never run, for a mechanical reason:** `GET /repos/.../actions/runs` ->
  `"total_count": 0`. The workflow file is not on `main` to be triggered by.
- **No PR exists:** `GET /repos/.../pulls?state=all` -> `[]`; `9fc2d5291` sits directly on `main`
  on top of `05b62ec30`, so ADR-0009's branch-per-PR rule was bypassed for the seed itself.
- **Branch protection: unverifiable from here** (unauthenticated `GET /branches/main/protection`
  -> HTTP 401). It is an open item, not a claim.

## Why this epic now?

- **The tripwire in `docs/kickoff.md` section 11 fired.** Its PR #1 exit criterion says the copy
  must include the dotfiles, "because `cp -r kickoff/* .` does not, `rsync -a` does". The push
  lost exactly those files, plus the ADRs - so the public repository currently advertises
  decisions it does not contain and a licence GitHub now detects (`AGPL-3.0`) while the canon
  that owns them is absent.
- **Nothing downstream can be evidenced without it.** M1's spike needs `win-cross-x64` to produce a
  binary; D-6's undo needs `pc_txn_*` to exist; D-1's sidecar needs the fixture that `main` does not
  have; and the whole "our gates are real" claim needs one green run with a pastable run id.
- **The baseline has an expiry date.** SumatraPDF 3.7 is in pre-release now; measuring 3.6.1 only
  (the original plan) would have set the bar at what users have rather than what they will get. M0.4
  measures both channels, so this epic is the last moment the comparison is cheap.

## Acceptance Criteria (grounded)

1. **`main` is green on its own terms:** `sh tools/check.sh` exit 0 on the merge commit of this
   epic, not only in the workspace, and `.github/workflows/gates.yml` present on `main`.
2. **Tree parity:** `git ls-tree -r --name-only main` contains the 42 seed paths; the five truncated
   files match their workspace byte counts (364 for `.gitignore`); `docs-check` and `spec-check`
   report 0 problems on `main`.
3. **The first green CI run exists**, with the run id and head sha pasted in `epic-1-dod.md`, and
   the `gates` job is a required check.
4. **A deliberately broken fixture turns the docs job red** and is then reverted - the proof that
   the gate bites, which is PR #2's own exit criterion.
5. **The seed arrived through a PR** with the six branch-protection settings in
   `docs/git-workflow.md` ticked (`pulls?state=all` -> at least one merged PR).
6. **The build system exists and builds on both toolchains:** `cmake --preset linux-core` +
   `ctest --preset linux-core` green with ASan/UBSan, `cmake --preset win-cross-x64` produces
   `tynypdf.exe`, and the MSVC job is required, not optional (ADR-0010).
7. **ADR-0010's four assumptions are retired by evidence** (`tools/win-probe/`, planned; the debt
   list in `AGENTS.md` attributes it to PR #1, so that is the number used here):
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
