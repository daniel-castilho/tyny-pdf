// Tile cache implementation - stub for Epic 2 spike
// Real implementation will provide a deterministic tile cache with LRU eviction

#include <pdfcore/status.h>

#include "pdfcore/render.h"

pc_status pc_tile_cache_create(uint32_t max_tiles, pc_tile_cache** out_cache) {
  (void)max_tiles;
  if (!out_cache)
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null out_cache"};
  *out_cache = nullptr;
  return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "tile_cache not implemented"};
}

pc_status pc_tile_cache_get(pc_tile_cache* cache, uint32_t x, uint32_t y, pc_tile** out_tile) {
  (void)cache;
  (void)x;
  (void)y;
  if (!out_tile)
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null out_tile"};
  *out_tile = nullptr;
  return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "not implemented"};
}

pc_status pc_tile_cache_put(pc_tile_cache* cache, uint32_t x, uint32_t y, pc_tile* tile) {
  (void)cache;
  (void)x;
  (void)y;
  (void)tile;
  return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "not implemented"};
}

void pc_tile_cache_destroy(pc_tile_cache* cache) {
  (void)cache;
}