# Annotation - capability specification

Status: drafted before implementation. Requirements whose artefact is a `unit:` test that does not
exist yet are reported as `pending` by `tools/spec-check.py`; a delta may not be declared finished
while one of its requirements is pending.

## Requirements

### R22.1 The system SHALL provide an annotation IR with type, page index, position, contents, author, dates, flags, color, opacity, and border width.

Verification: unit:tests/unit/test_annotation_api.cc

### R22.2 The system SHALL support creating annotations with auto-generated RFC 4648 base32 IDs (R2.3 compliant).

Verification: unit:tests/unit/test_annotation_api.cc

### R22.3 The system SHALL support adding, removing, and finding annotations by ID in the document IR.

Verification: unit:tests/unit/test_annotation_api.cc

### R22.4 The system SHALL support querying annotations by page index.

Verification: unit:tests/unit/test_annotation_query.cc

### R22.5 The annotation list SHALL persist across document close/reopen as part of the IR.

Verification: unit:tests/unit/test_annotation_persistence.cc

### R22.6 The system SHALL serialize annotations to canonical JSON for transaction log inclusion.

Verification: unit:tests/unit/test_annotation_json.cc

## Out of scope

- Rendering of annotations (belongs to render layer)
- PDF-level annotation import/export (belongs to backend)
- Annotation UI (belongs to viewer layer)
- Annotation reply threads (v2)

(End of file - total 46 lines)
