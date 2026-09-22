# CLI Capability Specification

## Requirements

### R12.1 The CLI SHALL render a specified page to a PNG file.

Verification: unit:tests/unit/test_cli_exit_codes.cc

### R12.2 The CLI SHALL return exit code 0 on success, 1 on corrupt file, 2 on argument error, 3 on unsupported.

Verification: unit:tests/unit/test_cli_exit_codes.cc

### R12.3 The CLI SHALL provide `txn replay <log.json> --out <out.json>` that deserializes a
canonical transaction log, replays it on a null-backend document, and writes canonical JSON
byte-identical to `pc_txn_to_json`. Exit 0 on success, 1 on a corrupt log, 2 on usage error
(ADR-0003 §6).

Verification: unit:tests/unit/test_txn_replay.cc

## Out of scope

- batch processing
- interactive mode
- text extraction
