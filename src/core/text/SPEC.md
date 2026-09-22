# Core Text Fallback (D-4, ADR-0011 R-M8)

Status: implemented (story 4.3). Requirements are verified by `tests/unit/test_text_fallback.cc`.

## Requirements

### R20.1 The core SHALL provide `pc_text_fallback_runs` that splits a UTF-8 string into maximal runs of codepoints covered by a single backend face, choosing the first face in the backend's declaration order that draws each codepoint.

Verification: unit:tests/unit/test_text_fallback.cc

### R20.2 When a codepoint has no covering face, the core SHALL return `PC_ERR_LIMIT` with detail `"missing glyph U+XXXX"` (uppercase hex) and SHALL emit no partial runs.

Verification: unit:tests/unit/test_text_fallback.cc

### R20.3 The core SHALL report `PC_ERR_CAPABILITY` when the backend does not declare `PC_CAP_FACE_COVERAGE` face coverage or declares zero faces, and SHALL report `PC_ERR_ARGUMENT` for null arguments or invalid UTF-8.

Verification: unit:tests/unit/test_text_fallback.cc

### R20.4 The core SHALL free returns of `pc_text_fallback_runs` via the named `pc_text_run_free`, and SHALL keep `src/core/text` free of any engine include.

Verification: unit:tests/unit/test_text_fallback.cc

## Out of scope

- Per-glyph shaping or justification (Epic 5).
- Line breaking and caret movement (`src/core/text/break.cc`, `caret.cc` - story 4.4).
- Unicode normalization (NFKC) - the fixture keeps the exact byte sequence the document carries.
