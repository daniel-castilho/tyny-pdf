# Render Cachemap (M1) — Spatial Tile Query

Status: story 5.3 landed the cachemap; story 1.5 gave it a content-bearing
caller (the viewer loop, `src/app/viewer/SPEC.md`). The cachemap provides
spatial indexing of tiles for visible-rect queries, working with the tile
cache (R27.1).

## Requirements

### R28.1 The render layer SHALL provide a cachemap that indexes tiles by their (x, y, zoom) coordinates and supports spatial queries: given a visible rectangle in tile coordinates, return the set of tiles that intersect it.

Verification: unit:tests/unit/test_cachemap.cc

### R28.2 The cachemap SHALL support `insert(x, y, zoom, tile)` and `query(visible_rect, zoom, out_tiles, out_count)` operations with O(log n) or better query time.

Verification: unit:tests/unit/test_cachemap.cc

### R28.3 The cachemap SHALL be budget-aware: when `pc_budget.max_tiles` or `max_bytes` is exceeded, it SHALL coordinate with the tile cache to evict LRU tiles that are not in the current visible rect.

Verification: unit:tests/unit/test_cachemap_budget.cc

## Out of scope

- Tile cache LRU eviction (handled by `src/render/tiles/`, story 5.3)
- Engine-specific tile rendering (handled by backends, R-M4)
- DPI-aware coordinate mapping (story 5.4)
