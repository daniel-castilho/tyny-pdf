# Text Engine-Agnostic — NFKC, pt-BR, Caret over Combining Marks

## Requirements

### R16.1 The system SHALL normalize text to NFKC for search and anchor operations.

Verification: unit:tests/unit/test_text_normalize.cc

### R16.2 The system SHALL collapse whitespace and casefold during normalization.

Verification: unit:tests/unit/test_text_normalize.cc

### R16.3 The system SHALL compose base characters with combining marks (e.g., e + U+0301 -> U+00E9).

Verification: unit:tests/unit/test_text_normalize.cc

### R16.4 The system SHALL provide grapheme cluster break positions where each base character and each combining mark counts as one visual position.

Verification: unit:tests/unit/test_text_break.cc

### R16.5 The system SHALL treat `e\u0301` as a single grapheme cluster with caret positions at [0, 2].

Verification: golden:tests/golden/text-caret-positions.txt

### R16.6 The system SHALL treat `c\u0327` as a single grapheme cluster with caret positions at [0, 2].

Verification: golden:tests/golden/text-caret-positions.txt

### R16.7 The system SHALL treat `a\u0303o` as two grapheme clusters with caret positions at [0, 2, 4].

Verification: golden:tests/golden/text-caret-positions.txt

### R16.8 The system SHALL treat `cafe\u0301` as grapheme clusters with caret positions at [0, 1, 2, 3, 5].

Verification: golden:tests/golden/text-caret-positions.txt

### R16.9 The system SHALL move caret left/right over a combining sequence in one visual step.

Verification: unit:tests/unit/test_caret.cc

### R16.10 The system SHALL extract text runs from a page via `pc_doc_page_text` returning `pc_text_run` value types.

Verification: unit:tests/unit/test_text_runs.cc

### R16.11 The system SHALL return identical text bytes through `null` and `mupdf` backends for the same logical page.

Verification: unit:tests/contract/backend_contract.cc

## Out of scope

- Full Unicode grapheme cluster algorithm (UAX #29) — simplified for Latin + pt-BR only
- Hyphenation / syllable breaking — deferred to Epic 5
- Font shaping, ligatures, bidi — handled by render layer
- ICU or any external Unicode library dependency (R-M9)
