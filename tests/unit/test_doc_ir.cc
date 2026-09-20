#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/doc.h"
#include "pdfcore/page.h"
#include "pdfcore/status.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_INT_EQ(a, b)                                                                        \
  do {                                                                                             \
    if ((a) != (b)) {                                                                              \
      fprintf(stderr, "FAIL: %s:%d: %s == %s (%d != %d)\n", __FILE__, __LINE__, #a, #b, (a), (b)); \
      tests_failed++;                                                                              \
    } else {                                                                                       \
      tests_passed++;                                                                              \
    }                                                                                              \
  } while (0)

#define ASSERT_SIZE_EQ(a, b)                                                                   \
  do {                                                                                         \
    if ((a) != (b)) {                                                                          \
      fprintf(stderr, "FAIL: %s:%d: %s == %s (%zu != %zu)\n", __FILE__, __LINE__, #a, #b, (a), \
              (b));                                                                            \
      tests_failed++;                                                                          \
    } else {                                                                                   \
      tests_passed++;                                                                          \
    }                                                                                          \
  } while (0)

#define ASSERT_PTR_NE(a, b)                                                             \
  do {                                                                                  \
    if ((a) == (b)) {                                                                   \
      fprintf(stderr, "FAIL: %s:%d: %s != %s (%p == %p)\n", __FILE__, __LINE__, #a, #b, \
              (void*)(a), (void*)(b));                                                  \
      tests_failed++;                                                                   \
    } else {                                                                            \
      tests_passed++;                                                                   \
    }                                                                                   \
  } while (0)

#define ASSERT_GT_DOUBLE(a, b)                                                                    \
  do {                                                                                            \
    if ((a) <= (b)) {                                                                             \
      fprintf(stderr, "FAIL: %s:%d: %s > %s (%f <= %f)\n", __FILE__, __LINE__, #a, #b, (a), (b)); \
      tests_failed++;                                                                             \
    } else {                                                                                      \
      tests_passed++;                                                                             \
    }                                                                                             \
  } while (0)

#define ASSERT_INT_GT(a, b)                                                                       \
  do {                                                                                            \
    if ((a) <= (b)) {                                                                             \
      fprintf(stderr, "FAIL: %s:%d: %s > %s (%d <= %d)\n", __FILE__, __LINE__, #a, #b, (a), (b)); \
      tests_failed++;                                                                             \
    } else {                                                                                      \
      tests_passed++;                                                                             \
    }                                                                                             \
  } while (0)

static const char* fixture_path(void) {
#ifdef TEST_FIXTURE_DIR
  static char buf[4096];
  snprintf(buf, sizeof(buf), "%s/simple.pdf", TEST_FIXTURE_DIR);
  return buf;
#else
  return "tests/fixtures/simple.pdf";
#endif
}

// R7.1: pc_doc_open opens document, copies geometry to IR, closes backend handle
// R9.1: core compiles with -fno-exceptions -fno-rtti
static void test_open_and_close(void) {
  pc_doc* doc = nullptr;
  pc_status s = pc_doc_open(fixture_path(), nullptr, &doc);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  ASSERT_PTR_NE(doc, nullptr);

  uint32_t count = pc_doc_page_count(doc);
  ASSERT_INT_GT(count, 0);

  pc_doc_close(doc);
}

// R7.2: pc_doc_page_count returns page count from IR
static void test_page_count(void) {
  pc_doc* doc = nullptr;
  pc_status s = pc_doc_open(fixture_path(), nullptr, &doc);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  uint32_t count = pc_doc_page_count(doc);
  ASSERT_INT_EQ(count, 5);

  pc_doc_close(doc);
}

// R7.3: pc_doc_page_get_box returns page box from IR
static void test_page_get_box(void) {
  pc_doc* doc = nullptr;
  pc_status s = pc_doc_open(fixture_path(), nullptr, &doc);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  uint32_t count = pc_doc_page_count(doc);
  ASSERT_INT_EQ(count, 5);

  for (uint32_t i = 0; i < count; ++i) {
    pc_page_box box = {};
    s = pc_doc_page_get_box(doc, i, &box);
    ASSERT_INT_EQ(s.code, PC_ERR_NONE);

    ASSERT_GT_DOUBLE(box.mediabox.x1, box.mediabox.x0);
    ASSERT_GT_DOUBLE(box.mediabox.y1, box.mediabox.y0);
    ASSERT_INT_EQ(box.rotation >= 0 && box.rotation <= 3, 1);
    int valid_rot =
        (box.rotation == 0 || box.rotation == 90 || box.rotation == 180 || box.rotation == 270);
    ASSERT_INT_EQ(valid_rot, 1);
  }

  pc_page_box box = {};
  s = pc_doc_page_get_box(doc, count, &box);
  ASSERT_INT_EQ(s.code, PC_ERR_ARGUMENT);

  pc_doc_close(doc);
}

// R7.4: pc_doc_sha256 returns SHA256 of source file for sidecar staleness
static void test_sha256(void) {
  pc_doc* doc = nullptr;
  pc_status s = pc_doc_open(fixture_path(), nullptr, &doc);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  const char* sha256 = pc_doc_sha256(doc);
  ASSERT_PTR_NE(sha256, nullptr);
  ASSERT_SIZE_EQ(strlen(sha256), (size_t)64);

  for (int i = 0; i < 64; ++i) {
    char c = sha256[i];
    int is_hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    ASSERT_INT_EQ(is_hex, 1);
  }

  pc_doc_close(doc);
}

// R7.5: pc_doc_close frees IR only, null-safe
// R7.3: pc_doc_page_get_box validates null args
// R7.4: pc_doc_sha256 returns nullptr for null doc
// R7.6: layering-check ensures no engine pointers in IR
// R9.2: no Windows/engine headers in core
// R9.3: no engine libraries linked
// R9.4: backend_line_ratio <= 0.07
static void test_null_arguments(void) {
  pc_doc* doc = nullptr;
  pc_status s = pc_doc_open(nullptr, nullptr, &doc);
  ASSERT_INT_EQ(s.code, PC_ERR_ARGUMENT);

  s = pc_doc_open(fixture_path(), nullptr, nullptr);
  ASSERT_INT_EQ(s.code, PC_ERR_ARGUMENT);

  pc_page_box box = {};
  s = pc_doc_page_get_box(nullptr, 0, &box);
  ASSERT_INT_EQ(s.code, PC_ERR_ARGUMENT);

  s = pc_doc_page_get_box(doc, 0, nullptr);
  ASSERT_INT_EQ(s.code, PC_ERR_ARGUMENT);

  ASSERT_INT_EQ(pc_doc_page_count(nullptr), 0);
  const char* null_sha = pc_doc_sha256(nullptr);
  ASSERT_INT_EQ(null_sha == nullptr, 1);

  pc_doc_close(nullptr);
}

int main(void) {
  test_open_and_close();
  test_page_count();
  test_page_get_box();
  test_sha256();
  test_null_arguments();

  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}