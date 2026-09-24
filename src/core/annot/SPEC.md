# Annotation Feature Specification

Status: story 6.4 implements annotations as undoable transactions (R41.x),
CLI annotation subcommands (R42.x), sidecar round-trip (R43.x),
and re-anchoring on document change (R44.x).

## Requirements

### R41.1 The core SHALL provide `pc_annot_cmd_add_from_selection` that creates a highlight/underline/strikethrough/squiggly annotation from a `pc_selection_result` and color, pushing a `PC_CMD_ANNOT_ADD` command to the transaction log (pc_txn) with the annotation geometry and payload.

Verification: unit:tests/unit/test_annot_txn.cc

### R41.2 The core SHALL provide `pc_annot_cmd_delete` that deletes an annotation by ID, pushing a `PC_CMD_ANNOT_DELETE` command to the transaction log.

Verification: unit:tests/unit/test_annot_txn.cc

### R41.3 The transaction log SHALL support undo/redo of annotation add/modify/delete operations. Undo of ADD removes the annotation; redo re-adds it. Undo of DELETE restores the annotation; redo deletes it again.

Verification: unit:tests/unit/test_annot_txn.cc

### R42.1 The CLI SHALL expose:
- `tynypdf-cli annot add <pdf> <page> <x> <y> <dpi> <type>
  <r> <g> <b>` — creates annotation from hit-test at point
- `tynypdf-cli annot delete <pdf> <annot_id>` — deletes annotation by ID
- `tynypdf-cli annot list <pdf> <page>` — lists annotations as JSON

Verification: unit:tests/unit/test_cli_annot.cc

### R43.1 The core SHALL export annotations to sidecar JSON and import them by replaying the transaction log, reconstructing the exact same annotation state without loss.

Verification: unit:tests/unit/test_annot_sidecar.cc

### R44.1 The core SHALL re-anchor annotations to the nearest matching text by byte offset plus fuzzy text match when document text changes, with a confidence score; annotations below the threshold SHALL be flagged as "detached" but not lost.

Verification: unit:tests/unit/test_annot_reanchor.cc

## Out of scope

- Free-text annotations with rich formatting (story 6.4+)
- Link annotations with actions (story 6.4+)
- Annotation replies/threads (story 6.4+)
- Stamp/widget annotations (story 6.4+)
