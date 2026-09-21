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

// R6.1: Sidecar SHALL NOT contain view state
// R6.2: Sidecar SHALL NOT contain text, page geometry, or credentials

static void test_excluded_keys(void) {
  // Test 1: Sidecar with excluded keys should be rejected
  pc_sidecar* s1 = (pc_sidecar*)calloc(1, sizeof(pc_sidecar));
  s1->format_version = 1;
  s1->document_sha256 = strdup("abc");
  s1->document_path = strdup("/tmp/test.pdf");
  s1->modified_time = 0;
  s1->page_count = 1;
  s1->annotations_json = strdup("[]");
  s1->unknown_json = strdup("{open_page: 1}");
  pc_status s = pc_sidecar_write("/tmp/test.pdf", s1);
  if (s.code == PC_ERR_ARGUMENT && s.detail && strstr(s.detail, "excluded key")) {
    tests_passed++;
  } else {
    fprintf(stderr, "FAIL: test 1: expected excluded key rejection, got code=%d detail=%s\n",
            s.code, s.detail ? s.detail : "-");
    tests_failed++;
  }
  free(s1->document_sha256); free(s1->document_path);
  free(s1->annotations_json); free(s1->unknown_json); free(s1);

  // Test 2: Sidecar without excluded keys should succeed
  pc_sidecar* s2 = (pc_sidecar*)calloc(1, sizeof(pc_sidecar));
  s2->format_version = 1;
  s2->document_sha256 = strdup("abc");
  s2->document_path = strdup("/tmp/test.pdf");
  s2->modified_time = 0;
  s2->page_count = 1;
  s2->annotations_json = strdup("[]");
  s2->unknown_json = strdup("{}");
  s = pc_sidecar_write("/tmp/test.pdf", s2);
  if (s.code == PC_ERR_NONE) {
    tests_passed++;
  } else {
    fprintf(stderr, "FAIL: test 2: expected success, got code=%d detail=%s\n",
            s.code, s.detail ? s.detail : "-");
    tests_failed++;
  }
  free(s2->document_sha256); free(s2->document_path);
  free(s2->annotations_json); free(s2->unknown_json); free(s2);

  // Test 3: Sidecar with valid unknown key should succeed
  pc_sidecar* s3 = (pc_sidecar*)calloc(1, sizeof(pc_sidecar));
  s3->format_version = 1;
  s3->document_sha256 = strdup("abc");
  s3->document_path = strdup("/tmp/test.pdf");
  s3->modified_time = 0;
  s3->page_count = 1;
  s3->annotations_json = strdup("[]");
  s3->unknown_json = strdup("{{\"annot_id_1\": {\"key\": \"value\"}}}");
  s = pc_sidecar_write("/tmp/test.pdf", s3);
  if (s.code == PC_ERR_NONE) {
    tests_passed++;
  } else {
    fprintf(stderr, "FAIL: test 3: expected success for valid unknown key, got code=%d detail=%s\n",
            s.code, s.detail ? s.detail : "-");
    tests_failed++;
  }
  free(s3->document_sha256); free(s3->document_path);
  free(s3->annotations_json); free(s3->unknown_json); free(s3);
}

int main(void) {
  test_excluded_keys();

  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}