# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Kick-off seed: 11 ADRs (10 accepted, ADR-0006 superseded by ADR-0008), `docs/kickoff.md`,
  `AGENTS.md`, `docs/naming.md` as the single source of product identifiers,
  `docs/dev-environment.md`, `docs/git-workflow.md` and `docs/lessons.md`.
- `src/features/sidecar/SPEC.md`: the first living specification, 14 requirements in EARS form with
  traceable verification targets.
- Repository gates in `tools/`: `lang-check.py` (ADR-0005), `sidecar-fmt.py` (ADR-0007 canonical
  bytes), `naming-sync.py` (ADR-0006/0008), `spec-check.py` (R-M13), `canonical-check.sh`,
  `docs-check.py`, and `check.sh` to run the whole set.
- `.github/workflows/gates.yml` (gates plus the five tool self-tests),
  `.github/PULL_REQUEST_TEMPLATE.md` with the evidence trailer, `.githooks/pre-commit`,
  `.gitattributes` and `.editorconfig`.
- `README.md` as the project front page and `CHANGELOG.md`, so every relative link the front page
  uses resolves in the same commit that writes it.
- `docs-check.py` fails on an unbalanced code fence: an open fence used to hide the rest of the file
  from the length and link rules, which is how a mangled block passed review (11 self-test
  properties).
- `LICENSE`: AGPL-3.0-or-later, FSF text verbatim.

### Not started

No source code, no releases, no downloaded binaries. The reader/viewer starts at milestone M0 of
`docs/kickoff.md`.
