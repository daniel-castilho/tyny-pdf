// R2.3: Annotation ID validation - 10-char lowercase RFC 4648 base32
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/sidecar.h"
#include "pdfcore/status.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ(a, b)                                                                            \
  do {                                                                                             \
    if ((a) != (b)) {                                                                              \
      fprintf(stderr, "FAIL: %s:%d: %s == %s (%d != %d)\n", __FILE__, __LINE__, #a, #b, (a), (b)); \
      tests_failed++;                                                                              \
    } else {                                                                                       \
      tests_passed++;                                                                              \
    }                                                                                              \
  } while (0)

#define ASSERT_STRSTR(haystack, needle)                                                          \
  do {                                                                                           \
    if (!(haystack) || !(needle) || strstr((haystack), (needle)) == nullptr) {                   \
      fprintf(stderr, "FAIL: %s:%d: strstr('%s', '%s') failed\n", __FILE__, __LINE__, #haystack, \
              #needle);                                                                          \
      tests_failed++;                                                                            \
    } else {                                                                                     \
      tests_passed++;                                                                            \
    }                                                                                            \
  } while (0)

int main(void) {
  // Test 1: ValidBase32Ids
  {
    const char* valid_ids[] = {"abcdefghij", "abcdefghik", "abcdefghil", "abcdefghim", "abcdefghin",
                               "abcdefghio", "abcdefghip", "abcdefghiq", "abcdefghir", "abcdefghis",
                               "abcdefghit", "abcdefghiu", "abcdefghiv", "abcdefghiw", "abcdefghix",
                               "abcdefghiy", "abcdefghiz", "abcdefghi2", "abcdefghi3", "abcdefghi4",
                               "abcdefghi5", "abcdefghi6", "abcdefghi7", "bcdxyz2345", "mnopqrstuv",
                               "wxyz234567"};

    for (int i = 0; i < 26; ++i) {
      pc_status s = pc_sidecar_validate_annotation_id(valid_ids[i]);
      ASSERT_EQ(s.code, PC_ERR_NONE);
    }
  }

  // Test 2: RejectsWrongLength
  {
    const char* invalid_ids[] = {"",          "a",           "ab",          "abcde",
                                 "abcdefghi", "abcdefghijk", "abcdefghijkl"};

    for (int i = 0; i < 7; ++i) {
      pc_status s = pc_sidecar_validate_annotation_id(invalid_ids[i]);
      ASSERT_EQ(s.code, PC_ERR_ARGUMENT);
    }
  }

  // Test 3: RejectsUppercase
  {
    const char* invalid_ids[] = {"ABCDEFGHIJ", "Abcdefghij", "abcdefghijK"};

    for (int i = 0; i < 3; ++i) {
      pc_status s = pc_sidecar_validate_annotation_id(invalid_ids[i]);
      ASSERT_EQ(s.code, PC_ERR_ARGUMENT);
    }
  }

  // Test 4: RejectsInvalidChars
  {
    const char* invalid_ids[] = {"abcdefgh10", "abcdefgh89", "abcdefgh!@", "abcde fghi",
                                 "abcde-fghi", "abcde.fghi", "abcde_ghij"};

    for (int i = 0; i < 7; ++i) {
      pc_status s = pc_sidecar_validate_annotation_id(invalid_ids[i]);
      ASSERT_EQ(s.code, PC_ERR_ARGUMENT);
    }
  }

  // Test 5: RejectsNull
  {
    pc_status s = pc_sidecar_validate_annotation_id(nullptr);
    ASSERT_EQ(s.code, PC_ERR_ARGUMENT);
  }

  // Test 6: AllCharsInAlphabet
  {
    const char* alphabet = "abcdefghijkmnopqrstuvwxyz234567";
    for (int i = 0; alphabet[i]; ++i) {
      char id[11];
      memset(id, alphabet[i], 10);
      id[10] = '\0';
      pc_status s = pc_sidecar_validate_annotation_id(id);
      ASSERT_EQ(s.code, PC_ERR_NONE);
    }
  }

  // Test 7: ExcludedCharsAreRejected
  {
    const char* excluded = "0189";
    for (int i = 0; excluded[i]; ++i) {
      char id[11];
      memset(id, 'a', 9);
      id[9] = excluded[i];
      id[10] = '\0';
      pc_status s = pc_sidecar_validate_annotation_id(id);
      ASSERT_EQ(s.code, PC_ERR_ARGUMENT);
    }
  }

  fprintf(stderr, "PASSED: %d, FAILED: %d\n", tests_passed, tests_failed);
  return tests_failed ? 1 : 0;
}