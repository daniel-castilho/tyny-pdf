#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

// R16.1 R16.2 R16.3
std::string normalize_for_search(std::string_view);
std::string normalize_for_anchor(std::string_view);

static int tests_passed = 0;
static int tests_failed = 0;

#define STR(s) std::string(s)

#define ASSERT_STREQ(a, b) do { \
    std::string _a = STR(a); \
    std::string _b = STR(b); \
    if (_a != _b) { \
        fprintf(stderr, "FAIL: %s:%d: %s == %s ('%s' != '%s')\n", __FILE__, __LINE__, #a, #b, _a.c_str(), _b.c_str()); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

static void test_nfkc_e_acute(void) {
  std::string s = normalize_for_search("e\u0301");
  ASSERT_STREQ(s, STR("\u00e9"));
}

static void test_nfkc_c_cedilla(void) {
  std::string s = normalize_for_search("c\u0327");
  ASSERT_STREQ(s, STR("\u00e7"));
}

static void test_nfkc_a_tilde(void) {
  std::string s = normalize_for_search("a\u0303");
  ASSERT_STREQ(s, STR("\u00e3"));
}

static void test_casefold_whitespace(void) {
  std::string s = normalize_for_search("  Hello   \n WORLD ");
  ASSERT_STREQ(s, STR("hello world"));
}

static void test_anchor_same_as_search(void) {
  std::string s1 = normalize_for_search("Hello World");
  std::string s2 = normalize_for_anchor("Hello World");
  ASSERT_STREQ(s1, s2);
}

int main(void) {
  test_nfkc_e_acute();
  test_nfkc_c_cedilla();
  test_nfkc_a_tilde();
  test_casefold_whitespace();
  test_anchor_same_as_search();

  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}