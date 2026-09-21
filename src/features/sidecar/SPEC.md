# Sidecar - capability specification

Status: drafted before implementation, as ADR-0002 requires. Requirements whose artefact is a
`unit:` test that does not exist yet are reported as `pending` by `tools/spec-check.py`; a delta
may not be declared finished while one of its requirements is pending. The four rows below that
cite `script:` are already enforced by `tools/sidecar-fmt.py`, green in the local gate as of
2026-09-16. CI runs them for the first time when this seed is pushed
(`.github/workflows/gates.yml`); nothing here has a CI history yet.

Document: the annotation sidecar (`<document>.tynypdf.json`). Design and rationale:
[`adr/0007-sidecar-format.md`](../../../adr/0007-sidecar-format.md).

The authoritative requirements for the sidecar capability are defined in
`src/core/sidecar/SPEC.md`. This feature SPEC references that capability specification.

## Out of scope

- The binary wire format used between the UI process and the render worker (a later ADR;
  decision D19 keeps JSON on disk and a flat binary only over the process boundary).
- Annotation formats owned by other applications (PDF annotations in the document itself,
  `<document>.pdf` in-place editing, XFA).
- Sync, conflict resolution and any network transport: ADR-0003's default-deny rule
  means this file never leaves the machine unless the user copies it.
- Cloud storage semantics: the sidecar is a file, and file-level backup tools own it.
- Automatic migration of third-party sidecar formats.
