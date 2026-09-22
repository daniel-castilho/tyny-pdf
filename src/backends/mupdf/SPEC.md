# MuPDF Backend Capability Specification

## Requirements

### R13.1 The mupdf backend SHALL render a PDF page using the MuPDF engine.

Verification: unit:tests/contract/backend_contract.cc

### R13.2 The mupdf backend SHALL return deterministic pixels for the same page index.

Verification: unit:tests/contract/backend_contract.cc

### R13.3 The mupdf backend SHALL declare `PC_CAP_FACE_COVERAGE` and answer `face_count`/`face_coverage` from a curated table of bundled fonts (MuPDF exposes no enumeration API), probing each face with `fz_lookup_builtin_font` + `fz_encode_character`.

Verification: unit:tests/unit/test_text_fallback.cc

## Out of scope

- actual PDF parsing (handled by MuPDF)
- font rendering (handled by MuPDF)
- annotation rendering (handled by MuPDF)
