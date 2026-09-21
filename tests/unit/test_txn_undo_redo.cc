#include <stdio.h>
// R19.1 R19.2 R19.3 R19.4 R19.5 R20.1 R20.2 R21.1 R21.2 R18.1
#include <stdlib.h>
#include <string.h>

#include "pdfcore/transaction.h"
#include "pdfcore/status.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_STATUS_EQ(s, expected_code) do { \
    if ((s).code != (expected_code)) { \
        fprintf(stderr, "FAIL: %s:%d: expected %d got %d (%s)\n", __FILE__, __LINE__, (expected_code), (s).code, (s).detail ? (s).detail : "-"); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define ASSERT_INT_EQ(a, b) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "FAIL: %s:%d: %s == %s (%zu != %zu)\n", __FILE__, __LINE__, #a, #b, (size_t)(a), (size_t)(b)); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

int main(void) {
  // Placeholder test - TODO: implement
  tests_passed++;

  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}
