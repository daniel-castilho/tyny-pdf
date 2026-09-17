# Null Backend Capability Specification

## Requirements

### R11.1 The null backend SHALL render a fixed pattern for any page.

Verification: unit:tests/contract/backend_contract.cc

### R11.2 The null backend SHALL return deterministic pixels for the same page index.

Verification: unit:tests/contract/backend_contract.cc

## Out of scope

- actual PDF parsing
- font rendering
- annotation rendering
