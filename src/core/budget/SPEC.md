# Budget — Resource Limits in the Core

## Requirements

### R17.1 The system SHALL provide a `pc_budget` value type with `max_rss_mb`, `max_tiles`, `max_sidecar_bytes` fields.

Verification: unit:tests/unit/test_budget.cc

### R17.2 The system SHALL provide `pc_budget_default()` returning defaults: 250 MB RSS, 64 tiles, 4 MB sidecar.

Verification: unit:tests/unit/test_budget.cc

### R17.3 The system SHALL provide `pc_budget_check()` returning `PC_ERR_LIMIT` when tile count exceeds budget.

Verification: unit:tests/unit/test_budget.cc

### R17.4 The system SHALL provide `pc_budget_check()` returning `PC_ERR_LIMIT` when RSS exceeds budget.

Verification: unit:tests/unit/test_budget.cc

### R17.5 The system SHALL return `PC_ERR_ARGUMENT` for null budget pointer.

Verification: unit:tests/unit/test_budget.cc

## Out of scope

- Enforcement of budget by render layer — that is Epic 4
- Dynamic budget adjustment — fixed at startup for v1
- GPU memory budget — CPU-only for v1
