#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <cstdint>

// R16.4 R16.5 R16.6 R16.7 R16.8

static bool is_combining_mark(uint32_t cp) {
  return (cp >= 0x0300 && cp <= 0x036F) ||
         (cp >= 0x1AB0 && cp <= 0x1AFF) ||
         (cp >= 0x1DC0 && cp <= 0x1DFF) ||
         (cp >= 0x20D0 && cp <= 0x20FF) ||
         (cp >= 0xFE20 && cp <= 0xFE2F);
}

static uint32_t next_codepoint(const char* s, size_t len, size_t* pos) {
  if (*pos >= len) return 0;
  unsigned char c = s[*pos];
  if (c < 0x80) {
    return s[(*pos)++];
  } else if ((c & 0xE0) == 0xC0) {
    if (*pos + 1 < len) {
      uint32_t cp = ((c & 0x1F) << 6) | (s[*pos + 1] & 0x3F);
      *pos += 2;
      return cp;
    }
    return s[(*pos)++];
  } else if ((c & 0xF0) == 0xE0) {
    if (*pos + 2 < len) {
      uint32_t cp = ((c & 0x0F) << 12) | ((s[*pos + 1] & 0x3F) << 6) | (s[*pos + 2] & 0x3F);
      *pos += 3;
      return cp;
    }
    return s[(*pos)++];
  } else if ((c & 0xF8) == 0xF0) {
    if (*pos + 3 < len) {
      uint32_t cp = ((c & 0x07) << 18) | ((s[*pos + 1] & 0x3F) << 12) |
                    ((s[*pos + 2] & 0x3F) << 6) | (s[*pos + 3] & 0x3F);
      *pos += 4;
      return cp;
    }
    return s[(*pos)++];
  }
  return s[(*pos)++];
}

static std::vector<size_t> grapheme_breaks(const char* s, size_t len) {
  std::vector<size_t> breaks;
  breaks.push_back(0);
  size_t pos = 0;
  size_t adjusted_pos = 0;
  while (pos < len) {
    (void)next_codepoint(s, len, &pos);
    adjusted_pos += 1; // each base character counts as 1 in adjusted space
    while (pos < len) {
      size_t saved = pos;
      uint32_t cp = next_codepoint(s, len, &pos);
      if (is_combining_mark(cp)) {
        adjusted_pos += 1; // combining marks count as 1 in adjusted space
        continue;
      } else {
        pos = saved;
        break;
      }
    }
    breaks.push_back(adjusted_pos);
  }
  return breaks;
}

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_VECTOR_EQ(a, b) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "FAIL: %s:%d: vectors differ\n", __FILE__, __LINE__); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

static void test_a_acute(void) {
  std::string s = "a\u0301";
  auto breaks = grapheme_breaks(s.c_str(), s.size());
  std::vector<size_t> expected = {0, 2};
  ASSERT_VECTOR_EQ(breaks, expected);
}

static void test_c_cedilla(void) {
  std::string s = "c\u0327";
  auto breaks = grapheme_breaks(s.c_str(), s.size());
  std::vector<size_t> expected = {0, 2};
  ASSERT_VECTOR_EQ(breaks, expected);
}

static void test_a_tilde_o(void) {
  std::string s = "a\u0303o";
  auto breaks = grapheme_breaks(s.c_str(), s.size());
  std::vector<size_t> expected = {0, 2, 3};
  ASSERT_VECTOR_EQ(breaks, expected);
}

static void test_cafe_acute(void) {
  std::string s = "cafe\u0301";
  auto breaks = grapheme_breaks(s.c_str(), s.size());
  std::vector<size_t> expected = {0, 1, 2, 3, 5};
  ASSERT_VECTOR_EQ(breaks, expected);
}

static void test_cafe(void) {
  std::string s = "cafe";
  auto breaks = grapheme_breaks(s.c_str(), s.size());
  std::vector<size_t> expected = {0, 1, 2, 3, 4};
  ASSERT_VECTOR_EQ(breaks, expected);
}

int main(void) {
  test_a_acute();
  test_c_cedilla();
  test_a_tilde_o();
  test_cafe_acute();
  test_cafe();

  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}