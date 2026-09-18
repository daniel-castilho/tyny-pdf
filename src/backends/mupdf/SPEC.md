# MuPDF Backend Capability Specification

## Requirements

### R13.1 The mupdf backend SHALL render a PDF page using the MuPDF engine.

Verification: unit:tests/contract/backend_contract.cc

### R13.2 The mupdf backend SHALL return deterministic pixels for the same page index.

Verification: unit:tests/contract/backend_contract.cc

## Out of scope

- actual PDF parsing (handled by MuPDF)
- font rendering (handled by MuPDF)
- annotation rendering (handled by MuPDF)
