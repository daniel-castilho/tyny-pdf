# Core Document IR - capability specification

Status: drafted before implementation. Requirements whose artefact is a `unit:` test that does not
exist yet are reported as `pending` by `tools/spec-check.py`.

Document: the intermediate representation (IR) for a PDF document. Design and rationale:
[`adr/0011-modularity-rules.md`](../../../adr/0011-modularity-rules.md) R-M4 (no engine object
crosses a seam), R-M8 (IR owns semantics).

## Requirements

### R7.1 The core SHALL provide `pc_doc_open` that opens a document via the backend vtable,
copies page geometry into `pc_page_box` value types, closes the backend document handle,
and returns an opaque `pc_doc*` owning only the IR.

Verification: unit:tests/unit/test_doc_ir.cc

### R7.2 The core SHALL provide `pc_doc_page_count` returning the number of pages from the IR.

Verification: unit:tests/unit/test_doc_ir.cc

### R7.3 The core SHALL provide `pc_doc_page_get_box` returning the `pc_page_box` (MediaBox,
CropBox, rotation) for a given page index from the IR.

Verification: unit:tests/unit/test_doc_ir.cc

### R7.4 The core SHALL provide `pc_doc_sha256` returning the hex-encoded SHA256 of the source
file bytes for sidecar staleness detection.

Verification: unit:tests/unit/test_doc_ir.cc

### R7.5 The core SHALL provide `pc_doc_close` that frees the IR only, never engine memory.

Verification: unit:tests/unit/test_doc_ir.cc

### R7.6 The core SHALL ensure all IR value types are pure value types with no engine pointers.

Verification: unit:tests/unit/test_doc_ir.cc

## Out of scope

- Rendering (belongs in `src/render` and backend vtable).
- Text extraction (story 3.2).
- Sidecar read/write (stories 3.3/3.4).
- Undo/redo log (Epic 4).