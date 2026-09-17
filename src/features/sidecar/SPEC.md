# Sidecar - capability specification

Status: drafted before implementation, as ADR-0002 requires. Requirements whose artefact is a
`unit:` test that does not exist yet are reported as `pending` by `tools/spec-check.py`; a delta
may not be declared finished while one of its requirements is pending. The four rows below that
cite `script:` are already enforced by `tools/sidecar-fmt.py`, green in the local gate as of
2026-09-16. CI runs them for the first time when this seed is pushed
(`.github/workflows/gates.yml`); nothing here has a CI history yet.

Document: the annotation sidecar (`<document>.tynypdf.json`). Design and rationale:
[`adr/0007-sidecar-format.md`](../../../adr/0007-sidecar-format.md).

## Requirements

### R1.1 The sidecar SHALL be byte-canonical: UTF-8 without BOM, LF endings, two-space indent, object keys in ascending code-point order, and no trailing whitespace.

Verification: script:tools/sidecar-fmt.py

### R1.2 When the same document is formatted twice, the tool SHALL produce identical bytes for any
input that parses.

Verification: script:tools/sidecar-fmt.py

### R1.3 The tool SHALL reject a file whose bytes differ from its canonical form, and SHALL rewrite
it in place without changing meaning when asked to fix it.

Verification: script:tools/sidecar-fmt.py

### R2.1 The sidecar SHALL be identified by file name only: `<document-basename>.tynypdf.json`, next to the document, and `tools/sidecar-fmt.py` SHALL treat exactly that suffix as a sidecar.

Verification: script:tools/sidecar-fmt.py

### R2.2 When a sidecar declares `format_version` greater than the reader supports, the reader SHALL load it read-only and show the reason.

Verification: unit:tests/unit/test_sidecar_reader.cc

### R2.3 The validator SHALL reject any annotation id that is not a 10-character lowercase RFC 4648
base32 string without padding.

Verification: unit:tests/unit/test_sidecar_ids.cc

## R3 Atomic writes

### R3.1 The writer SHALL create the sidecar by writing a temporary file in the same directory, flushing it, then renaming over the target.

Verification: unit:tests/unit/test_sidecar_writer.cc

### R3.2 While a `.tynypdf.lock` file for the document exists and is fresher than five minutes, the application SHALL refuse to write and say why.

Verification: unit:tests/unit/test_sidecar_lock.cc

## R4 Interop

### R4.1 When the sidecar contains a key the application does not know, the application SHALL preserve it unchanged on save.

Verification: unit:tests/unit/test_sidecar_unknown_keys.cc

### R4.2 The application SHALL NOT write a sidecar for a document it did not open.

Verification: unit:tests/unit/test_sidecar_writer.cc

## R5 Staleness

### R5.1 When the recorded page count differs from the document's, the application SHALL mark the sidecar stale and disable annotation writes until the user confirms.

Verification: unit:tests/unit/test_sidecar_staleness.cc

### R5.2 The application SHALL treat a changed document fingerprint as the strong signal and the modification time as the weak one, and SHALL report which fired.

Verification: golden:tests/golden/sidecar-stale-report.txt

## R6 Exclusions

### R6.1 The sidecar SHALL NOT contain view state (open page, zoom, sidebar, scroll position, window size).

Verification: unit:tests/unit/test_sidecar_schema.cc

### R6.2 The sidecar SHALL NOT contain the document's text, its page geometry, or any credential, including a signing passphrase.

Verification: unit:tests/unit/test_sidecar_schema.cc

## Out of scope

- The binary wire format used between the UI process and the render worker (a later ADR; decision
  D19 keeps JSON on disk and a flat binary only over the process boundary).
- Annotation formats owned by other applications (PDF annotations in the document itself,
  `<document>.pdf` in-place editing, XFA).
- Sync, conflict resolution and any network transport: ADR-0003's default-deny rule means this file
  never leaves the machine unless the user copies it.
- Cloud storage semantics: the sidecar is a file, and file-level backup tools own it.
- Automatic migration of third-party sidecar formats.
