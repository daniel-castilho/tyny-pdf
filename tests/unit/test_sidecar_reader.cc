// R2.2: Future version handling - load read-only and show reason
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

int main(void) {
  // Test 1: ReadsCurrentVersion
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

    pc_sidecar* read_sc = nullptr;
    s = pc_sidecar_read(doc, &read_sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_TRUE(read_sc != nullptr);

    ASSERT_EQ(read_sc->format_version, 1);
    ASSERT_EQ(read_sc->read_only, 0);

    pc_sidecar_free(read_sc);
    FREE_SC_FIELDS(sc);
    cleanup_doc(doc);
  }

  // Test 2: FutureVersionReadOnly (R2.2)
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    char sidecar_path[4096];
    snprintf(sidecar_path, sizeof(sidecar_path), "%s.tynypdf.json", doc);
    FILE* f = fopen(sidecar_path, "w");
    ASSERT_TRUE(f != nullptr);
    fprintf(f,
            "{\n"
            "  \"annotations\": [],\n"
            "  \"document\": {\n"
            "    \"modified\": 1700000000,\n"
            "    \"pages\": 1,\n"
            "    \"path\": \"%s\",\n"
            "    \"sha256\": \"abc123\"\n"
            "  },\n"
            "  \"format_version\": 99,\n"
            "  \"is_stale\": 0,\n"
            "  \"stale_reason\": 0,\n"
            "  \"unknown\": {}\n"
            "}\n",
            doc);
    fclose(f);

    pc_sidecar* read_sc = nullptr;
    pc_status read_st = pc_sidecar_read(doc, &read_sc);
    ASSERT_EQ(read_st.code, PC_ERR_VERSION);
    ASSERT_TRUE(read_sc != nullptr);
    ASSERT_STREQ(read_st.detail, "format_version 99 > supported 1");

    ASSERT_EQ(read_sc->format_version, 99);
    ASSERT_EQ(read_sc->read_only, 1);

    // A read-only view may be rendered but not mutated: write must refuse (R2.2).
    pc_status wr_st = pc_sidecar_write(doc, read_sc);
    ASSERT_EQ(wr_st.code, PC_ERR_STATE);

    pc_sidecar_free(read_sc);
    cleanup_doc(doc);
  }

  // Test 3: MissingSidecarReturnsError
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    pc_sidecar* read_sc = nullptr;
    pc_status s = pc_sidecar_read(doc, &read_sc);
    ASSERT_EQ(s.code, PC_ERR_IO);
    ASSERT_TRUE(read_sc == nullptr);

    cleanup_doc(doc);
  }

  // Test 4: FreeNullIsSafe
  { pc_sidecar_free(nullptr); }

  // Test 5: CorruptedSidecarHandledGracefully
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    char sidecar_path[4096];
    snprintf(sidecar_path, sizeof(sidecar_path), "%s.tynypdf.json", doc);
    FILE* f = fopen(sidecar_path, "w");
    ASSERT_TRUE(f != nullptr);
    fprintf(f, "{ invalid json }");
    fclose(f);

    pc_sidecar* read_sc = nullptr;
    pc_status s = pc_sidecar_read(doc, &read_sc);
    if (s.code == PC_ERR_NONE && read_sc) {
      pc_sidecar_free(read_sc);
    }

    cleanup_doc(doc);
  }

  // Test 6: RoundTripPreservesData
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup("deadbeefcafe");
    sc.document_path = strdup(doc);
    sc.modified_time = 1700000000;
    sc.page_count = 10;
    sc.annotations_json = strdup("[{\"id\":\"abcdefghij\",\"rect\":[0,0,100,100]}]");
    sc.unknown_json = strdup("{\"custom\":\"data\"}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    pc_status s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    pc_sidecar* read_sc = nullptr;
    s = pc_sidecar_read(doc, &read_sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_TRUE(read_sc != nullptr);

    ASSERT_STREQ(read_sc->document_sha256, "deadbeefcafe");
    ASSERT_EQ(read_sc->page_count, 10);
    ASSERT_STREQ(read_sc->annotations_json, "[{\"id\":\"abcdefghij\",\"rect\":[0,0,100,100]}]");
    ASSERT_STREQ(read_sc->unknown_json, "{\"custom\":\"data\"}");

    pc_sidecar_free(read_sc);
    FREE_SC_FIELDS(sc);
    cleanup_doc(doc);
  }

  fprintf(stderr, "PASSED: %d, FAILED: %d\n", tests_passed, tests_failed);
  return tests_failed ? 1 : 0;
}