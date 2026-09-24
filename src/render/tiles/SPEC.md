# Render Tiles (M1) — Tile Cache with LRU Eviction

Status: story 5.3 landed the cache; story 1.5 gave it a content-bearing caller
(the viewer loop, `src/app/viewer/SPEC.md`). The tile cache provides 256x256
tiles with LRU eviction, bounded by `pc_budget` from the core (ADR-0011
R-M8). Known defect, issue open: the put-replace branch in `tiles.cc`
double-frees the transferred payload; the viewer never reaches it (page flips
reset the cache) and no test pins it.

## Requirements

### R27.1 The render layer SHALL provide a tile cache with 256x256 tiles, LRU eviction, and a `max_tiles` ceiling read from `pc_budget.max_tiles` (0 = unlimited).

Verification: unit:tests/unit/test_tiles.cc

### R27.2 The tile cache SHALL support `get(x, y, zoom)` and `put(x, y, zoom, tile)` operations, with LRU eviction when `max_tiles` is exceeded.

Verification: unit:tests/unit/test_tiles.cc

### R27.3 The tile cache SHALL track per-tile RSS and enforce a cumulative `max_bytes` ceiling read from `pc_budget.max_bytes` (0 = unlimited); inserting a tile that would exceed the ceiling triggers LRU eviction until the budget is satisfied.

Verification: unit:tests/unit/test_tiles_budget.cc

## Out of scope

- Cachemap spatial query (handled by `src/render/cachemap/`, story 5.3)
- Engine-specific tile rendering (handled by backends, R-M4)
- DPI-aware tile scaling (story 5.4)
