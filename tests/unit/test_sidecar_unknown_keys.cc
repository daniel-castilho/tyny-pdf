// R4.1: Unknown keys preserved on save
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

// Helper: write doc content and compute the real fingerprint so a read-back sidecar stays fresh
static char* setup_doc_with_fingerprint(const char* doc, const char* content) {
  FILE* f = fopen(doc, "wb");
  if (!f)
    return nullptr;
  fwrite(content, 1, strlen(content), f);
  fclose(f);

  char* fingerprint = nullptr;
  pc_status s = pc_sidecar_compute_fingerprint(doc, &fingerprint);
  if (s.code != PC_ERR_NONE || !fingerprint) {
    return nullptr;
  }
  return fingerprint;
}

int main(void) {
  // Test 1: PreservesUnknownKeysOnWrite
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
    sc.unknown_json = strdup("{\"future_feature\":{\"enabled\":true},\"custom_meta\":\"data\"}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    pc_status s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    pc_sidecar* read_sc = nullptr;
    s = pc_sidecar_read(doc, &read_sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_NE(read_sc, nullptr);

    ASSERT_NE(read_sc->unknown_json, nullptr);
    ASSERT_STRSTR(read_sc->unknown_json, "future_feature");
    ASSERT_STRSTR(read_sc->unknown_json, "custom_meta");

    pc_sidecar_free(read_sc);
    FREE_SC_FIELDS(sc);
    cleanup_doc(doc);
  }

  // Test 2: PreservesUnknownKeysAcrossRoundtrip
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    // A real fingerprint keeps the read-back sidecar fresh, so the roundtrip write passes.
    char* fingerprint = setup_doc_with_fingerprint(doc, "test content");
    ASSERT_TRUE(fingerprint != nullptr);

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup(fingerprint);
    sc.document_path = strdup(doc);
    sc.modified_time = time(nullptr);
    sc.page_count = 5;
    sc.annotations_json = strdup("[{\"id\":\"abcdefghij\"}]");
    sc.unknown_json = strdup("{\"app_version\":\"1.0\",\"user_prefs\":{\"theme\":\"dark\"}}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    pc_status s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    pc_sidecar* read_sc = nullptr;
    s = pc_sidecar_read(doc, &read_sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_NE(read_sc, nullptr);
    ASSERT_EQ(read_sc->is_stale, 0);

    s = pc_sidecar_write(doc, read_sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    pc_sidecar* read_sc2 = nullptr;
    s = pc_sidecar_read(doc, &read_sc2);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_NE(read_sc2, nullptr);

    ASSERT_STRSTR(read_sc2->unknown_json, "app_version");
    ASSERT_STRSTR(read_sc2->unknown_json, "user_prefs");
    ASSERT_STRSTR(read_sc2->unknown_json, "theme");

    pc_sidecar_free(read_sc);
    pc_sidecar_free(read_sc2);
    FREE_SC_FIELDS(sc);
    free(fingerprint);
    cleanup_doc(doc);
  }

  fprintf(stderr, "PASSED: %d, FAILED: %d\n", tests_passed, tests_failed);
  return tests_failed ? 1 : 0;
}