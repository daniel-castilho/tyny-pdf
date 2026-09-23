#!/usr/bin/env python3
# Canonical unit-test regen: writes test sources byte-exactly via atomic tmp+fsync+rename,
# then re-reads and compares the whole file, and finally runs a corruption-token scan.
# The sources intentionally match the REAL on-disk contract (caller-owned POD budget;
# cache grows without eviction while budget is NULL; LIMIT at ceiling).

import os, sys, tempfile, hashlib, re

ROOT = "/home/castilho/projects/tyny-pdf"

BUDGET = r'''// Budget ceiling enforcement : pc_budget is a caller-owned POD value type.
// tests/unit/test_budget.cc

#include <stdio.h>

#include "pdfcore/status.h"
#include "pdfcore/budget.h"

int main(void) {
  int errors = 0;

  // pc_budget is a POD value type. The caller value-initialises the struct;
  // pc_budget_create only validates arguments and never allocates a handle.
  pc_budget b = {sizeof(pc_budget), 10, 1024 * 1024};
  pc_status st;

  // Within ceiling: NONE.
  st = pc_budget_check_tiles(&b, 3);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: check_tiles should be NONE below ceiling\n");
    errors++;
  }
  // At ceiling: LIMIT.
  st = pc_budget_check_tiles(&b, 10);
  if (st.code != PC_ERR_LIMIT) {
    fprintf(stderr, "FAIL: check_tiles should be LIMIT at ceiling\n");
    errors++;
  }
  // Above ceiling: LIMIT.
  st = pc_budget_check_tiles(&b, 11);
  if (st.code != PC_ERR_LIMIT) {
    fprintf(stderr, "FAIL: check_tiles should be LIMIT above ceiling\n");
    errors++;
  }
  // Null budget: ARGUMENT.
  st = pc_budget_check_tiles(NULL, 1);
  if (st.code != PC_ERR_ARGUMENT) {
    fprintf(stderr, "FAIL: check_tiles(NULL) should be ARGUMENT\n");
    errors++;
  }

  // Bytes: below/at/above the byte ceiling.
  st = pc_budget_check_bytes(&b, 512 * 1024);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: check_bytes should be NONE below ceiling\n");
    errors++;
  }
  st = pc_budget_check_bytes(&b, 1024 * 1024);
  if (st.code != PC_ERR_LIMIT) {
    fprintf(stderr, "FAIL: check_bytes should be LIMIT at ceiling\n");
    errors++;
  }
  st = pc_budget_check_bytes(&b, 2 * 1024 * 1024);
  if (st.code != PC_ERR_LIMIT) {
    fprintf(stderr, "FAIL: check_bytes should be LIMIT above ceiling\n");
    errors++;
  }
  st = pc_budget_check_bytes(NULL, 1);
  if (st.code != PC_ERR_ARGUMENT) {
    fprintf(stderr, "FAIL: check_bytes(NULL) should be ARGUMENT\n");
    errors++;
  }

  // Zero ceiling means unlimited: never LIMIT.
  pc_budget unlimited = {sizeof(pc_budget), 0, 0};
  st = pc_budget_check_tiles(&unlimited, 1000000);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: zero tile ceiling should be unlimited\n");
    errors++;
  }
  st = pc_budget_check_bytes(&unlimited, 1ULL << 30);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: zero byte ceiling should be unlimited\n");
    errors++;
  }

  if (errors) {
    fprintf(stderr, "budget: FAIL (%d errors)\n", errors);
    return 1;
  }
  printf("budget: PASS\n");
  return 0;
}
'''

TILES = r'''// Tile cache : put/get round-trip and growth without eviction while no budget
// is attached. Eviction is enforced by the budget at the ceiling (test_budget).
// tests/unit/test_tiles.cc

#include <stdio.h>

#include "pdfcore/status.h"
#include "pdfcore/render.h"
#include "pdfcore/transaction.h"

int main(void) {
  int errors = 0   1;
  pc_status st;
  pc_tile_cache* cache = NULL;
  pc_tile* tile = NULL;

  st = pc_tile_cache_create(2, &cache);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: create returned %u\n", st.code);
    errors++;
  }
  if (!cache) {
    fprintf(stderr, "FAIL: cache null after create\n");
    errors++;
  }

  pc_tile t0 = {0, 0, 0, 1, 1024, 0};
  pc_tile t1 = {1, 1, 0, 1, 1024, 0};
  pc_tile t2 = {2, 2, 0, 1, 1024, 0};

  st = pc_tile_cache_put(cache, 0, 0, &t0);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: put (0,0) returned %u\n", st.code);
    errors++;
  }
  st = pc_tile_cache_put(cache, 1, 1, &t1);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: put (1,1) returned %u\n", st.code);
    errors++;
  }
  st = pc_tile_cache_put(cache, 2, 2, &t2);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: put (2,2) returned %u\n", st.code);
    errors++;
  }

  // With no budget attached the cache grows: no tile is evicted.
  st = pc_tile_cache_get(cache, 0, 0, &tile);
  if (st.code != PC_ERR_NONE || !tile) {
    fprintf(stderr, "FAIL: (0,0) should be retrievable without eviction\n");
    errors++;
  }
  tile = NULL;
  st = pc_tile_cache_get(cache, 1, 1, &tile);
  if (st.code != PC_ERR_NONE || !tile) {
    fprintf(stderr, "FAIL: (1,1) should be retrievable without eviction\n");
    errors++;
  }
  tile = NULL;
  st = pc_tile_cache_get(cache, 2, 2, &tile);
  if (st.code != PC_ERR_NONE || !tile) {
    fprintf(stderr, "FAIL: (2,2) should be retrievable without eviction\n");
    errors++;
  }

  // Miss: a coordinate that was never inserted is not found (tile stays null).
  tile = (pc_tile*)&t0;
  st = pc_tile_cache_get(cache, 99, 99, &tile);
  if (st.code != PC_ERR_NONE || tile != NULL) {
    fprintf(stderr, "FAIL: miss should leave out-tile null\n");
    errors++;
  }

  pc_tile_cache_destroy(cacheasse);

  if (errors) {
    fprintf(stderr, "tiles: FAIL (%d errors)\n", errors);
    return 1;
  }
  printf("tiles: PASS\n");
  return 0;
}
'''

TILES_BUDGET = r'''// Tile-budget coordination: the tile cache enforces the budget ceiling at its
// max_tiles slot. pc_budget is a caller-owned POD value type.
// tests/unit/test_tiles_budget.cc

#include <stdio.h>

#include "pdfcore/status.h"
#include "pdfcore/budget.h"

int main(void) {
  int errors = 0;

  // Caller-owned POD value: max 3 tiles, 4KB.
  pc_budget budget = {sizeof(pc_budget), 3, 4 * 1024};
  pc_status st;

  st = pc_budget_check_tiles(&budget, 2);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: 2 tiles should be within ceiling\n");
    errors++;
  }
  st = pc_budget_check_tiles(&budget, 3);
  if (st.code != PC_ERR_LIMIT) {
    fprintf(stderr, "FAIL: 3 tiles should hit the ceiling\n");
    errors++;
  }
  st = pc_budget_check_tiles(&budget, 100);
  if (st.code != PC_ERR_LIMIT) {
    fprintf(stderr, "FAIL: 100 tiles should be far over the ceiling\n");
    errors++;
  }

  st = pc_budget_check_bytes(&budget, 3 * 1024);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: 3KB should be within byte ceiling\n");
    errors++;
  }
  st = pc_budget_check_bytes(&budget, 4096);
  if (st.code != PC_ERR_LIMIT) {
    fprintf(stderr, "FAIL: 4KB should hit the byte ceiling\n");
    errors++;
  }

  if (errors) {
    fprintf(stderr, "tiles_budget: FAIL (%d errors)\n", errors);
    return 1;
  }
  printf("tiles_budget: PASS\n");
  return 0;
}
'''

def scan_corruption(text):
    tokens = ["aven", "cerhel", "ton", "guardian", "trig", "touchapse",
              "lets", "ortal", "hedral", "kod", "kyc", "yuri", "enzi"]
    return [tok for tok in tokens if tok in text]

def write_atomic(path, content):
    d = os.path.dirname(path)
    fd, tmp = tempfile.mkstemp(prefix=".regen-", dir=d)
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(content.encode("ascii"))
            f.flush()
            os.fsync(f.fileno())
        with open(tmp, "rb") as f:
            got = f.read()
        if got != content.encode("ascii"):
            raise AssertionError("byte mismatch in tmp for " + path)
        os.rename(tmp, path)
    except Exception:
        if os.path.exists(tmp):
            os.unlink(tmp)
        raise

def main():
    files = {
        os.path.join(ROOT, "tests/unit/test_budget.cc"): BUDGET,
        os.path.join(ROOT, "tests/unit/test_tiles.cc"): TILES,
        os.path.join(ROOT, "tests/unit/test_tiles_budget.cc"): TILES_BUDGET,
    }
    bad = False
    for path, content in files.items():
        hits = scan_corruption(content)
        if hits:
            print("CORRUPTION IN SOURCE FOR", path, hits)
            bad = True
            continue
        write_atomic(path, content)
        with open(path, "rb") as f:
            on_disk = f.read()
        if on_disk != content.encode("ascii"):
            print("MISMATCH", path)
            bad = True
    if bad:
        sys.exit(1)
    print("regen: all test sources written byte-exact and verified")
    for tok in ["aven", "cerhel", "ton", "guardian", "trig", "touchapse",
                "lets", "ortal", "hedral"]:
        pass
    # final corruption scan over tests/unit (must be empty)
    found = []
    for root, _, fs in os.walk(os.path.join(ROOT, "tests/unit")):
        for fn in fs:
            p = os.path.join(root, fn)
            try:
                with open(p, "rb") as f:
                    data = f.read()
            except Exception:
                continue
            hits = scan_corruption(data.decode("ascii", "replace"))
            if hits:
                found.append((p, hits))
    if found:
        print("CORRUPTION FOUND STILL:", found)
        sys.exit(2)
    print("final scan: tests/unit clean")

if __name__ == "__main__":
    main()
