// Cachemap + budget — budget ceiling test independent of cache backend.
// R28.3
// tests/unit/test_cachemap_budget.cc

#include <stdio.h>

#include "pdfcore/budget.h"
#include "pdfcore/status.h"
#include "pdfcore/transaction.h"

int main(void) {
  int errors = 0;
  pc_status st;

  // POD budget: {size, max_tiles=2, max_bytes=2048}.
  pc_budget b = {sizeof(pc_budget), 2, 2048};

  // Tile ceiling checks.
  st = pc_budget_check_tiles(&b, 1);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "cachemap_budget FAIL: 1 tile within ceiling\n");
    errors++;
  }
  st = pc_budget_check_tiles(&b, 2);
  if (st.code != PC_ERR_LIMIT) {
    fprintf(stderr, "cachemap_budget FAIL: 2 tiles at ceiling\n");
    errors++;
  }

  // Byte ceiling checks.
  st = pc_budget_check_bytes(&b, 1024);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "cachemap_budget FAIL: 1KB within ceiling\n");
    errors++;
  }
  st = pc_budget_check_bytes(&b, 2048);
  if (st.code != PC_ERR_LIMIT) {
    fprintf(stderr, "cachemap_budget FAIL: 2KB at ceiling\n");
    errors++;
  }

  pc_budget_destroy(&b);

  if (errors) {
    fprintf(stderr, "cachemap_budget: FAIL (%d errors)\n", errors);
    return 1;
  }
  printf("cachemap_budget: PASS\n");
  return 0;
}