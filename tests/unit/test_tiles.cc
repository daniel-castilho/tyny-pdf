// Tile cache round-trip — put/get growth without a budget attached.
// The cache without a budget grows (never evicts); eviction is exercised
// only by the budget-driven tests.
// R27.1 R27.2
// tests/unit/test_tiles.cc

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "pdfcore/render.h"
#include "pdfcore/status.h"
#include "pdfcore/transaction.h"

// Complete the opaque pc_tile type forward-declared by render.h.
struct pc_tile {
  uint32_t x;
  uint32_t y;
  uint32_t zoom;
  uint32_t ref_count;
  uint64_t size_bytes;
  uint64_t last_access;
  void* data;
};

static pc_tile* make_tile(uint32_t x, uint32_t y, uint64_t size_bytes) {
  pc_tile* tile = (pc_tile*)calloc(1, sizeof(pc_tile));
  tile->x = x;
  tile->y = y;
  tile->zoom = 0;
  tile->size_bytes = size_bytes;
  return tile;
}

int main(void) {
  int errors = 0;
  pc_status st;
  pc_tile_cache* cache = NULL;
  pc_tile* tile = NULL;

  st = pc_tile_cache_create(2, &cache);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: create\n");
    errors++;
  }
  if (!cache) {
    fprintf(stderr, "FAIL: cache null after create\n");
    errors++;
  }

  pc_tile* tiles[3];
  tiles[0] = make_tile(0, 0, 1024);
  tiles[1] = make_tile(1, 1, 1024);
  tiles[2] = make_tile(2, 2, 1024);

  st = pc_tile_cache_put(cache, 0, 0, tiles[0]);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: put(0,0)\n");
    errors++;
  }
  st = pc_tile_cache_put(cache, 1, 1, tiles[1]);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: put(1,1)\n");
    errors++;
  }
  st = pc_tile_cache_put(cache, 2, 2, tiles[2]);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: put(2,2)\n");
    errors++;
  }

  // Without a budget the cache grows: all three tiles survive.
  st = pc_tile_cache_get(cache, 0, 0, &tile);
  if (st.code != PC_ERR_NONE || !tile) {
    fprintf(stderr, "FAIL: get(0,0)\n");
    errors++;
  }
  tile = NULL;
  st = pc_tile_cache_get(cache, 1, 1, &tile);
  if (st.code != PC_ERR_NONE || !tile) {
    fprintf(stderr, "FAIL: get(1,1)\n");
    errors++;
  }
  tile = NULL;
  st = pc_tile_cache_get(cache, 2, 2, &tile);
  if (st.code != PC_ERR_NONE || !tile) {
    fprintf(stderr, "FAIL: get(2,2)\n");
    errors++;
  }

  // Miss: coordinate never put. Returns NONE with out_tile left NULL.
  tile = NULL;
  st = pc_tile_cache_get(cache, 9, 9, &tile);
  if (st.code != PC_ERR_NONE || tile != NULL) {
    fprintf(stderr, "FAIL: get miss should leave out NULL\n");
    errors++;
  }

  pc_tile_cache_destroy(cache);

  if (errors) {
    fprintf(stderr, "tiles: FAIL (%d errors)\n", errors);
    return 1;
  }
  printf("tiles: PASS\n");
  return 0;
}