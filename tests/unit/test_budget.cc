// Budget ceiling test — pc_budget is a POD value type (caller-owned).
// R29.1 R29.2
// tests/unit/test_budget.cc

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "pdfcore/budget.h"
#include "pdfcore/status.h"
#include "pdfcore/transaction.h"

int main(void) {
  int errors = 0;
  pc_status st;

  // POD value: {size, max_tiles=5, max_bytes=2048}.
  pc_budget b = {sizeof(pc_budget), 5, 2048};

  // Below ceiling -> NONE.
  st = pc_budget_check_tiles(&b, 4);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: 4 tiles within ceiling\n");
    errors++;
  }
  // At ceiling -> LIMIT.
  st = pc_budget_check_tiles(&b, 5);
  if (st.code != PC_ERR_LIMIT) {
    fprintf(stderr, "FAIL: 5 tiles at ceiling\n");
    errors++;
  }
  // Above ceiling -> LIMIT.
  st = pc_budget_check_tiles(&b, 6);
  if (st.code != PC_ERR_LIMIT) {
    fprintf(stderr, "FAIL: 6 tiles above ceiling\n");
    errors++;
  }
  // Null budget -> ARGUMENT.
  st = pc_budget_check_tiles(NULL, 1);
  if (st.code != PC_ERR_ARGUMENT) {
    fprintf(stderr, "FAIL: NULL budget tiles\n");
    errors++;
  }

  // Bytes: below / at / above ceiling.
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
  st = pc_budget_check_bytes(&b, 4096);
  if (st.code != PC_ERR_LIMIT) {
    fprintf(stderr, "FAIL: 4KB above byte ceiling\n");
    errors++;
  }
  st = pc_budget_check_bytes(NULL, 1);
  if (st.code != PC_ERR_ARGUMENT) {
    fprintf(stderr, "FAIL: NULL budget bytes\n");
    errors++;
  }

  // Unlimited budget (max==0) never trips.
  pc_budget u = {sizeof(pc_budget), 0, 0};
  st = pc_budget_check_tiles(&u, 1000000);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: unlimited tiles\n");
    errors++;
  }
  st = pc_budget_check_bytes(&u, 1ULL << 40);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: unlimited bytes\n");
    errors++;
  }

  // create validates; does not allocate (POD value type). destroy is no-op.
  pc_budget* out = NULL;
  st = pc_budget_create(5, 2048, &out);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: create valid out\n");
    errors++;
  }
  st = pc_budget_create(5, 2048, NULL);
  if (st.code != PC_ERR_ARGUMENT) {
    fprintf(stderr, "FAIL: create NULL out\n");
    errors++;
  }
  pc_budget_destroy(&b);
  pc_budget_destroy(&u);
  pc_budget_destroy(NULL);

  if (errors) {
    fprintf(stderr, "budget: FAIL (%d errors)\n", errors);
    return 1;
  }
  printf("budget: PASS\n");
  return 0;
}