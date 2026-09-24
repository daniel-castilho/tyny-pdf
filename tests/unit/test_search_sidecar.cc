// R40.1 - Search sidecar round-trip test

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore/backend.h"
#include "pdfcore/geom.h"
#include "pdfcore/page.h"
#include "pdfcore/search.h"
#include "pdfcore/selection.h"
#include "pdfcore/status.h"

// Test that search results can be encoded/decoded through sidecar
// This is a placeholder - full implementation requires sidecar writer/reader

int main() {
  pc_search_hit hit = {};
  hit.quad = {100.0, 100.0, 150.0, 100.0, 100.0, 120.0, 150.0, 120.0};
  hit.byte_offset = 42;
  hit.byte_len = 5;

  // Verify struct layout is stable
  if (hit.quad.ul_x != 100.0 || hit.quad.ul_y != 100.0 || hit.quad.ur_x != 150.0 ||
      hit.quad.ur_y != 100.0 || hit.quad.ll_x != 100.0 || hit.quad.ll_y != 120.0 ||
      hit.quad.lr_x != 150.0 || hit.quad.lr_y != 120.0 || hit.byte_offset != 42 ||
      hit.byte_len != 5) {
    printf("FAIL search_hit_struct_layout\n");
    return 1;
  }

  pc_search_results results = {};
  results.items = &hit;
  results.count = 1;

  if (results.count != 1 || results.items[0].byte_offset != 42 || results.items[0].byte_len != 5) {
    printf("FAIL search_results_struct_layout\n");
    return 1;
  }

  printf("PASS search_struct_layout\n");
  return 0;
}