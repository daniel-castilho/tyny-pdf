// Tile cache implementation — 256x256 tiles with LRU eviction.
// src/render/tiles/tiles.cc
// No engine includes (ADR-0011 R-M10). Engine-free.

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/budget.h"
#include "pdfcore/render.h"
#include "pdfcore/status.h"
#include "pdfcore/transaction.h"

#define TILE_SIZE 256

typedef struct pc_tile {
  uint32_t x;
  uint32_t y;
  uint32_t zoom;
  uint32_t ref_count;
  uint64_t size_bytes;
  uint64_t last_access;  // LRU timestamp
  void* data;            // Opaque tile data (engine-specific)
} pc_tile;

struct pc_tile_cache {
  uint32_t max_tiles;
  uint64_t max_bytes;
  uint64_t current_bytes;
  uint32_t tile_count;
  uint64_t access_counter;  // Monotonically increasing for LRU
  pc_budget* budget;
  pc_tile** tiles;    // Array of tile pointers
  uint32_t capacity;  // Allocated capacity of tiles array
};

static uint64_t g_access_counter = 1;

static pc_status make_status(uint32_t code, uint32_t detail_id, const char* detail) {
  pc_status st;
  st.size = sizeof(pc_status);
  st.code = code;
  st.detail_id = detail_id;
  st.detail = detail;
  return st;
}

static void tile_destroy(pc_tile* tile) {
  if (!tile)
    return;
  if (tile->data) {
    free(tile->data);
  }
  free(tile);
}

static int tile_compare(const void* a, const void* b) {
  const pc_tile* ta = *(const pc_tile**)a;
  const pc_tile* tb = *(const pc_tile**)b;
  if (ta->last_access < tb->last_access)
    return -1;
  if (ta->last_access > tb->last_access)
    return 1;
  return 0;
}

static pc_status cache_evict_lru(pc_tile_cache* cache) {
  if (!cache || cache->tile_count == 0) {
    return make_status(PC_ERR_NONE, 0, NULL);
  }

  // Sort tiles by last_access (LRU first)
  qsort(cache->tiles, cache->tile_count, sizeof(pc_tile*), tile_compare);

  // Evict the LRU tile (first in sorted array)
  pc_tile* lru = cache->tiles[0];
  if (lru) {
    cache->current_bytes -= lru->size_bytes;
    cache->tile_count--;
    // Shift remaining tiles
    memmove(&cache->tiles[0], &cache->tiles[1], (cache->tile_count) * sizeof(pc_tile*));
    tile_destroy(lru);
  }
  return make_status(PC_ERR_NONE, 0, NULL);
}

static pc_status cache_enforce_budget(pc_tile_cache* cache) {
  if (!cache->budget)
    return make_status(PC_ERR_NONE, 0, NULL);

  // Check tile count budget
  while (cache->budget->max_tiles > 0 && cache->tile_count >= cache->budget->max_tiles) {
    pc_status st = cache_evict_lru(cache);
    if (st.code != PC_ERR_NONE)
      return st;
  }

  // Check byte budget
  while (cache->budget->max_bytes > 0 && cache->current_bytes >= cache->budget->max_bytes) {
    pc_status st = cache_evict_lru(cache);
    if (st.code != PC_ERR_NONE)
      return st;
  }

  return make_status(PC_ERR_NONE, 0, NULL);
}

pc_status pc_tile_cache_create(uint32_t max_tiles, pc_tile_cache** out_cache) {
  if (!out_cache) {
    return make_status(PC_ERR_ARGUMENT, 0, "null argument");
  }
  *out_cache = NULL;

  pc_tile_cache* cache = (pc_tile_cache*)calloc(1, sizeof(pc_tile_cache));
  if (!cache) {
    return make_status(PC_ERR_MEMORY, 0, "allocation failed");
  }

  cache->max_tiles = max_tiles;
  cache->max_bytes = 0;  // 0 = unlimited
  cache->budget = NULL;
  cache->capacity = max_tiles > 0 ? max_tiles : 64;  // Default capacity
  cache->tiles = (pc_tile**)calloc(cache->capacity, sizeof(pc_tile*));
  if (!cache->tiles) {
    free(cache);
    return make_status(PC_ERR_MEMORY, 0, "allocation failed");
  }

  *out_cache = cache;
  return make_status(PC_ERR_NONE, 0, NULL);
}

pc_status pc_tile_cache_get(pc_tile_cache* cache, uint32_t x, uint32_t y, pc_tile** out_tile) {
  if (!cache || !out_tile) {
    return make_status(PC_ERR_ARGUMENT, 0, "null argument");
  }
  *out_tile = NULL;

  for (uint32_t i = 0; i < cache->tile_count; i++) {
    pc_tile* t = cache->tiles[i];
    if (t->x == x && t->y == y && t->zoom == 0) {  // Zoom 0 for now
      t->last_access = g_access_counter++;
      t->ref_count++;
      *out_tile = t;
      return make_status(PC_ERR_NONE, 0, NULL);
    }
  }
  return make_status(PC_ERR_NONE, 0, "tile not found");
}

pc_status pc_tile_cache_put(pc_tile_cache* cache, uint32_t x, uint32_t y, pc_tile* tile) {
  if (!cache || !tile) {
    return make_status(PC_ERR_ARGUMENT, 0, "null argument");
  }

  // Check if tile already exists
  for (uint32_t i = 0; i < cache->tile_count; i++) {
    pc_tile* t = cache->tiles[i];
    if (t->x == x && t->y == y && t->zoom == 0) {
      // Update existing tile
      cache->current_bytes -= t->size_bytes;
      t->size_bytes = tile->size_bytes;
      t->data = tile->data;
      t->last_access = g_access_counter++;
      cache->current_bytes += t->size_bytes;
      tile_destroy(tile);  // Free the passed tile, we use our own
      return cache_enforce_budget(cache);
    }
  }

  // Resize array if needed
  if (cache->tile_count >= cache->capacity) {
    uint32_t new_cap = cache->capacity * 2;
    pc_tile** new_tiles = (pc_tile**)realloc(cache->tiles, new_cap * sizeof(pc_tile*));
    if (!new_tiles) {
      return make_status(PC_ERR_MEMORY, 0, "realloc failed");
    }
    cache->tiles = new_tiles;
    cache->capacity = new_cap;
  }

  // Add new tile
  tile->x = x;
  tile->y = y;
  tile->zoom = 0;
  tile->last_access = g_access_counter++;
  cache->tiles[cache->tile_count++] = tile;
  cache->current_bytes += tile->size_bytes;

  return cache_enforce_budget(cache);
}

void pc_tile_cache_destroy(pc_tile_cache* cache) {
  if (!cache)
    return;
  for (uint32_t i = 0; i < cache->tile_count; i++) {
    tile_destroy(cache->tiles[i]);
  }
  free(cache->tiles);
  free(cache);
}