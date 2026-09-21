// R6.1: No view state (open page, zoom, sidebar, scroll, window size)
// R6.2: No document text, page geometry, or credentials
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

// Helper: write doc content and compute real fingerprint
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
  // Test 1: RejectsViewState_R6_1
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    // Write doc and get real fingerprint
    char* fingerprint = setup_doc_with_fingerprint(doc, "test content");
    ASSERT_TRUE(fingerprint != nullptr);

    const char* view_state_keys[] = {"open_page", "zoom",        "sidebar",
                                     "scroll",    "window_size", "scroll_position",
                                     "scroll_x",  "scroll_y"};

    for (int i = 0; i < 8; ++i) {
      pc_sidecar sc = {};
      sc.format_version = 1;
      sc.document_sha256 = strdup(fingerprint);
      sc.document_path = strdup(doc);
      sc.modified_time = time(nullptr);
      sc.page_count = 1;
      sc.annotations_json = strdup("[]");
      char unknown[256];
      snprintf(unknown, sizeof(unknown), "{\"%s\":123}", view_state_keys[i]);
      sc.unknown_json = strdup(unknown);
      sc.is_stale = 0;
      sc.stale_reason = PC_STALE_NONE;

      pc_status s = pc_sidecar_write(doc, &sc);
      ASSERT_EQ(s.code, PC_ERR_ARGUMENT);

      FREE_SC_FIELDS(sc);
    }

    free(fingerprint);
    cleanup_doc(doc);
  }

  // Test 2: RejectsDocumentTextAndGeometry_R6_2
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    char* fingerprint = setup_doc_with_fingerprint(doc, "test content");
    ASSERT_TRUE(fingerprint != nullptr);

    const char* excluded_keys[] = {"page_geometry", "text_content", "credential", "passphrase",
                                   "document_text", "page_text",    "font_data",  "signing_key"};

    for (int i = 0; i < 8; ++i) {
      pc_sidecar sc = {};
      sc.format_version = 1;
      sc.document_sha256 = strdup(fingerprint);
      sc.document_path = strdup(doc);
      sc.modified_time = time(nullptr);
      sc.page_count = 1;
      sc.annotations_json = strdup("[]");
      char unknown[256];
      snprintf(unknown, sizeof(unknown), "{\"%s\":\"secret\"}", excluded_keys[i]);
      sc.unknown_json = strdup(unknown);
      sc.is_stale = 0;
      sc.stale_reason = PC_STALE_NONE;

      pc_status s = pc_sidecar_write(doc, &sc);
      ASSERT_EQ(s.code, PC_ERR_ARGUMENT);

      FREE_SC_FIELDS(sc);
    }

    free(fingerprint);
    cleanup_doc(doc);
  }

  // Test 3: AcceptsValidAnnotations
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    char* fingerprint = setup_doc_with_fingerprint(doc, "test content");
    ASSERT_TRUE(fingerprint != nullptr);

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup(fingerprint);
    sc.document_path = strdup(doc);
    sc.modified_time = time(nullptr);
    sc.page_count = 1;
    sc.annotations_json = strdup(
        "[{\"id\":\"abcdefghij\",\"type\":\"text\",\"rect\":[0,0,100,100],\"contents\":\"Note\"}]");
    sc.unknown_json = strdup("{}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    pc_status s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    FREE_SC_FIELDS(sc);
    free(fingerprint);
    cleanup_doc(doc);
  }

  // Test 4: ValidSidecarStructure
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    char* fingerprint = setup_doc_with_fingerprint(doc, "test content");
    ASSERT_TRUE(fingerprint != nullptr);

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup(fingerprint);
    sc.document_path = strdup(doc);
    sc.modified_time = time(nullptr);
    sc.page_count = 1;
    sc.annotations_json = strdup("[]");
    sc.unknown_json = strdup("{\"allowed_custom\":\"value\"}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    pc_status s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    pc_sidecar* read_sc = nullptr;
    s = pc_sidecar_read(doc, &read_sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);
    ASSERT_TRUE(read_sc != nullptr);

    ASSERT_EQ(read_sc->format_version, 1);
    ASSERT_STREQ(read_sc->document_path, doc);
    ASSERT_STREQ(read_sc->document_sha256, fingerprint);
    ASSERT_EQ(read_sc->page_count, 1);
    ASSERT_STREQ(read_sc->annotations_json, "[]");
    ASSERT_STREQ(read_sc->unknown_json, "{\"allowed_custom\":\"value\"}");
    ASSERT_EQ(read_sc->is_stale, 0);
    ASSERT_EQ(read_sc->stale_reason, PC_STALE_NONE);

    pc_sidecar_free(read_sc);
    FREE_SC_FIELDS(sc);
    free(fingerprint);
    cleanup_doc(doc);
  }

  fprintf(stderr, "PASSED: %d, FAILED: %d\n", tests_passed, tests_failed);
  return tests_failed ? 1 : 0;
}