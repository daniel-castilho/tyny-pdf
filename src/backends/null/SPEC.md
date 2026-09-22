# Null Backend Capability Specification

## Requirements

### R11.1 The null backend SHALL render a fixed pattern for any page.

Verification: unit:tests/contract/backend_contract.cc

### R11.2 The null backend SHALL return deterministic pixels for the same page index.

Verification: unit:tests/contract/backend_contract.cc

### R11.3 The null backend SHALL declare no `PC_CAP_FACE_COVERAGE`; `face_count` SHALL answer 0 and `face_coverage` SHALL answer `PC_ERR_CAPABILITY` (never "nothing to cover", R-M5).

Verification: unit:tests/unit/test_text_fallback.cc

## Out of scope

- actual PDF parsing
- font rendering
- annotation rendering
