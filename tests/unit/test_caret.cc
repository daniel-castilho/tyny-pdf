#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <cstdint>

// R16.9

#include "caret.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "FAIL: %s:%d: %s == %s (%zu != %zu)\n", __FILE__, __LINE__, #a, #b, (a), (b)); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

static void test_caret_left_a_acute(void) {
  std::string s = "a\u0301";
  ASSERT_EQ(caret_left(s.c_str(), s.size(), 2), (size_t)0);
  ASSERT_EQ(caret_left(s.c_str(), s.size(), 1), (size_t)0);
}

static void test_caret_right_a_acute(void) {
  std::string s = "a\u0301";
  ASSERT_EQ(caret_right(s.c_str(), s.size(), 0), (size_t)2);
  ASSERT_EQ(caret_right(s.c_str(), s.size(), 1), (size_t)2);
}

static void test_caret_left_c_cedilla(void) {
  std::string s = "c\u0327";
  ASSERT_EQ(caret_left(s.c_str(), s.size(), 2), (size_t)0);
}

static void test_caret_right_c_cedilla(void) {
  std::string s = "c\u0327";
  ASSERT_EQ(caret_right(s.c_str(), s.size(), 0), (size_t)2);
}

static void test_caret_a_tilde_o(void) {
  std::string s = "a\u0303o";
  ASSERT_EQ(caret_left(s.c_str(), s.size(), 3), (size_t)2);
  ASSERT_EQ(caret_right(s.c_str(), s.size(), 2), (size_t)3);
}

static void test_caret_cafe_acute(void) {
  std::string s = "cafe\u0301";
  auto breaks = grapheme_breaks(s.c_str(), s.size());
  printf("DEBUG cafe\u0301 breaks: ");
  for (size_t b : breaks) printf("%zu ", b);
  printf("\n");
  // caret_left at end (adjusted pos 5) should go to 3 (before e\u0301 cluster)
  ASSERT_EQ(caret_left(s.c_str(), s.size(), 5), (size_t)3);
  // caret_right at 3 (after f) should go to 5 (after e\u0301 cluster)
  ASSERT_EQ(caret_right(s.c_str(), s.size(), 3), (size_t)5);
}

static void test_caret_cafe(void) {
  std::string s = "cafe";
  ASSERT_EQ(caret_left(s.c_str(), s.size(), 4), (size_t)3);
  ASSERT_EQ(caret_right(s.c_str(), s.size(), 3), (size_t)4);
}

int main(void) {
  test_caret_left_a_acute();
  test_caret_right_a_acute();
  test_caret_left_c_cedilla();
  test_caret_right_c_cedilla();
  test_caret_a_tilde_o();
  test_caret_cafe_acute();
  test_caret_cafe();

  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}