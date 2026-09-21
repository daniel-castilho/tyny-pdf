#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// R16.10 R16.11

#include "pdfcore/doc.h"
#include "pdfcore/page.h"
#include "pdfcore/status.h"
#include "pdfcore/text.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_INT_EQ(a, b) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "FAIL: %s:%d: %s == %s (%d != %d)\n", __FILE__, __LINE__, #a, #b, (a), (b)); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define ASSERT_PTR_NE(a, b) do { \
    if ((a) == (b)) { \
        fprintf(stderr, "FAIL: %s:%d: %s != %s (%p == %p)\n", __FILE__, __LINE__, #a, #b, (void*)(a), (void*)(b)); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

static const char* fixture_path(void) {
#ifdef TEST_FIXTURE_DIR
  static char buf[4096];
  snprintf(buf, sizeof(buf), "%s/simple.pdf", TEST_FIXTURE_DIR);
  return buf;
#else
  return "tests/fixtures/simple.pdf";
#endif
}

static void test_doc_page_text(void) {
  pc_doc* doc = nullptr;
  pc_status s = pc_doc_open(fixture_path(), nullptr, &doc);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  ASSERT_PTR_NE(doc, nullptr);

  pc_text_run* runs = nullptr;
  uint32_t count = 0;
  s = pc_doc_page_text(doc, 0, &runs, &count);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  ASSERT_INT_EQ(count, 1);
  ASSERT_PTR_NE(runs, nullptr);

  ASSERT_INT_EQ(runs[0].size > 0, 1);
  ASSERT_PTR_NE(runs[0].utf8, nullptr);

  pc_text_run_free(runs, count);
  pc_doc_close(doc);
}

static void test_doc_page_text_invalid_page(void) {
  pc_doc* doc = nullptr;
  pc_status s = pc_doc_open(fixture_path(), nullptr, &doc);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  pc_text_run* runs = nullptr;
  uint32_t count = 0;
  s = pc_doc_page_text(doc, 999, &runs, &count);
  ASSERT_INT_EQ(s.code, PC_ERR_ARGUMENT);

  pc_doc_close(doc);
}

static void test_doc_page_text_null_args(void) {
  pc_doc* doc = nullptr;
  pc_status s = pc_doc_open(fixture_path(), nullptr, &doc);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  pc_text_run* runs = nullptr;
  uint32_t count = 0;
  s = pc_doc_page_text(nullptr, 0, &runs, &count);
  ASSERT_INT_EQ(s.code, PC_ERR_ARGUMENT);

  s = pc_doc_page_text(doc, 0, nullptr, &count);
  ASSERT_INT_EQ(s.code, PC_ERR_ARGUMENT);

  s = pc_doc_page_text(doc, 0, &runs, nullptr);
  ASSERT_INT_EQ(s.code, PC_ERR_ARGUMENT);

  pc_doc_close(doc);
}

int main(void) {
  test_doc_page_text();
  test_doc_page_text_invalid_page();
  test_doc_page_text_null_args();

  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}