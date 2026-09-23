// Tile budget ceiling test — pc_budget is a POD value type; no cache
// symbols are linked by this target.
// R27.3
// tests/unit/test_tiles_budget.cc

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "pdfcore/budget.h"
#include "pdfcore/status.h"
#include "pdfcore/transaction.h"

int main(void) {
  int errors = 0;
  pc_status st;

  // POD budget: {size, max_tiles=2, max_bytes=2048}.
  pc_budget b = {sizeof(pc_budget), 2, 2048};

  // Tiles: 1 within ceiling -> NONE.
  st = pc_budget_check_tiles(&b, 1);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: 1 tile within ceiling\n");
    errors++;
  }
  // 2 at ceiling -> LIMIT.
  st = pc_budget_check_tiles(&b, 2);
  if (st.code != PC_ERR_LIMIT) {
    fprintf(stderr, "FAIL: 2 tiles at ceiling\n");
    errors++;
  }
  // 3 above -> LIMIT.
  st = pc_budget_check_tiles(&b, 3);
  if (st.code != PC_ERR_LIMIT) {
    fprintf(stderr, "FAIL: 3 tiles above ceiling\n");
    errors++;
  }
  // Null -> ARGUMENT.
  st = pc_budget_check_tiles(NULL, 1);
  if (st.code != PC_ERR_ARGUMENT) {
    fprintf(stderr, "FAIL: NULL budget tiles\n");
    errors++;
  }

  // Bytes: 1KB within, 2KB at ceiling.
  st = pc_budget_check_bytes(&b, 1024);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: 1KB within byte ceiling\n");
    errors++;
  }
  st = pc_budget_check_bytes(&b, 2048);
  if (st.code != PC_ERR_LIMIT) {
    fprintf(stderr, "FAIL: 2KB at byte ceiling\n");
    errors++;
  }
  st = pc_budget_check_bytes(NULL, 1);
  if (st.code != PC_ERR_ARGUMENT) {
    fprintf(stderr, "FAIL: NULL budget bytes\n");
    errors++;
  }

  pc_budget_destroy(&b);

  if (errors) {
    fprintf(stderr, "tiles_budget: FAIL (%d errors)\n", errors);
    return 1;
  }
  printf("tiles_budget: PASS\n");
  return 0;
}