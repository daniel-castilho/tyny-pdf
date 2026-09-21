#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// R17.1 R17.2 R17.3 R17.4 R17.5

#include "pdfcore/budget.h"
#include "pdfcore/status.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_INT_EQ(a, b) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "FAIL: %s:%d: %s == %s (%d != %d)\n", __FILE__, __LINE__, #a, #b, (a), (b)); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define ASSERT_STATUS_EQ(s, expected_code) do { \
    if ((s).code != (expected_code)) { \
        fprintf(stderr, "FAIL: %s:%d: expected code %d got %d (%s)\n", __FILE__, __LINE__, (expected_code), (s).code, (s).detail ? (s).detail : "-"); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

static void test_budget_defaults(void) {
  pc_budget b = pc_budget_default();
  ASSERT_INT_EQ(b.max_rss_mb, 250);
  ASSERT_INT_EQ(b.max_tiles, 64);
  ASSERT_INT_EQ(b.max_sidecar_bytes, 4 * 1024 * 1024);
}

static void test_budget_check_ok(void) {
  pc_budget b = pc_budget_default();
  pc_status s = pc_budget_check(&b, 10, 50 * 1024 * 1024);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);
}

static void test_budget_check_tile_limit(void) {
  pc_budget b = pc_budget_default();
  b.max_tiles = 1;
  pc_status s = pc_budget_check(&b, 2, 10 * 1024 * 1024);
  ASSERT_STATUS_EQ(s, PC_ERR_LIMIT);
}

static void test_budget_check_rss_limit(void) {
  pc_budget b = pc_budget_default();
  b.max_rss_mb = 10;
  pc_status s = pc_budget_check(&b, 1, 20 * 1024 * 1024);
  ASSERT_STATUS_EQ(s, PC_ERR_LIMIT);
}

static void test_budget_check_null(void) {
  pc_status s = pc_budget_check(nullptr, 1, 100);
  ASSERT_STATUS_EQ(s, PC_ERR_ARGUMENT);
}

static void test_budget_check_edge_cases(void) {
  pc_budget b = pc_budget_default();
  b.max_tiles = 5;
  b.max_rss_mb = 100;

  pc_status s = pc_budget_check(&b, 5, 100 * 1024 * 1024);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);

  s = pc_budget_check(&b, 6, 100 * 1024 * 1024);
  ASSERT_STATUS_EQ(s, PC_ERR_LIMIT);

  s = pc_budget_check(&b, 5, 101 * 1024 * 1024);
  ASSERT_STATUS_EQ(s, PC_ERR_LIMIT);
}

int main(void) {
  test_budget_defaults();
  test_budget_check_ok();
  test_budget_check_tile_limit();
  test_budget_check_rss_limit();
  test_budget_check_null();
  test_budget_check_edge_cases();

  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}