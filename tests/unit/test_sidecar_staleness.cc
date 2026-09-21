// R5.1: Page count staleness detection
// R5.2: Fingerprint (strong) and mtime (weak) staleness
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
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

#define ASSERT_NE(a, b)                                                       \
  do {                                                                        \
    if ((a) == (b)) {                                                         \
      fprintf(stderr, "FAIL: %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
      tests_failed++;                                                         \
    } else {                                                                  \
      tests_passed++;                                                         \
    }                                                                         \
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

// Free string fields of stack-allocated pc_sidecar (NOT pc_sidecar_free which frees the struct
// itself)
#define FREE_SC_FIELDS(sc)       \
  do {                           \
    free((sc).document_sha256);  \
    free((sc).document_path);    \
    free((sc).annotations_json); \
    free((sc).unknown_json);     \
  } while (0)

static char* make_temp_doc() {
  char* tmpl = strdup("/tmp/tynypdf_test_XXXXXX");
  int fd = mkstemp(tmpl);
  if (fd < 0) {
    free(tmpl);
    return nullptr;
  }
  close(fd);
  return tmpl;
}

static void cleanup_doc(const char* doc) {
  if (!doc)
    return;
  char path[4096];
  snprintf(path, sizeof(path), "%s.tynypdf.json", doc);
  unlink(path);
  unlink(doc);
  free((void*)doc);
}

static int write_file_content(const char* path, const char* content) {
  FILE* f = fopen(path, "wb");
  if (!f)
    return 0;
  fwrite(content, 1, strlen(content), f);
  fclose(f);
  return 1;
}

// Helper: write content and compute real fingerprint
static char* setup_doc_with_fingerprint(const char* doc, const char* content) {
  ASSERT_TRUE(write_file_content(doc, content));
  char* fingerprint = nullptr;
  pc_status s = pc_sidecar_compute_fingerprint(doc, &fingerprint);
  ASSERT_EQ(s.code, PC_ERR_NONE);
  ASSERT_TRUE(fingerprint != nullptr);
  return fingerprint;
}

int main(void) {
  // Test 1: PageCountMismatch_R5_1
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);
    char* fingerprint = setup_doc_with_fingerprint(
        doc,
        "%PDF-1.4\n1 0 obj\n<<>>\nendobj\nxref\n0 1\n0000000000 65535 f \ntrailer\n<<>>\n%%EOF");

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup(fingerprint);
    sc.document_path = strdup(doc);
    sc.modified_time = time(nullptr);
    sc.page_count = 5;  // Mismatch!
    sc.annotations_json = strdup("[]");
    sc.unknown_json = strdup("{}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    pc_status s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    pc_sidecar* read_sc = nullptr;
    s = pc_sidecar_read(doc, &read_sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_NE(read_sc, nullptr);

    ASSERT_EQ(read_sc->is_stale, 1);
    ASSERT_EQ(read_sc->stale_reason, PC_STALE_PAGE_COUNT);

    pc_sidecar_free(read_sc);
    FREE_SC_FIELDS(sc);
    free(fingerprint);
    cleanup_doc(doc);
  }

  // Test 2: FingerprintChanged_R5_2_Strong
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);
    char* fingerprint = setup_doc_with_fingerprint(doc, "original content");

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup(fingerprint);
    sc.document_path = strdup(doc);
    sc.modified_time = time(nullptr);
    sc.page_count = 1;
    sc.annotations_json = strdup("[]");
    sc.unknown_json = strdup("{}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    pc_status s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    pc_sidecar* read_sc = nullptr;
    s = pc_sidecar_read(doc, &read_sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_NE(read_sc, nullptr);

    ASSERT_TRUE(write_file_content(doc, "modified content"));

    pc_sidecar* read_sc2 = nullptr;
    s = pc_sidecar_read(doc, &read_sc2);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_NE(read_sc2, nullptr);

    ASSERT_EQ(read_sc2->is_stale, 1);
    ASSERT_EQ(read_sc2->stale_reason, PC_STALE_FINGERPRINT);

    pc_sidecar_free(read_sc);
    pc_sidecar_free(read_sc2);
    FREE_SC_FIELDS(sc);
    free(fingerprint);
    cleanup_doc(doc);
  }

  // Test 3: MtimeChanged_R5_2_Weak
  // Note: fingerprint check runs first, so mtime change only detected if fingerprint matches
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);
    char* fingerprint = setup_doc_with_fingerprint(doc, "content");

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup(fingerprint);
    sc.document_path = strdup(doc);
    sc.modified_time = time(nullptr);
    sc.page_count = 1;
    sc.annotations_json = strdup("[]");
    sc.unknown_json = strdup("{}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    pc_status s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    pc_sidecar* read_sc = nullptr;
    s = pc_sidecar_read(doc, &read_sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_NE(read_sc, nullptr);

    usleep(1100000);
    struct timespec ts[2];
    time_t now = time(nullptr);
    ts[0].tv_sec = now;
    ts[0].tv_nsec = 0;
    ts[1].tv_sec = now;
    ts[1].tv_nsec = 0;
    utimensat(AT_FDCWD, doc, ts, 0);

    pc_sidecar* read_sc2 = nullptr;
    s = pc_sidecar_read(doc, &read_sc2);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_NE(read_sc2, nullptr);

    // Since fingerprint unchanged, mtime change should be detected
    ASSERT_EQ(read_sc2->is_stale, 1);
    ASSERT_EQ(read_sc2->stale_reason, PC_STALE_MODIFIED_TIME);

    pc_sidecar_free(read_sc);
    pc_sidecar_free(read_sc2);
    FREE_SC_FIELDS(sc);
    free(fingerprint);
    cleanup_doc(doc);
  }

  // Test 4: FreshDocumentNotStale
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);
    char* fingerprint = setup_doc_with_fingerprint(doc, "content");

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup(fingerprint);
    sc.document_path = strdup(doc);
    sc.modified_time = time(nullptr);
    sc.page_count = 1;
    sc.annotations_json = strdup("[]");
    sc.unknown_json = strdup("{}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    pc_status s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    pc_sidecar* read_sc = nullptr;
    s = pc_sidecar_read(doc, &read_sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_NE(read_sc, nullptr);

    // Should NOT be stale
    ASSERT_EQ(read_sc->is_stale, 0);
    ASSERT_EQ(read_sc->stale_reason, PC_STALE_NONE);

    pc_sidecar_free(read_sc);
    FREE_SC_FIELDS(sc);
    free(fingerprint);
    cleanup_doc(doc);
  }

  // Test 5: CheckStalenessFunction
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);
    char* fingerprint = setup_doc_with_fingerprint(doc, "content");

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup(fingerprint);
    sc.document_path = strdup(doc);
    sc.modified_time = time(nullptr);
    sc.page_count = 1;
    sc.annotations_json = strdup("[]");
    sc.unknown_json = strdup("{}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    pc_status s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    pc_sidecar* read_sc = nullptr;
    s = pc_sidecar_read(doc, &read_sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_NE(read_sc, nullptr);

    s = pc_sidecar_check_staleness(doc, read_sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_EQ(read_sc->is_stale, 0);
    ASSERT_EQ(read_sc->stale_reason, PC_STALE_NONE);

    ASSERT_TRUE(write_file_content(doc, "modified"));
    s = pc_sidecar_check_staleness(doc, read_sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_EQ(read_sc->is_stale, 1);
    ASSERT_EQ(read_sc->stale_reason, PC_STALE_FINGERPRINT);

    pc_sidecar_free(read_sc);
    FREE_SC_FIELDS(sc);
    free(fingerprint);
    cleanup_doc(doc);
  }

  fprintf(stderr, "PASSED: %d, FAILED: %d\n", tests_passed, tests_failed);
  return tests_failed ? 1 : 0;
}