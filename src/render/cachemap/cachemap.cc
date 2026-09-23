// Cachemap implementation — spatial tile query with budget awareness.
// src/render/cachemap/cachemap.cc
// No engine includes (ADR-0011 R-M10). Engine-free.

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/budget.h"
#include "pdfcore/render.h"
#include "pdfcore/status.h"

#define CACHEMAP_BUCKETS 1024

typedef struct cachemap_entry {
  int x;
  int y;
  uint32_t zoom;
  pc_tile* tile;
  struct cachemap_entry* next;
} cachemap_entry;

struct pc_cachemap {
  uint32_t width;
  uint32_t height;
  cachemap_entry** buckets;
  uint32_t bucket_count;
  pc_budget* budget;
  pc_tile_cache* tile_cache;  // For budget coordination
};

static uint32_t hash_coords(int x, int y, uint32_t zoom, uint32_t bucket_count) {
  // Simple spatial hash
  uint64_t key = ((uint64_t)(uint32_t)x << 32) | ((uint64_t)(uint32_t)y << 16) | zoom;
  // Knuth's multiplicative hash
  return (uint32_t)((key * 2654435761u) % bucket_count);
}

static pc_status make_status(uint32_t code, uint32_t detail_id, const char* detail) {
  pc_status st;
  st.size = sizeof(pc_status);
  st.code = code;
  st.detail_id = detail_id;
  st.detail = detail;
  return st;
}

pc_status pc_cachemap_create(uint32_t width, uint32_t height, pc_cachemap** out_map) {
  if (!out_map) {
    return make_status(PC_ERR_ARGUMENT, 0, "null argument");
  }
  *out_map = NULL;

  pc_cachemap* map = (pc_cachemap*)calloc(1, sizeof(pc_cachemap));
  if (!map) {
    return make_status(PC_ERR_MEMORY, 0, "allocation failed");
  }

  map->width = width;
  map->height = height;
  map->bucket_count = CACHEMAP_BUCKETS;
  map->buckets = (cachemap_entry**)calloc(CACHEMAP_BUCKETS, sizeof(cachemap_entry*));
  if (!map->buckets) {
    free(map);
    return make_status(PC_ERR_MEMORY, 0, "allocation failed");
  }

  *out_map = map;
  return make_status(PC_ERR_NONE, 0, NULL);
}

pc_status pc_cachemap_query(pc_cachemap* map, int x, int y, pc_tile** out_tile) {
  if (!map || !out_tile) {
    return make_status(PC_ERR_ARGUMENT, 0, "null argument");
  }
  *out_tile = NULL;

  uint32_t idx = hash_coords(x, y, 0, map->bucket_count);
  cachemap_entry* e = map->buckets[idx];
  while (e) {
    if (e->x == x && e->y == y && e->zoom == 0) {
      *out_tile = e->tile;
      return make_status(PC_ERR_NONE, 0, NULL);
    }
    e = e->next;
  }
  return make_status(PC_ERR_NONE, 0, "tile not found");
}

pc_status pc_cachemap_query_rect(pc_cachemap* map, int x, int y, uint32_t w, uint32_t h,
                                 pc_tile** out_tiles, uint32_t* out_count) {
  if (!map || !out_tiles || !out_count) {
    return make_status(PC_ERR_ARGUMENT, 0, "null argument");
  }
  *out_count = 0;

  // For now, simple iteration over all buckets (can be optimized with spatial index)
  // This is a minimal implementation for story 5.3
  uint32_t found = 0;
  for (uint32_t i = 0; i < map->bucket_count; i++) {
    cachemap_entry* e = map->buckets[i];
    while (e && found < 256) {  // Cap at 256 tiles per query
      if (e->x >= (int)x && e->x < (int)(x + w) && e->y >= (int)y && e->y < (int)(y + h)) {
        out_tiles[found++] = e->tile;
      }
      e = e->next;
    }
  }
  *out_count = found;
  return make_status(PC_ERR_NONE, 0, NULL);
}

pc_status pc_cachemap_insert(pc_cachemap* map, int x, int y, pc_tile* tile) {
  if (!map || !tile) {
    return make_status(PC_ERR_ARGUMENT, 0, "null argument");
  }

  uint32_t idx = hash_coords(x, y, 0, map->bucket_count);

  // Check if already exists
  cachemap_entry* e = map->buckets[idx];
  while (e) {
    if (e->x == x && e->y == y && e->zoom == 0) {
      e->tile = tile;
      return make_status(PC_ERR_NONE, 0, NULL);
    }
    e = e->next;
  }

  // Insert new entry
  cachemap_entry* new_entry = (cachemap_entry*)calloc(1, sizeof(cachemap_entry));
  if (!new_entry) {
    return make_status(PC_ERR_MEMORY, 0, "allocation failed");
  }
  new_entry->x = x;
  new_entry->y = y;
  new_entry->zoom = 0;
  new_entry->tile = tile;
  new_entry->next = map->buckets[idx];
  map->buckets[idx] = new_entry;

  return make_status(PC_ERR_NONE, 0, NULL);
}

void pc_cachemap_destroy(pc_cachemap* map) {
  if (!map)
    return;
  for (uint32_t i = 0; i < map->bucket_count; i++) {
    cachemap_entry* e = map->buckets[i];
    while (e) {
      cachemap_entry* next = e->next;
      free(e);
      e = next;
    }
  }
  free(map->buckets);
  free(map);
}