// R2.3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pdfcore/sidecar.h"
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

// Test valid annotation IDs (10-char lowercase RFC 4648 base32)
static void test_valid_ids(void) {
  struct { const char* id; } valid[] = {
    "abcd2fghij",   // valid: letters + digit 2
    "abcdefghij",   // valid: a-k (l excluded)
    "234567abcd",   // valid: digits 2-7 only
    "abcdefghk2",   // valid: includes k and digit 2
    {NULL},         // sentinel
  };

  for (int i = 0; valid[i].id != NULL; ++i) {
    pc_status s = pc_sidecar_validate_annotation_id(valid[i].id);
    ASSERT_STATUS_EQ(s, PC_ERR_NONE);
  }
}

// Test invalid annotation IDs
static void test_invalid_ids(void) {
  struct { const char* id; } invalid[] = {
    "abc",          // too short (3 chars)
    "abcdefghijk",  // too long (11 chars)
    "ABCDEFGHIJ",   // uppercase - should fail
    "abcdefghij=",  // has padding - should fail
    "abcde!ghij",   // special char - should fail
    "abcde1ghij",   // digit 1 excluded - should fail
    "abcde8ghij",   // digit 8 excluded - should fail
    "abcde0ghij",   // digit 0 excluded - should fail
    "abc_lmnopq",   // 'l' excluded - should fail
    {NULL},         // sentinel
  };

  for (int i = 0; invalid[i].id != NULL; ++i) {
    pc_status s = pc_sidecar_validate_annotation_id(invalid[i].id);
    ASSERT_STATUS_EQ(s, PC_ERR_ARGUMENT);
  }
}

static void test_null_argument(void) {
  pc_status s = pc_sidecar_validate_annotation_id(NULL);
  ASSERT_STATUS_EQ(s, PC_ERR_ARGUMENT);
}

int main(void) {
  test_valid_ids();
  test_invalid_ids();
  test_null_argument();

  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}