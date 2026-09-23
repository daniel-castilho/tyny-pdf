// Cachemap test — spatial insert/query/overwrite and miss behavior.
// R28.1 R28.2
// tests/unit/test_cachemap.cc

#include <stdio.h>
#include <stdlib.h>

#include "pdfcore/budget.h"
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
  // Create cachemap (20x20 cells)
  pc_cachemap* map = NULL;
  pc_status st_create = pc_cachemap_create(20, 20, &map);
  if (st_create.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: pc_cachemap_create returned %u\n", st_create.code);
    return 1;
  }
  if (!map) {
    fprintf(stderr, "FAIL: map is null after create\n");
    return 1;
  }

  pc_tile* tiles_to_free[2] = {NULL, NULL};
  int tile_count = 0;

  // Insert (5,7) -> tile
  pc_tile* tile = make_tile(5, 7, 1024);
  tiles_to_free[tile_count++] = tile;
  pc_status st_ins = pc_cachemap_insert(map, 5, 7, tile);
  if (st_ins.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: pc_cachemap_insert returned %u\n", st_ins.code);
    return 1;
  }

  // Query it back
  pc_tile* out = NULL;
  pc_status st_q = pc_cachemap_query(map, 5, 7, &out);
  if (st_q.code != PC_ERR_NONE || !out) {
    fprintf(stderr, "FAIL: pc_cachemap_query failed\n");
    return 1;
  }
  if (out->x != 5 || out->y != 7) {
    fprintf(stderr, "FAIL: returned tile has wrong coords\n");
    return 1;
  }

  // Overwrite same cell
  pc_tile* tile2 = make_tile(5, 7, 2048);
  tiles_to_free[tile_count++] = tile2;
  pc_status st_ov = pc_cachemap_insert(map, 5, 7, tile2);
  if (st_ov.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: pc_cachemap_insert (overwrite) returned %u\n", st_ov.code);
    return 1;
  }
  pc_tile* out2 = NULL;
  pc_status st_o2 = pc_cachemap_query(map, 5, 7, &out2);
  if (st_o2.code != PC_ERR_NONE || !out2 || out2->size_bytes != 2048) {
    fprintf(stderr, "FAIL: overwrite did not replace tile\n");
    return 1;
  }

  // Query a cell that was never inserted -> miss (null tile, PC_ERR_NONE)
  pc_tile* out_miss = NULL;
  pc_status st_miss = pc_cachemap_query(map, 42, 42, &out_miss);
  if (st_miss.code != PC_ERR_NONE || out_miss != NULL) {
    fprintf(stderr, "FAIL: miss should return null tile\n");
    return 1;
  }

  pc_cachemap_destroy(map);

  // Free all tiles we allocated (the cachemap doesn't own them)
  for (int i = 0; i < tile_count; i++) {
    free(tiles_to_free[i]);
  }

  printf("cachemap: PASS\n");
  return 0;
}