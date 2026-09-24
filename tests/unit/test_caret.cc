// Story 4.4 (R23.1-R23.3) / Story 6.2 (R33.1): caret movement over one grapheme cluster, headless.
// `e + U+0301` moves in ONE step, the caret never stops inside a combining
// sequence, inside an ABNT2 `~ + a` composition, or between a letter and its
// hyphen. No backend is linked: the stop set is the cluster starts from
// break.h plus the end of the buffer.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/status.h"
#include "pdfcore/text.h"

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

#define ASSERT_PC_EXPECTED(expected, got)                                               \
  do {                                                                                  \
    if ((got).code != (expected)) {                                                     \
      fprintf(stderr, "FAIL: %s:%d: code %u (want %d) detail=%s\n", __FILE__, __LINE__, \
              (got).code, (expected), (got).detail ? (got).detail : "-");               \
      tests_failed++;                                                                   \
    } else {                                                                            \
      tests_passed++;                                                                   \
    }                                                                                   \
  } while (0)

static const char* abnt2_fixture_path(void) {
#ifdef TEST_FIXTURE_DIR
  static char buf[4096];
  snprintf(buf, sizeof(buf), "%s/text/abnt2-golden.txt", TEST_FIXTURE_DIR);
  return buf;
#else
  return "tests/fixtures/text/abnt2-golden.txt";
#endif
}

static uint32_t caret_left_at(const char* s, uint32_t len, uint32_t pos) {
  uint32_t out = 0xDEADBEEFu;
  pc_status st = pc_caret_left(s, len, pos, &out);
  ASSERT_PC_EXPECTED(PC_ERR_NONE, st);
  return out;
}

static uint32_t caret_right_at(const char* s, uint32_t len, uint32_t pos) {
  uint32_t out = 0xDEADBEEFu;
  pc_status st = pc_caret_right(s, len, pos, &out);
  ASSERT_PC_EXPECTED(PC_ERR_NONE, st);
  return out;
}

// R23.1/R23.2: `e + U+0301` is crossed in ONE step, left and right - the caret
// path over the pair is [0,2] per direction, never [0,1,2].
static void test_combining_one_step(void) {
  static const char pair[] = "e\xcc\x81";
  const uint32_t len = (uint32_t)(sizeof(pair) - 1);
  ASSERT_INT_EQ((int)caret_left_at(pair, len, len), 0);
  ASSERT_INT_EQ((int)caret_right_at(pair, len, 0), (int)len);
  ASSERT_INT_EQ((int)caret_left_at(pair, len, 0), 0);            // holds at the start
  ASSERT_INT_EQ((int)caret_right_at(pair, len, len), (int)len);  // holds at the end
  // From inside the cluster the caret exits it, never lands on the mark.
  ASSERT_INT_EQ((int)caret_left_at(pair, len, 1), 0);
  ASSERT_INT_EQ((int)caret_right_at(pair, len, 1), (int)len);
}

// R23.2: `a + U+0303` + `o` - the stop after the tilde sits at byte 3; the
// caret never stops at bytes 1 or 2 (inside the combining sequence).
static void test_no_stop_inside_combining(void) {
  static const char s[] = "a\xcc\x83o";
  ASSERT_INT_EQ((int)caret_left_at(s, 4, 4), 3);
  ASSERT_INT_EQ((int)caret_left_at(s, 4, 3), 0);
  ASSERT_INT_EQ((int)caret_right_at(s, 4, 0), 3);
  ASSERT_INT_EQ((int)caret_right_at(s, 4, 3), 4);
  ASSERT_INT_EQ((int)caret_left_at(s, 4, 1), 0);
  ASSERT_INT_EQ((int)caret_right_at(s, 4, 1), 3);
  ASSERT_INT_EQ((int)caret_left_at(s, 4, 2), 0);
  ASSERT_INT_EQ((int)caret_right_at(s, 4, 2), 3);
}

// R21.3 at the caret level: the hyphen is crossed with its letter - one step
// from `a` to `c`, never a stop at the hyphen.
static void test_hyphen_one_step(void) {
  static const char s[] = "guarda-chuva";
  ASSERT_INT_EQ((int)caret_right_at(s, 12, 5), 7);
  ASSERT_INT_EQ((int)caret_left_at(s, 12, 7), 5);
}

// R23.3: an ABNT2 `~ + a` composition is one run - one step across it, both
// directions; a standalone tilde (tilde + space) is its own cluster.
static void test_abnt2_composition(void) {
  static const char comp[] = "~a";
  ASSERT_INT_EQ((int)caret_right_at(comp, 2, 0), 2);
  ASSERT_INT_EQ((int)caret_left_at(comp, 2, 2), 0);

  static const char alone[] = "~ ";
  ASSERT_INT_EQ((int)caret_right_at(alone, 2, 0), 1);
  ASSERT_INT_EQ((int)caret_right_at(alone, 2, 1), 2);
}

// The ABNT2 fixture, end to end: for every byte position of every line the
// caret stops only on cluster starts or at the end - never inside a
// composition, never inside a combining sequence.
static void test_abnt2_fixture_stop_set(void) {
  FILE* f = fopen(abnt2_fixture_path(), "r");
  if (!f) {
    fprintf(stderr, "FAIL: cannot open fixture %s\n", abnt2_fixture_path());
    tests_failed++;
    return;
  }
  char line[1024];
  while (fgets(line, sizeof(line), f)) {
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
    pc_text_boundary* b = nullptr;
    uint32_t count = 0;
    pc_status s = pc_text_break_positions(line, (uint32_t)n, &b, &count);
    if (s.code != PC_ERR_NONE) {
      fprintf(stderr, "FAIL: break on fixture line: %u %s\n", s.code, s.detail ? s.detail : "-");
      tests_failed++;
      return;
    }
    for (uint32_t pos = 0; pos <= (uint32_t)n; ++pos) {
      uint32_t left = caret_left_at(line, (uint32_t)n, pos);
      int left_ok = 0;
      if (left == 0) {
        left_ok = 1;
      }
      for (uint32_t i = 0; i < count; ++i) {
        if (b[i].byte_offset == left) {
          left_ok = 1;
        }
      }
      if (!left_ok) {
        fprintf(stderr, "FAIL: caret_left(%u) on \"%s\" stopped at %u (not a cluster start)\n", pos,
                line, left);
        tests_failed++;
      } else {
        tests_passed++;
      }
      uint32_t right = caret_right_at(line, (uint32_t)n, pos);
      int right_ok = (right == (uint32_t)n);
      for (uint32_t i = 0; i < count; ++i) {
        if (b[i].byte_offset == right) {
          right_ok = 1;
        }
      }
      if (!right_ok) {
        fprintf(stderr, "FAIL: caret_right(%u) on \"%s\" stopped at %u (not a cluster start)\n",
                pos, line, right);
        tests_failed++;
      } else {
        tests_passed++;
      }
    }
    pc_text_boundary_free(b, count);
  }
  fclose(f);
}

// Hostile input reports, never traps (ADR-0003).
static void test_arguments(void) {
  uint32_t out = 0;
  pc_status s = pc_caret_left(nullptr, 1, 0, &out);
  ASSERT_PC_EXPECTED(PC_ERR_ARGUMENT, s);
  s = pc_caret_left("a", 1, 0, nullptr);
  ASSERT_PC_EXPECTED(PC_ERR_ARGUMENT, s);
  s = pc_caret_left("a", 1, 2, &out);  // beyond the buffer
  ASSERT_PC_EXPECTED(PC_ERR_ARGUMENT, s);
  s = pc_caret_right("a", 1, 2, &out);
  ASSERT_PC_EXPECTED(PC_ERR_ARGUMENT, s);

  static const char* bad_seq[] = {
      "\xc0\xaf",
      "\xed\xa0\x80",
      "\x80",
  };
  for (size_t i = 0; i < sizeof(bad_seq) / sizeof(bad_seq[0]); ++i) {
    s = pc_caret_left(bad_seq[i], 3, 0, &out);
    ASSERT_PC_EXPECTED(PC_ERR_ARGUMENT, s);
    s = pc_caret_right(bad_seq[i], 3, 0, &out);
    ASSERT_PC_EXPECTED(PC_ERR_ARGUMENT, s);
  }
}

int main(void) {
  test_combining_one_step();
  test_no_stop_inside_combining();
  test_hyphen_one_step();
  test_abnt2_composition();
  test_abnt2_fixture_stop_set();
  test_arguments();

  printf("test_caret: %d passed, %d failed\n", tests_passed, tests_failed);
  return tests_failed ? 1 : 0;
}
