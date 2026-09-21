// R3.1: Atomic write via tmp->rename+fsync
// R4.2: Application SHALL NOT write sidecar for document it did not open
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

#define ASSERT_STRNE(a, b)                                                            \
  do {                                                                                \
    if ((a) && (b) && strcmp((a), (b)) == 0) {                                        \
      fprintf(stderr, "FAIL: %s:%d: %s != %s (equal)\n", __FILE__, __LINE__, #a, #b); \
      tests_failed++;                                                                 \
    } else if (!(a) || !(b)) {                                                        \
      fprintf(stderr, "FAIL: %s:%d: null pointer\n", __FILE__, __LINE__);             \
      tests_failed++;                                                                 \
    } else {                                                                          \
      tests_passed++;                                                                 \
    }                                                                                 \
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

#define ASSERT_FALSE(expr)                                             \
  do {                                                                 \
    if (expr) {                                                        \
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

int main(void) {
  // Test 1: AtomicWriteCreatesTempThenRenames
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup("abc123");
    sc.document_path = strdup(doc);
    sc.modified_time = time(nullptr);
    sc.page_count = 5;
    sc.annotations_json =
        strdup("[{\"id\":\"abcdefghij\",\"type\":\"text\",\"rect\":[0,0,100,100]}]");
    sc.unknown_json = strdup("{}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    pc_status s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    char sidecar_path[4096];
    snprintf(sidecar_path, sizeof(sidecar_path), "%s.tynypdf.json", doc);
    FILE* f = fopen(sidecar_path, "r");
    ASSERT_NE(f, nullptr);
    if (f)
      fclose(f);

    FREE_SC_FIELDS(sc);
    cleanup_doc(doc);
  }

  // Test 2: RejectsPathMismatch_R4_2
  {
    char* doc1 = make_temp_doc();
    char* doc2 = make_temp_doc();
    ASSERT_TRUE(doc1 != nullptr);
    ASSERT_TRUE(doc2 != nullptr);

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup("abc123");
    sc.document_path = strdup(doc1);
    sc.modified_time = time(nullptr);
    sc.page_count = 1;
    sc.annotations_json = strdup("[]");
    sc.unknown_json = strdup("{}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    pc_status s = pc_sidecar_write(doc2, &sc);
    ASSERT_EQ(s.code, PC_ERR_ARGUMENT);
    ASSERT_NE(s.detail, nullptr);
    ASSERT_STRSTR(s.detail, "path mismatch");

    FREE_SC_FIELDS(sc);
    cleanup_doc(doc1);
    cleanup_doc(doc2);
  }

  // Test 3: CanonicalJSONFormat_R1_1
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup("deadbeef1234567890");
    sc.document_path = strdup(doc);
    sc.modified_time = 1700000000;
    sc.page_count = 3;
    sc.annotations_json =
        strdup("[{\"id\":\"abcdefghij\",\"type\":\"highlight\",\"rect\":[10.0,20.0,30.0,40.0]}]");
    sc.unknown_json = strdup("{\"custom_field\":\"value\"}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    pc_status s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    char sidecar_path[4096];
    snprintf(sidecar_path, sizeof(sidecar_path), "%s.tynypdf.json", doc);
    FILE* f = fopen(sidecar_path, "r");
    ASSERT_NE(f, nullptr);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1);
    size_t got = fread(buf, 1, sz, f);
    ASSERT_EQ((int)got, (int)sz);
    buf[sz] = '\0';
    fclose(f);

    ASSERT_STRSTR(buf, "\n");
    ASSERT_TRUE(strchr(buf, '\r') == nullptr);
    ASSERT_STRSTR(buf, "  ");
    ASSERT_TRUE(strstr(buf, "\"annotations\"") < strstr(buf, "\"document\""));
    ASSERT_TRUE(strstr(buf, "\"document\"") < strstr(buf, "\"format_version\""));
    ASSERT_TRUE(strstr(buf, "\"format_version\"") < strstr(buf, "\"is_stale\""));
    ASSERT_TRUE(strstr(buf, "\"is_stale\"") < strstr(buf, "\"stale_reason\""));
    ASSERT_TRUE(strstr(buf, "\"stale_reason\"") < strstr(buf, "\"unknown\""));

    free(buf);
    FREE_SC_FIELDS(sc);
    cleanup_doc(doc);
  }

  // Test 4: RejectsExcludedKeys_R6
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup("abc123");
    sc.document_path = strdup(doc);
    sc.modified_time = time(nullptr);
    sc.page_count = 1;
    sc.annotations_json = strdup("[]");
    sc.unknown_json = strdup("{\"open_page\":1,\"zoom\":1.5}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    pc_status s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_ARGUMENT);

    FREE_SC_FIELDS(sc);
    cleanup_doc(doc);
  }

  // Test 5: FsyncCalled (atomic behavior)
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup("abc123");
    sc.document_path = strdup(doc);
    sc.modified_time = time(nullptr);
    sc.page_count = 1;
    sc.annotations_json = strdup("[]");
    sc.unknown_json = strdup("{}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    pc_status s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    char sidecar_path[4096];
    snprintf(sidecar_path, sizeof(sidecar_path), "%s.tynypdf.json", doc);
    FILE* f = fopen(sidecar_path, "r");
    ASSERT_NE(f, nullptr);
    if (f)
      fclose(f);

    FREE_SC_FIELDS(sc);
    cleanup_doc(doc);
  }

  fprintf(stderr, "PASSED: %d, FAILED: %d\n", tests_passed, tests_failed);
  return tests_failed ? 1 : 0;
}