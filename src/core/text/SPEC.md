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

### R21.1 The core SHALL provide `pc_text_break_positions` that computes UAX #29 extended grapheme cluster boundaries for a UTF-8 string, returning an array of `pc_text_boundary` (byte_offset, cluster_index) starting at 0 and ending at string length.

Verification: unit:tests/unit/test_text_break.cc

### R21.2 The core SHALL NOT break between a base letter and a following combining mark (General_Category = Mn or Me) per UAX #29 rule GB9; the test corpus `tests/fixtures/text/ptbr-break-golden.txt` covers `e\u0301`, `a\u0303`, `o\u0301` as single clusters.

Verification: unit:tests/unit/test_text_break.cc

### R21.3 The core SHALL NOT break inside a pt-BR hyphenated compound when a hyphen (U+002D) is preceded by a letter: the hyphen and the following letter form one cluster with the preceding word (e.g., `guarda-chuva` → single cluster boundary around the hyphen). The test corpus `tests/fixtures/text/ptbr-break-golden.txt` covers this case.

Verification: unit:tests/unit/test_text_break.cc

### R21.4 The core SHALL treat the ABNT2 tilde-composition (`~` + `{a,o,n,A,O,N}`) as a single cluster and SHALL NOT treat a standalone `~` as composing. The test corpus `tests/fixtures/text/abnt2-golden.txt` covers `~a ~o ~n ~A ~O ~N` (each one cluster) and `~` alone (two boundaries).

Verification: unit:tests/unit/test_text_break.cc

### R23.1 The core SHALL provide `pc_caret_left` that, given a byte position, returns the nearest cluster boundary at or before that position (moves by whole clusters, never stops inside a combining sequence).

Verification: unit:tests/unit/test_caret.cc

### R23.2 The core SHALL provide `pc_caret_right` that, given a byte position, returns the nearest cluster boundary after that position (moves by whole clusters, never stops inside a combining sequence).

Verification: unit:tests/unit/test_caret.cc

### R23.3 The core SHALL report `PC_ERR_RANGE` when the byte position exceeds the string length, and `PC_ERR_ARGUMENT` for null arguments or invalid UTF-8.

Verification: unit:tests/unit/test_caret.cc

## Out of scope

- Per-glyph shaping or justification (Epic 5).
- Unicode normalization (NFKC) - the fixture keeps the exact byte sequence the document carries.
