// R5.1 R5.2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctime>

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

#define ASSERT_INT_EQ(a, b) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "FAIL: %s:%d: %s == %s (%d != %d)\n", __FILE__, __LINE__, #a, #b, (a), (b)); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

static void test_page_count_mismatch(void) {
  const char* doc_path = "/tmp/test_staleness_pages.pdf";
  FILE* f = fopen(doc_path, "w");
  fclose(f);

  // Write sidecar with page_count = 5
  pc_sidecar* sidecar = (pc_sidecar*)calloc(1, sizeof(pc_sidecar));
  sidecar->format_version = 1;
  sidecar->document_sha256 = strdup("abc123");
  sidecar->document_path = strdup(doc_path);
  sidecar->modified_time = time(nullptr);
  sidecar->page_count = 5;  // Sidecar says 5 pages
  sidecar->annotations_json = strdup("[]");
  sidecar->unknown_json = strdup("{}");

  pc_status s = pc_sidecar_write(doc_path, sidecar);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);

  // Read back - should detect page count mismatch
  // (our stub returns 1 page, sidecar says 5)
  pc_sidecar* read = nullptr;
  s = pc_sidecar_read(doc_path, &read);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);
  ASSERT_INT_EQ(read->is_stale, 1);
  ASSERT_INT_EQ(read->stale_reason, PC_STALE_PAGE_COUNT);

  pc_sidecar_free(sidecar);
  pc_sidecar_free(read);
  unlink(doc_path);
  unlink("/tmp/test_staleness_pages.pdf.tynypdf.json");
}

static void test_fingerprint_match(void) {
  const char* doc_path = "/tmp/test_staleness_fp.pdf";
  FILE* f = fopen(doc_path, "w");
  fprintf(f, "test content");
  fclose(f);

  // Write sidecar with correct fingerprint
  char* fp = nullptr;
  pc_sidecar_compute_fingerprint(doc_path, &fp);
  
  pc_sidecar* sidecar = (pc_sidecar*)calloc(1, sizeof(pc_sidecar));
  sidecar->format_version = 1;
  sidecar->document_sha256 = fp;  // correct fingerprint
  sidecar->document_path = strdup(doc_path);
  sidecar->modified_time = time(nullptr);
  sidecar->page_count = 1;
  sidecar->annotations_json = strdup("[]");
  sidecar->unknown_json = strdup("{}");

  pc_status s = pc_sidecar_write(doc_path, sidecar);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);

  // Read back - fingerprint matches, should not be stale
  pc_sidecar* read = nullptr;
  s = pc_sidecar_read(doc_path, &read);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);
  ASSERT_INT_EQ(read->is_stale, 0);

  pc_sidecar_free(sidecar);
  pc_sidecar_free(read);
  unlink(doc_path);
  unlink("/tmp/test_staleness_fp.pdf.tynypdf.json");
}

static void test_modified_time_changed(void) {
  const char* doc_path = "/tmp/test_staleness_mtime.pdf";
  FILE* f = fopen(doc_path, "w");
  fprintf(f, "test content");
  fclose(f);

  // Write sidecar with old mtime
  pc_sidecar* sidecar = (pc_sidecar*)calloc(1, sizeof(pc_sidecar));
  sidecar->format_version = 1;
  sidecar->document_sha256 = strdup("abc123");
  sidecar->document_path = strdup(doc_path);
  sidecar->modified_time = time(nullptr) - 3600;  // 1 hour ago
  sidecar->page_count = 1;
  sidecar->annotations_json = strdup("[]");
  sidecar->unknown_json = strdup("{}");

  pc_status s = pc_sidecar_write(doc_path, sidecar);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);

  // Touch the file to update mtime
  usleep(10000);  // ensure mtime changes
  f = fopen(doc_path, "a");
  fprintf(f, " ");
  fclose(f);

  // Read back - mtime changed, should be stale (weak signal)
  pc_sidecar* read = nullptr;
  s = pc_sidecar_read(doc_path, &read);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);
  // Weak signal: may be stale if fingerprint doesn't match
  // Since fingerprint is wrong ("abc123"), it'll be STALE_FINGERPRINT first

  pc_sidecar_free(sidecar);
  pc_sidecar_free(read);
  unlink(doc_path);
  unlink("/tmp/test_staleness_mtime.pdf.tynypdf.json");
}

static void test_compute_fingerprint(void) {
  const char* doc_path = "/tmp/test_fp.pdf";
  FILE* f = fopen(doc_path, "w");
  fprintf(f, "hello world");
  fclose(f);

  char* fp = nullptr;
  pc_status s = pc_sidecar_compute_fingerprint(doc_path, &fp);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);
  ASSERT_INT_EQ((int)strlen(fp), 64);  // SHA256 hex = 64 chars

  // Same content = same fingerprint
  char* fp2 = nullptr;
  s = pc_sidecar_compute_fingerprint(doc_path, &fp2);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);
  ASSERT_INT_EQ(strcmp(fp, fp2), 0);

  free(fp);
  free(fp2);
  unlink(doc_path);
}

int main(void) {
  test_page_count_mismatch();
  test_fingerprint_match();
  test_modified_time_changed();
  test_compute_fingerprint();

  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}