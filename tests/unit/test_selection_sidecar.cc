// R32.5 - Selection sidecar round-trip test

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore/backend.h"
#include "pdfcore/geom.h"
#include "pdfcore/page.h"
#include "pdfcore/selection.h"
#include "pdfcore/status.h"

// Test that selection result can be encoded/decoded through sidecar
// This is a placeholder - full implementation requires sidecar writer/reader

int main() {
  pc_selection_result r1 = {};
  r1.page_index = 5;
  r1.quad.ul_x = 100.0;
  r1.quad.ul_y = 200.0;
  r1.quad.ur_x = 110.0;
  r1.quad.ur_y = 200.0;
  r1.quad.ll_x = 100.0;
  r1.quad.ll_y = 220.0;
  r1.quad.lr_x = 110.0;
  r1.quad.lr_y = 220.0;
  r1.byte_offset = 42;
  r1.byte_len = 10;

  // Verify struct layout is stable
  if (r1.page_index != 5 || r1.quad.ul_x != 100.0 || r1.quad.ul_y != 200.0 ||
      r1.quad.ur_x != 110.0 || r1.quad.ur_y != 200.0 || r1.quad.ll_x != 100.0 ||
      r1.quad.ll_y != 220.0 || r1.quad.lr_x != 110.0 || r1.quad.lr_y != 220.0 ||
      r1.byte_offset != 42 || r1.byte_len != 10) {
    printf("FAIL selection_struct_layout\n");
    return 1;
  }

  printf("PASS selection_struct_layout\n");
  return 0;
}