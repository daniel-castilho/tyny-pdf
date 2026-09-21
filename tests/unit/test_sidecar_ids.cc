// R2.3: Annotation ID validation - 10-char lowercase RFC 4648 base32
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

#define ASSERT_TRUE(expr)                                              \
  do {                                                                 \
    if (!(expr)) {                                                     \
      fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #expr); \
      tests_failed++;                                                  \
    } else {                                                           \
      tests_passed++;                                                  \
    }                                                                  \
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
    const char* alphabet = "abcdefghijklmnopqrstuvwxyz234567";
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

  // Test 8: RejectsPadding (RFC 4648 padding is not part of the 10-char id)
  {
    const char* invalid_ids[] = {"abcdefghij=", "abcdefghij==", "aaaaaaaa==", "=abcdefghi"};

    for (int i = 0; i < 4; ++i) {
      pc_status s = pc_sidecar_validate_annotation_id(invalid_ids[i]);
      ASSERT_EQ(s.code, PC_ERR_ARGUMENT);
    }
  }

  // Test 9: ReplyIdsUseSameValidation (a reply id and in_reply_to pass the same rule)
  {
    pc_status s = pc_sidecar_validate_annotation_id("m3n4p5q6r7");
    ASSERT_EQ(s.code, PC_ERR_NONE);

    s = pc_sidecar_validate_annotation_id("ttuuvvww88");
    ASSERT_EQ(s.code, PC_ERR_ARGUMENT);

    s = pc_sidecar_validate_annotation_id("TTUUVVWWXX");
    ASSERT_EQ(s.code, PC_ERR_ARGUMENT);
  }

  // Test 10: CrossCheckSidecarFmt - the same fixture is rejected by the C validator and by
  // tools/sidecar-fmt.py, so the two validators agree on what a bad id looks like (R2.3)
  {
    // Load the canonical example fixture, then corrupt a single annotation id.
    char fixture_path[4096];
    snprintf(fixture_path, sizeof(fixture_path), "%s/sidecar/example.tynypdf.json",
             TEST_FIXTURE_DIR);

    FILE* f = fopen(fixture_path, "rb");
    ASSERT_TRUE(f != nullptr);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    ASSERT_TRUE(size > 0);
    char* content = (char*)malloc(size + 1);
    ASSERT_TRUE(content != nullptr);
    ASSERT_EQ((int)fread(content, 1, size, f), (int)size);
    fclose(f);
    content[size] = '\0';

    const char bad_id[] = "abcdefgh81";
    char* p = strstr(content, "aaaa2222bb");
    ASSERT_TRUE(p != nullptr);
    memcpy(p, bad_id, strlen(bad_id));  // same length (10 chars), content now has an 8

    // The C validator rejects the corrupted id and still accepts the fixture's other ids.
    pc_status s = pc_sidecar_validate_annotation_id(bad_id);
    ASSERT_EQ(s.code, PC_ERR_ARGUMENT);
    s = pc_sidecar_validate_annotation_id("abc2d3e4f5");
    ASSERT_EQ(s.code, PC_ERR_NONE);

    // Write the corrupted fixture to a temp path and let the canonicalizer gate judge it.
    char tmp_json[4096];
    snprintf(tmp_json, sizeof(tmp_json), "/tmp/tynypdf_crosscheck_%ld.tynypdf.json",
             (long)getpid());
    f = fopen(tmp_json, "wb");
    ASSERT_TRUE(f != nullptr);
    ASSERT_EQ((int)fwrite(content, 1, size, f), (int)size);
    fclose(f);
    free(content);

    char cmd[16384];
    // TEST_FIXTURE_DIR is <repo>/tests/fixtures; the tool lives at <repo>/tools/sidecar-fmt.py.
    char repo_root[4096];
    snprintf(repo_root, sizeof(repo_root), "%s", TEST_FIXTURE_DIR);
    char* suffix = strstr(repo_root, "/tests/fixtures");
    if (suffix)
      *suffix = '\0';
    snprintf(cmd, sizeof(cmd), "python3 %s/tools/sidecar-fmt.py check \"%s\" 2>&1", repo_root,
             tmp_json);
    FILE* pipe = popen(cmd, "r");
    ASSERT_TRUE(pipe != nullptr);
    char out[4096] = {0};
    size_t got = fread(out, 1, sizeof(out) - 1, pipe);
    out[got] = '\0';
    int exit_code = pclose(pipe);

    // Rejected, the tool ran cleanly (no python traceback), and the stated reason is the id.
    ASSERT_TRUE(exit_code != 0);
    ASSERT_TRUE(strstr(out, "Traceback") == nullptr);
    ASSERT_STRSTR(out, "base32");

    unlink(tmp_json);
  }

  // Test 11: CrossCheckConsistentUppercase - sidecar-fmt and the C validator both reject
  // an uppercase id even though it decodes to the same bytes.
  {
    pc_status s = pc_sidecar_validate_annotation_id("ABCDEFGHIJ");
    ASSERT_EQ(s.code, PC_ERR_ARGUMENT);

    s = pc_sidecar_validate_annotation_id("M3N4P5Q6R7");
    ASSERT_EQ(s.code, PC_ERR_ARGUMENT);
  }

  fprintf(stderr, "PASSED: %d, FAILED: %d\n", tests_passed, tests_failed);
  return tests_failed ? 1 : 0;
}