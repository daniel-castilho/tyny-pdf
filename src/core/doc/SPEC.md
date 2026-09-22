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

### R16.1 The core SHALL expose `pc_doc_has_capability` (supported or unsupported per backend
capability) and `pc_doc_find_tables`, where an unsupported capability is reported as
`PC_ERR_CAPABILITY`, never as an empty list of tables (R-M5).

Verification: unit:tests/contract/backend_contract.cc

### R18.1 The core SHALL keep the transaction log as command value types (type + annotation id +
before/after rect) that hold no engine handle and mutate only the IR (R-M4, R-M8).

Verification: unit:tests/unit/test_txn_core.cc

### R18.2 The core SHALL provide `pc_txn_create` that borrows a `pc_doc` and an optional
`pc_budget` (0 fields = no ceiling), naming the allocator for `*out_txn`.

Verification: unit:tests/unit/test_txn_core.cc

### R18.3 The core SHALL provide `pc_txn_apply`, `pc_txn_undo` and `pc_txn_redo` so that the IR
after `apply; undo; redo` is byte-identical to the IR after `apply`, as measured by `pc_doc_hash`
(sha256 of page boxes and annotations).

Verification: unit:tests/unit/test_txn_core.cc

### R18.4 The core SHALL reject a command that would push the undo log past the `pc_budget`
tile or byte ceiling with `PC_ERR_LIMIT` and detail `"undo budget exceeded"`, leaving the IR
unchanged.

Verification: unit:tests/unit/test_txn_core.cc

## Out of scope

- Rendering (belongs in `src/render` and backend vtable).
- Text extraction (story 3.2).
- Sidecar read/write (stories 3.3/3.4).
- Annotation form rules and page anchoring (D-2, Epics 5/6).
- Transaction JSON round-trip lives in `src/core/json/SPEC.md` (R22.1-R22.3, story 4.2).

