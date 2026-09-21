# Undo/Redo - capability specification

Status: drafted before implementation. Requirements whose artefact is a `unit:` test that does not
exist yet are reported as `pending` by `tools/spec-check.py`; a delta may not be declared finished
while one of its requirements is pending.

## Requirements

### R19.1 The system SHALL provide a transaction API `pc_txn_*` for creating, committing, and rolling back changes to a document.

Verification: unit:tests/unit/test_txn_api.cc

### R19.2 When a mutating operation on the document IR occurs (annotation add/edit/delete, form field change, redaction, text edit), the system SHALL record it as a transaction in the command log.

Verification: unit:tests/unit/test_txn_log.cc

### R19.3 The system SHALL support undo and redo to arbitrary depth, with `pc_txn_undo` and `pc_txn_redo`.

Verification: unit:tests/unit/test_txn_undo_redo.cc

### R19.4 When a document undergoes a sequence of N operations, the system SHALL produce byte-identical state after undo(N) followed by redo(N).

Verification: unit:tests/unit/test_txn_replay.cc

### R19.5 The CLI SHALL expose `tynypdf-cli txn replay` to replay a transaction log from a JSON file.

Verification: unit:tests/unit/test_txn_cli_replay.cc

### R20.1 The transaction log SHALL be stored in the document IR, not in the sidecar, and SHALL persist across document close/reopen.

Verification: unit:tests/unit/test_txn_persistence.cc

### R20.2 The transaction log SHALL be cleared on explicit `pc_txn_clear_log` or when the document is saved without changes.

Verification: unit:tests/unit/test_txn_log_clear.cc

### R21.1 The undo/redo mechanism SHALL operate purely on the IR in `src/core/doc` and SHALL NOT depend on the engine backend.

Verification: unit:tests/unit/test_txn_backend_independence.cc

### R21.2 The render layer (`src/render`) SHALL contain no undo logic.

Verification: unit:tests/unit/test_render_no_undo.cc

### R18.1 When a document receives 10,000 random operations, the system SHALL undo and redo to byte-identical state.

Verification: golden:tests/golden/txn_fuzz_10k.txt

## Out of scope

- Network-based undo or collaborative editing (ADR-0003 default-deny)
- Persisting the command log in the sidecar file (it lives in the IR)
- UI keyboard shortcuts (Ctrl+Z/Ctrl+Y) - those belong to the viewer layer
- Selective undo (undoing a specific past operation without undoing intermediate ones)
- Transaction log compression or garbage collection (v1 keeps full history in memory)

(End of file - total 67 lines)
