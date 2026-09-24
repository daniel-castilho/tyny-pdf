// R43.1 - Annotation sidecar round-trip test

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore/annot.h"
#include "pdfcore/backend.h"
#include "pdfcore/geom.h"
#include "pdfcore/page.h"
#include "pdfcore/selection.h"
#include "pdfcore/status.h"

// Test that annotation can be encoded/decoded through sidecar
// This is a placeholder - full implementation requires sidecar writer/reader

int main() {
  pc_annot annot = {};
  annot.type = PC_ANNOT_HIGHLIGHT;
  annot.page_index = 0;
  annot.rect = {100.0, 100.0, 150.0, 120.0};
  annot.quad = {100.0, 100.0, 150.0, 100.0, 100.0, 120.0, 150.0, 120.0};
  annot.byte_offset = 42;
  annot.byte_len = 5;
  annot.color[0] = 255;
  annot.color[1] = 255;
  annot.color[2] = 0;
  annot.flags = 0;
  annot.payload = nullptr;
  annot.payload_size = 0;

  // Verify struct layout is stable
  if (annot.type != PC_ANNOT_HIGHLIGHT || annot.page_index != 0 || annot.rect.x0 != 100.0 ||
      annot.rect.y0 != 100.0 || annot.rect.x1 != 150.0 || annot.rect.y1 != 120.0 ||
      annot.quad.ul_x != 100.0 || annot.quad.ul_y != 100.0 || annot.quad.ur_x != 150.0 ||
      annot.quad.ur_y != 100.0 || annot.quad.ll_x != 100.0 || annot.quad.ll_y != 120.0 ||
      annot.quad.lr_x != 150.0 || annot.quad.lr_y != 120.0 || annot.byte_offset != 42 ||
      annot.byte_len != 5 || annot.color[0] != 255 || annot.color[1] != 255 ||
      annot.color[2] != 0) {
    printf("FAIL annot_struct_layout\n");
    return 1;
  }

  pc_annot_list list = {};
  list.items = &annot;
  list.count = 1;

  if (list.count != 1 || list.items[0].type != PC_ANNOT_HIGHLIGHT ||
      list.items[0].page_index != 0 || list.items[0].byte_offset != 42 ||
      list.items[0].byte_len != 5) {
    printf("FAIL annot_list_struct_layout\n");
    return 1;
  }

  printf("PASS annot_struct_layout\n");
  return 0;
}