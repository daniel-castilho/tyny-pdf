# MuPDF Backend Capability Specification

## Requirements

### R13.1 The mupdf backend SHALL render a PDF page using the MuPDF engine.

Verification: manual: implementation in progress

### R13.2 The mupdf backend SHALL return deterministic pixels for the same page index.

Verification: manual: implementation in progress

## Out of scope

- actual PDF parsing (handled by MuPDF)
- font rendering (handled by MuPDF)
- annotation rendering (handled by MuPDF)
- full implementation pending (currently stubbed with PC_ERR_UNSUPPORTED)
