// Story 4.4 (R21.1-R21.4): UAX #29 grapheme clusters with pt-BR hyphen and
// ABNT2 exceptions, headless. The break report over the whole pt-BR corpus is
// pinned byte-for-byte in tests/golden/text-break-positions.txt (approval
// pattern, same as text-fallback-faces.txt). No backend is linked: pure
// buffer arithmetic (R21.4).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/sha256.h"
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

static const char* fixture_path(void) {
#ifdef TEST_FIXTURE_DIR
  static char buf[4096];
  snprintf(buf, sizeof(buf), "%s/text/ptbr-break-golden.txt", TEST_FIXTURE_DIR);
  return buf;
#else
  return "tests/fixtures/text/ptbr-break-golden.txt";
#endif
}

static const char* golden_path(void) {
#ifdef TEST_GOLDEN_DIR
  static char buf[4096];
  snprintf(buf, sizeof(buf), "%s/text-break-positions.txt", TEST_GOLDEN_DIR);
  return buf;
#else
  return "tests/golden/text-break-positions.txt";
#endif
}

static void free_boundaries(pc_text_boundary* b, uint32_t count) {
  pc_text_boundary_free(b, count);
}

// R21.2: a combining sequence is ONE cluster - `e + U+0301` breaks as one
// unit, the caret never stops on the combining mark (UAX #29 GB9).
static void test_combining_one_cluster(void) {
  pc_text_boundary* b = nullptr;
  uint32_t count = 0;
  pc_status s = pc_text_break_positions("e\xcc\x81", 3, &b, &count);
  ASSERT_PC_EXPECTED(PC_ERR_NONE, s);
  ASSERT_INT_EQ((int)count, 1);
  ASSERT_INT_EQ((int)b[0].byte_offset, 0);
  ASSERT_INT_EQ((int)b[0].cluster_index, 0);
  free_boundaries(b, count);

  // `a + U+0303` followed by `o`: the break sits after the tilde, never
  // inside the combining sequence.
  b = nullptr;
  count = 0;
  s = pc_text_break_positions("a\xcc\x83o", 4, &b, &count);
  ASSERT_PC_EXPECTED(PC_ERR_NONE, s);
  ASSERT_INT_EQ((int)count, 2);
  ASSERT_INT_EQ((int)b[0].byte_offset, 0);
  ASSERT_INT_EQ((int)b[1].byte_offset, 3);
  free_boundaries(b, count);
}

// R21.3: the pt-BR hyphen binds to the letter it follows, so `guarda-` ends a
// cluster at the hyphen and no stop exists between letter and hyphen.
static void test_hyphen_binds(void) {
  pc_text_boundary* b = nullptr;
  uint32_t count = 0;
  pc_status s = pc_text_break_positions("guarda-chuva", 12, &b, &count);
  ASSERT_PC_EXPECTED(PC_ERR_NONE, s);
  ASSERT_INT_EQ((int)count, 11);
  for (int i = 0; i < 6; ++i) {
    ASSERT_INT_EQ((int)b[i].byte_offset, i);
  }
  ASSERT_INT_EQ((int)b[6].byte_offset, 7);  // no stop at the hyphen (byte 6)
  ASSERT_INT_EQ((int)b[10].byte_offset, 11);
  free_boundaries(b, count);
}

// R21.4: an ABNT2 `~ + a` composition is one cluster.
static void test_abnt2_composition_one_cluster(void) {
  pc_text_boundary* b = nullptr;
  uint32_t count = 0;
  pc_status s = pc_text_break_positions("~a", 2, &b, &count);
  ASSERT_PC_EXPECTED(PC_ERR_NONE, s);
  ASSERT_INT_EQ((int)count, 1);
  ASSERT_INT_EQ((int)b[0].byte_offset, 0);
  free_boundaries(b, count);

  // Tilde + space is no composition: the tilde stands alone.
  b = nullptr;
  count = 0;
  s = pc_text_break_positions("~ ", 2, &b, &count);
  ASSERT_PC_EXPECTED(PC_ERR_NONE, s);
  ASSERT_INT_EQ((int)count, 2);
  free_boundaries(b, count);
}

// UAX #29 GB3: CR followed by LF is one cluster.
static void test_crlf(void) {
  pc_text_boundary* b = nullptr;
  uint32_t count = 0;
  pc_status s = pc_text_break_positions("\r\n", 2, &b, &count);
  ASSERT_PC_EXPECTED(PC_ERR_NONE, s);
  ASSERT_INT_EQ((int)count, 1);
  free_boundaries(b, count);
}

static void test_empty_and_arguments(void) {
  pc_text_boundary* b = nullptr;
  uint32_t count = 0;
  pc_status s = pc_text_break_positions("", 0, &b, &count);
  ASSERT_PC_EXPECTED(PC_ERR_NONE, s);
  ASSERT_INT_EQ((int)count, 0);

  s = pc_text_break_positions(nullptr, 1, &b, &count);
  ASSERT_PC_EXPECTED(PC_ERR_ARGUMENT, s);
  s = pc_text_break_positions("a", 1, nullptr, &count);
  ASSERT_PC_EXPECTED(PC_ERR_ARGUMENT, s);
  s = pc_text_break_positions("a", 1, &b, nullptr);
  ASSERT_PC_EXPECTED(PC_ERR_ARGUMENT, s);

  // A hostile byte sequence is one reported failure, never a partial array
  // (ADR-0003): overlong, surrogate, stray continuation, out of range.
  static const char* bad_seq[] = {
      "\xc0\xaf", "\xe0\x80\xaf", "\xed\xa0\x80", "\x80", "\xf4\x90\x80\x80",
  };
  for (size_t i = 0; i < sizeof(bad_seq) / sizeof(bad_seq[0]); ++i) {
    b = (pc_text_boundary*)0x1;
    count = 7;
    s = pc_text_break_positions(bad_seq[i], 3, &b, &count);
    ASSERT_PC_EXPECTED(PC_ERR_ARGUMENT, s);
    ASSERT_INT_EQ((int)count, 0);
  }

  pc_text_boundary_free(nullptr, 0);
}

// R21.2 approval (V2): one `<byte_offset>:<sha256-16-hex>` line per cluster
// over the whole corpus buffer, so a rule change must be argued in the diff.
// With `--bless` the report is written to the golden path instead of compared
// (used once to create the golden, never in CI).
static void test_break_positions_golden(int bless) {
  FILE* f = fopen(fixture_path(), "rb");
  if (!f) {
    fprintf(stderr, "FAIL: cannot open fixture %s\n", fixture_path());
    tests_failed++;
    return;
  }
  static char corpus[65536];
  size_t n = fread(corpus, 1, sizeof(corpus) - 1, f);
  fclose(f);
  corpus[n] = '\0';

  pc_text_boundary* b = nullptr;
  uint32_t count = 0;
  pc_status s = pc_text_break_positions(corpus, (uint32_t)n, &b, &count);
  if (s.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: break on corpus: %u %s\n", s.code, s.detail ? s.detail : "-");
    tests_failed++;
    return;
  }

  char report[131072];
  size_t r = 0;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t start = b[i].byte_offset;
    uint32_t end = (i + 1 < count) ? b[i + 1].byte_offset : (uint32_t)n;
    pc_sha256 ctx;
    uint8_t digest[32];
    pc_sha256_init(&ctx);
    pc_sha256_update(&ctx, (const uint8_t*)(corpus + start), end - start);
    pc_sha256_final(&ctx, digest);
    int w = snprintf(report + r, sizeof(report) - r, "%u:%02x%02x%02x%02x%02x%02x%02x%02x\n", start,
                     digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6],
                     digest[7]);
    if (w < 0 || (size_t)w >= sizeof(report) - r) {
      fprintf(stderr, "FAIL: report overflow\n");
      tests_failed++;
      pc_text_boundary_free(b, count);
      return;
    }
    r += (size_t)w;
  }
  report[r] = '\0';
  pc_text_boundary_free(b, count);

  if (bless) {
    FILE* g = fopen(golden_path(), "wb");
    if (!g) {
      fprintf(stderr, "FAIL: cannot write golden %s\n", golden_path());
      tests_failed++;
      return;
    }
    fwrite(report, 1, r, g);
    fclose(g);
    printf("blessed %s (%lu bytes)\n", golden_path(), (unsigned long)r);
    return;
  }

  FILE* gold = fopen(golden_path(), "rb");
  if (!gold) {
    fprintf(stderr, "GOLDEN MISSING - %s\nreport was:\n%s", golden_path(), report);
    tests_failed++;
    return;
  }
  char golden[131072] = {0};
  size_t gn = fread(golden, 1, sizeof(golden) - 1, gold);
  golden[gn] = '\0';
  fclose(gold);

  if (strcmp(golden, report) != 0) {
    fprintf(stderr, "GOLDEN MISMATCH\ngolden:\n%s\nreport:\n%s\n", golden, report);
    tests_failed++;
  } else {
    tests_passed++;
  }
}

int main(int argc, char** argv) {
  int bless = (argc > 1 && strcmp(argv[1], "--bless") == 0);
  test_combining_one_cluster();
  test_hyphen_binds();
  test_abnt2_composition_one_cluster();
  test_crlf();
  test_empty_and_arguments();
  test_break_positions_golden(bless);

  printf("test_text_break: %d passed, %d failed\n", tests_passed, tests_failed);
  return tests_failed ? 1 : 0;
}
