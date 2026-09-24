# Search Feature Specification

Status: story 6.3 implements page text search with highlight quads (R37.x),
CLI search subcommand (R38.x), budget-aware search (R39.x),
and sidecar persistence (R40.x).

## Requirements

### R37.1 The core SHALL provide `pc_search_page` that, given a UTF-8 query string, searches the page's text layout (R13.4) case-insensitively and returns all non-overlapping hits as quads + byte ranges, sorted by byte offset.

Verification: unit:tests/unit/test_search.cc

### R37.2 The search SHALL be case-insensitive for ASCII; a query "lorem" matches "Lorem" and "LOREM".

Verification: unit:tests/unit/test_search.cc

### R38.1 The CLI SHALL expose `tynypdf-cli search <pdf> <page> <query> <dpi>` that prints all hits as JSON array of quads + byte ranges.

Verification: unit:tests/unit/test_cli_search.cc

### R39.1 The search SHALL respect the per-document `pc_budget` (R29.x): the total memory for hits across all pages in a session SHALL NOT exceed `max_bytes`, returning PC_ERR_LIMIT when exceeded rather than allocating unbounded.

Verification: unit:tests/unit/test_search_budget.cc

### R40.1 The search results (page + byte ranges) SHALL round-trip through the sidecar without loss.

Verification: unit:tests/unit/test_search_sidecar.cc

## Out of scope

- Regex search (literal UTF-8 only)
- Cross-page search (single page per call; CLI iterates)
- Search-as-you-type incremental (batch only)
- Annotation authoring on search results (story 6.4)
