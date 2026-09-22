# Core JSON Canonical Serialization (ADR-0007)

Status: implemented (story 4.2). Requirements are verified by `tests/unit/test_txn_replay.cc`.

## Requirements

### R22.1 The core SHALL provide `pc_json_serialize` that serializes a JSON value tree to canonical form (keys sorted, 2-space indent, LF, 3 decimal places for doubles, no trailing whitespace).

Verification: unit:tests/unit/test_txn_replay.cc

### R22.2 The core SHALL provide `pc_json_parse` that parses canonical JSON into a value tree.

Verification: unit:tests/unit/test_txn_replay.cc

### R22.3 The core SHALL provide `pc_txn_to_json` and `pc_txn_from_json` for transaction log round-trip.

Verification: unit:tests/unit/test_txn_replay.cc

## Out of scope

- Full JSON schema validation.
- General-purpose JSON parser beyond the canonical format required by this epic.
