#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctime>
#include <sys/stat.h>

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

static void test_write_read_basic(void) {
  const char* doc_path = "/tmp/test_doc_basic.pdf";
  FILE* f = fopen(doc_path, "w");
  fclose(f);

  pc_sidecar* sidecar = (pc_sidecar*)calloc(1, sizeof(pc_sidecar));
  sidecar->format_version = 1;
  sidecar->document_sha256 = strdup("abc123");
  sidecar->document_path = strdup(doc_path);
  sidecar->modified_time = time(nullptr);
  sidecar->page_count = 5;
  sidecar->annotations_json = strdup("[{\"id\":\"abcdefgh12\",\"text\":\"Test\"}]");
  sidecar->unknown_json = strdup("{}");

  pc_status s = pc_sidecar_write(doc_path, sidecar);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);

  pc_sidecar* read = nullptr;
  s = pc_sidecar_read(doc_path, &read);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);
  ASSERT_INT_EQ(read->format_version, 1);

  pc_sidecar_free(sidecar);
  pc_sidecar_free(read);
  unlink(doc_path);
  unlink("/tmp/test_doc_basic.pdf.tynypdf.json");
}

static void test_write_atomic_rename(void) {
  const char* doc_path = "/tmp/test_doc_atomic.pdf";
  FILE* f = fopen(doc_path, "w");
  fclose(f);

  pc_sidecar* sidecar = (pc_sidecar*)calloc(1, sizeof(pc_sidecar));
  sidecar->format_version = 1;
  sidecar->document_sha256 = strdup("def456");
  sidecar->document_path = strdup(doc_path);
  sidecar->modified_time = time(nullptr);
  sidecar->page_count = 3;
  sidecar->annotations_json = strdup("[]");
  sidecar->unknown_json = strdup("{}");

  pc_status s = pc_sidecar_write(doc_path, sidecar);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);

  pc_sidecar_free(sidecar);
  unlink(doc_path);
}

static void test_write_document_mismatch(void) {
  const char* doc_path = "/tmp/test_doc_mismatch.pdf";
  FILE* f = fopen(doc_path, "w");
  fclose(f);

  pc_sidecar* sidecar = (pc_sidecar*)calloc(1, sizeof(pc_sidecar));
  sidecar->format_version = 1;
  sidecar->document_sha256 = strdup("abc");
  sidecar->document_path = strdup("/different/path.pdf"); // Mismatch!
  sidecar->modified_time = time(nullptr);
  sidecar->page_count = 1;
  sidecar->annotations_json = strdup("[]");
  sidecar->unknown_json = strdup("{}");

  pc_status s = pc_sidecar_write(doc_path, sidecar);
  ASSERT_STATUS_EQ(s, PC_ERR_ARGUMENT);

  pc_sidecar_free(sidecar);
  unlink(doc_path);
}

static void test_lock_fresh(void) {
  const char* doc_path = "/tmp/test_doc_lock.pdf";
  FILE* f = fopen(doc_path, "w");
  fclose(f);

  pc_status s = pc_sidecar_try_lock(doc_path);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);

  // Second lock should fail
  s = pc_sidecar_try_lock(doc_path);
  ASSERT_STATUS_EQ(s, PC_ERR_STATE);

  pc_sidecar_unlock(doc_path);
  unlink(doc_path);
}

static void test_excluded_keys_rejected(void) {
  const char* doc_path = "/tmp/test_doc_excluded.pdf";
  FILE* f = fopen(doc_path, "w");
  fclose(f);

  pc_sidecar* sidecar = (pc_sidecar*)calloc(1, sizeof(pc_sidecar));
  sidecar->format_version = 1;
  sidecar->document_sha256 = strdup("abc");
  sidecar->document_path = strdup(doc_path);
  sidecar->modified_time = time(nullptr);
  sidecar->page_count = 1;
  sidecar->annotations_json = strdup("[]");
  sidecar->unknown_json = strdup("{\"open_page\": 5}");

  pc_status s = pc_sidecar_write(doc_path, sidecar);
  ASSERT_STATUS_EQ(s, PC_ERR_ARGUMENT);

  pc_sidecar_free(sidecar);
  unlink(doc_path);
}

int main(void) {
  test_write_read_basic();
  test_write_atomic_rename();
  test_write_document_mismatch();
  test_lock_fresh();
  test_excluded_keys_rejected();

  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}