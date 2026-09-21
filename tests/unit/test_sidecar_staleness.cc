// R5.1: Page count staleness detection
// R5.2: Fingerprint (strong) and mtime (weak) staleness
// R5.1: writes to a stale sidecar are refused until the caller passes force (write_force)
//
// The C-library null backend reports a fixed page count of 5 for any document, so
// page-count mismatch tests drive the corner by recording a different count.
//
// stdout is a deterministic stale report (one line per stale signal, in a fixed order)
// that tests/golden/sidecar-stale-report.txt locks byte-for-byte; mtimes come from
// fixed values (946684740 = 1999-12-31T23:59:00Z, 946684800 = 2000-01-01T00:00:00Z) so the
// golden cannot drift with the wall clock.
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

#define ASSERT_STREQ(a, b)                                                                       \
  do {                                                                                           \
    if ((a) && (b) && strcmp((a), (b)) == 0) {                                                   \
      tests_passed++;                                                                            \
    } else {                                                                                     \
      fprintf(stderr, "FAIL: %s:%d: %s == %s ('%s' != '%s')\n", __FILE__, __LINE__, #a, #b, (a), \
              (b));                                                                              \
      tests_failed++;                                                                            \
    }                                                                                            \
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

// Free string fields of stack-allocated pc_sidecar (NOT pc_sidecar_free which frees the struct
// itself)
#define FREE_SC_FIELDS(sc)       \
  do {                           \
    free((sc).document_sha256);  \
    free((sc).document_path);    \
    free((sc).annotations_json); \
    free((sc).unknown_json);     \
  } while (0)

// Null backend reports page_count == 5 for any document; a matching sidecar must record 5.
#define NULL_BACKEND_PAGES 5
// Fixed mtimes so the golden report is deterministic (RFC 3339).
#define MTIME_STALE_SIDECAR 946684740   // 1999-12-31T23:59:00Z
#define MTIME_STALE_DOCUMENT 946684800  // 2000-01-01T00:00:00Z

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

static void set_doc_mtime(const char* path, int64_t sec) {
  struct timespec ts[2];
  ts[0].tv_sec = sec;
  ts[0].tv_nsec = 0;
  ts[1].tv_sec = sec;
  ts[1].tv_nsec = 0;
  utimensat(AT_FDCWD, path, ts, 0);
}

// Helper: write content, pin the doc mtime, and compute the real fingerprint
static char* setup_doc_with_fingerprint(const char* doc, const char* content) {
  ASSERT_TRUE(write_file_content(doc, content));
  set_doc_mtime(doc, MTIME_STALE_SIDECAR);
  char* fingerprint = nullptr;
  pc_status s = pc_sidecar_compute_fingerprint(doc, &fingerprint);
  ASSERT_EQ(s.code, PC_ERR_NONE);
  ASSERT_TRUE(fingerprint != nullptr);
  return fingerprint;
}

// Prints the frozen stale report for a sidecar to stdout (the golden stream).
static void print_report(const pc_sidecar* sc) {
  char buf[4096];
  pc_status s = pc_sidecar_stale_report(sc, buf, sizeof(buf));
  if (s.code != PC_ERR_NONE)
    return;
  if (sc->is_stale)
    printf("%s\n", buf);
}

int main(void) {
  // Test 1: PageCountMismatch_R5_1
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);
    char* fingerprint = setup_doc_with_fingerprint(doc, "content");

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup(fingerprint);
    sc.document_path = strdup(doc);
    sc.modified_time = MTIME_STALE_SIDECAR;
    sc.page_count = 9;  // Mismatch against the null backend's 5
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
    char report[256];
    s = pc_sidecar_stale_report(read_sc, report, sizeof(report));
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_STRSTR(report, "stale: sidecar pages 9 vs document 5");
    print_report(read_sc);

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
    sc.modified_time = MTIME_STALE_SIDECAR;
    sc.page_count = NULL_BACKEND_PAGES;
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
    ASSERT_EQ(read_sc->is_stale, 0);
    pc_sidecar_free(read_sc);

    // Rewrite the document (content bytes differ; mtime pinned so only the fingerprint fires).
    ASSERT_TRUE(write_file_content(doc, "modified content"));
    set_doc_mtime(doc, MTIME_STALE_SIDECAR);

    pc_sidecar* read_sc2 = nullptr;
    s = pc_sidecar_read(doc, &read_sc2);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_NE(read_sc2, nullptr);

    ASSERT_EQ(read_sc2->is_stale, 1);
    ASSERT_EQ(read_sc2->stale_reason, PC_STALE_FINGERPRINT);
    char report[256];
    s = pc_sidecar_stale_report(read_sc2, report, sizeof(report));
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_STRSTR(report, "stale: fingerprint mismatch (doc sha256 ");
    ASSERT_STRSTR(report, "... vs sidecar ");
    print_report(read_sc2);

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
    sc.modified_time = MTIME_STALE_SIDECAR;
    sc.page_count = NULL_BACKEND_PAGES;
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
    ASSERT_EQ(read_sc->is_stale, 0);
    pc_sidecar_free(read_sc);

    // Touch the document to a strictly newer mtime without changing its bytes.
    set_doc_mtime(doc, MTIME_STALE_DOCUMENT);

    pc_sidecar* read_sc2 = nullptr;
    s = pc_sidecar_read(doc, &read_sc2);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_NE(read_sc2, nullptr);

    ASSERT_EQ(read_sc2->is_stale, 1);
    ASSERT_EQ(read_sc2->stale_reason, PC_STALE_MODIFIED_TIME);
    char report[256];
    s = pc_sidecar_stale_report(read_sc2, report, sizeof(report));
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_STRSTR(report,
                  "stale: mtime newer (doc 2000-01-01T00:00:00Z vs sidecar 1999-12-31T23:59:00Z)");
    print_report(read_sc2);

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
    sc.modified_time = MTIME_STALE_SIDECAR;
    sc.page_count = NULL_BACKEND_PAGES;
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

    ASSERT_EQ(read_sc->is_stale, 0);
    ASSERT_EQ(read_sc->stale_reason, PC_STALE_NONE);
    char report[256] = {0};
    s = pc_sidecar_stale_report(read_sc, report, sizeof(report));
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_STREQ(report, "");

    pc_sidecar_free(read_sc);
    FREE_SC_FIELDS(sc);
    free(fingerprint);
    cleanup_doc(doc);
  }

  // Test 5: CheckStalenessFunction - fresh returns PC_ERR_NONE, stale returns PC_ERR_STATE
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);
    char* fingerprint = setup_doc_with_fingerprint(doc, "content");

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup(fingerprint);
    sc.document_path = strdup(doc);
    sc.modified_time = MTIME_STALE_SIDECAR;
    sc.page_count = NULL_BACKEND_PAGES;
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
    set_doc_mtime(doc, MTIME_STALE_SIDECAR);
    s = pc_sidecar_check_staleness(doc, read_sc);
    ASSERT_EQ(s.code, PC_ERR_STATE);
    ASSERT_EQ(read_sc->is_stale, 1);
    ASSERT_EQ(read_sc->stale_reason, PC_STALE_FINGERPRINT);

    pc_sidecar_free(read_sc);
    FREE_SC_FIELDS(sc);
    free(fingerprint);
    cleanup_doc(doc);
  }

  // Test 6: WriterRefusesStaleWithoutForce (R5.1)
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);
    char* fingerprint = setup_doc_with_fingerprint(doc, "content");

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup(fingerprint);
    sc.document_path = strdup(doc);
    sc.modified_time = MTIME_STALE_SIDECAR;
    sc.page_count = NULL_BACKEND_PAGES;
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

    // Trap: the document changed behind us, so the sidecar is now stale.
    ASSERT_TRUE(write_file_content(doc, "changed"));
    set_doc_mtime(doc, MTIME_STALE_SIDECAR);
    s = pc_sidecar_check_staleness(doc, read_sc);
    ASSERT_EQ(s.code, PC_ERR_STATE);
    ASSERT_EQ(read_sc->is_stale, 1);

    // Plain write is refused with the stale detail as the message (R5.1).
    s = pc_sidecar_write(doc, read_sc);
    ASSERT_EQ(s.code, PC_ERR_STATE);
    ASSERT_STRSTR(s.detail, "stale:");

    // An explicit force bypasses only the staleness guard.
    s = pc_sidecar_write_force(doc, read_sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    char sidecar_path[4096];
    snprintf(sidecar_path, sizeof(sidecar_path), "%s.tynypdf.json", doc);
    ASSERT_TRUE(access(sidecar_path, F_OK) == 0);

    pc_sidecar_free(read_sc);
    FREE_SC_FIELDS(sc);
    free(fingerprint);
    cleanup_doc(doc);
  }

  fprintf(stderr, "PASSED: %d, FAILED: %d\n", tests_passed, tests_failed);
  return tests_failed ? 1 : 0;
}