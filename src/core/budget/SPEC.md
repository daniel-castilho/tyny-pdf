# Core Budget (M1) — Memory Budget for Render & Transactions

Status: planned (story 5.3). The budget subsystem enforces memory ceilings
for the render tile cache and the transaction log (ADR-0011 R-M8).

## Requirements

### R29.1 The core SHALL provide a `pc_budget` struct with `max_tiles` and `max_bytes` ceilings, usable by both the transaction log and the render tile cache.

Verification: unit:tests/unit/test_budget.cc

### R29.2 The core SHALL enforce `max_tiles` and `max_bytes` ceilings in the transaction log: pushing a command that would exceed either ceiling answers `PC_ERR_LIMIT` with detail "budget exceeded" and leaves the IR untouched.

Verification: unit:tests/unit/test_txn_budget.cc

## Out of scope

- Tile cache eviction policy (handled by `src/render/tiles/`, story 5.3)
- Render-side budget enforcement (handled by `src/render/tiles/`, story 5.3)
- Engine-specific memory accounting (engine-free per R-M10)
