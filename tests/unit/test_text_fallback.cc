// Story 4.3 (R20.1-R20.4): font fallback per run, no tofu. The null backend declares
// PC_CAP_FACE_COVERAGE off (R-M5 -> capability not supported, never "nothing to cover");
// the mupdf backend answers per-face coverage from its curated face table, and the fixture
// report is pinned byte-for-byte in tests/golden/text-fallback-faces.txt (approval pattern,
// same as status_enum.txt / sidecar-stale-report.txt).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/backend.h"
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

#define ASSERT_STR_EQ(a, b)                                                                     \
  do {                                                                                          \
    if (strcmp((a), (b)) != 0) {                                                                \
      fprintf(stderr, "FAIL: %s:%d: %s == %s (\"%s\" != \"%s\")\n", __FILE__, __LINE__, #a, #b, \
              (a), (b));                                                                        \
      tests_failed++;                                                                           \
    } else {                                                                                    \
      tests_passed++;                                                                           \
    }                                                                                           \
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
  snprintf(buf, sizeof(buf), "%s/text/fallback-ptbr.txt", TEST_FIXTURE_DIR);
  return buf;
#else
  return "tests/fixtures/text/fallback-ptbr.txt";
#endif
}

static const char* golden_path(void) {
#ifdef TEST_GOLDEN_DIR
  static char buf[4096];
  snprintf(buf, sizeof(buf), "%s/text-fallback-faces.txt", TEST_GOLDEN_DIR);
  return buf;
#else
  return "tests/golden/text-fallback-faces.txt";
#endif
}

// Decode one UTF-8 codepoint at `in`; returns its length or 0 on invalid input. The report and the
// golden must never depend on invisible bytes, so non-ASCII is escaped as U+XXXX in codepoint
// space (the fixture was validated by fallback, so this decoder only needs to be correct here).
static size_t decode_cp(const unsigned char* in, uint32_t* out_cp) {
  size_t len = 0;
  uint32_t cp = 0;
  if (in[0] < 0x80) {
    cp = in[0];
    len = 1;
  } else if ((in[0] & 0xE0) == 0xC0) {
    cp = in[0] & 0x1F;
    len = 2;
  } else if ((in[0] & 0xF0) == 0xE0) {
    cp = in[0] & 0x0F;
    len = 3;
  } else if ((in[0] & 0xF8) == 0xF0) {
    cp = in[0] & 0x07;
    len = 4;
  } else {
    return 0;
  }
  for (size_t i = 1; i < len; ++i) {
    if ((in[i] & 0xC0) != 0x80)
      return 0;
    cp = (cp << 6) | (in[i] & 0x3F);
  }
  *out_cp = cp;
  return len;
}

// Escape a run's bytes into the stable report form: printable ASCII verbatim, everything else
// as U+XXXX in codepoint space so combining marks stay visible and the golden never depends on
// invisible bytes.
static void escape_run(char* out, size_t cap, const char* s) {
  size_t o = 0;
  const unsigned char* p = (const unsigned char*)s;
  while (*p && o + 9 < cap) {
    if (*p >= 0x20 && *p < 0x7F) {
      out[o++] = (char)*p++;
    } else {
      uint32_t cp = 0;
      size_t n = decode_cp(p, &cp);
      if (n == 0)
        break;
      int w = snprintf(out + o, cap - o, "U+%04X", cp);
      if (w < 0)
        break;
      o += (size_t)w;
      p += n;
    }
  }
  out[o] = '\0';
}

#ifdef PC_HAVE_MUPDF
// R20.1/R20.2/R13.3: mupdf backend splits the pt-BR fixture into runs, each pinned to a face id,
// and the whole report matches the golden byte-for-byte (a face-list reorder or a probing change
// must be argued in the diff).
static int test_mupdf_runs_golden(void) {
  const pc_backend_api* api = pc_mupdf_backend_get_api();
  void* doc = nullptr;
  pc_status s = api->doc_open(fixture_path(), nullptr, &doc);
  if (s.code != PC_ERR_NONE || !doc) {
    fprintf(stderr, "mupdf doc_open failed: %u %s\n", s.code, s.detail ? s.detail : "-");
    return 1;
  }

  FILE* f = fopen(fixture_path(), "r");
  if (!f) {
    fprintf(stderr, "cannot open fixture\n");
    api->doc_close(doc);
    return 1;
  }

  char report[8192];
  size_t r = 0;
  char line[1024];
  while (fgets(line, sizeof(line), f)) {
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
    pc_text_run* runs = nullptr;
    uint32_t count = 0;
    s = pc_text_fallback_runs(api, doc, line, &runs, &count);
    if (s.code != PC_ERR_NONE) {
      fprintf(stderr, "fallback on line failed: %u %s\n", s.code, s.detail ? s.detail : "-");
      fclose(f);
      pc_text_run_free(runs, count);
      api->doc_close(doc);
      return 1;
    }
    char esc_line[1024];
    escape_run(esc_line, sizeof(esc_line), line);
    int w = snprintf(report + r, sizeof(report) - r, "%s ->", esc_line);
    if (w < 0 || (size_t)w >= sizeof(report) - r) {
      fprintf(stderr, "report overflow\n");
      fclose(f);
      pc_text_run_free(runs, count);
      api->doc_close(doc);
      return 1;
    }
    r += (size_t)w;
    for (uint32_t i = 0; i < count; ++i) {
      char esc[128];
      escape_run(esc, sizeof(esc), runs[i].utf8);
      int w2 = snprintf(report + r, sizeof(report) - r, " face%u=\"%s\"", runs[i].face, esc);
      if (w2 < 0 || (size_t)w2 >= sizeof(report) - r) {
        fprintf(stderr, "report overflow\n");
        fclose(f);
        pc_text_run_free(runs, count);
        api->doc_close(doc);
        return 1;
      }
      r += (size_t)w2;
    }
    report[r++] = '\n';
    pc_text_run_free(runs, count);
  }
  fclose(f);
  report[r] = '\0';
  api->doc_close(doc);

  FILE* gold = fopen(golden_path(), "r");
  if (!gold) {
    fprintf(stderr, "GOLDEN MISSING - %s\nreport was:\n%s", golden_path(), report);
    tests_failed++;
    return 0;
  }
  char golden[8192] = {0};
  size_t gn = fread(golden, 1, sizeof(golden) - 1, gold);
  golden[gn] = '\0';
  fclose(gold);

  if (strcmp(golden, report) != 0) {
    fprintf(stderr, "GOLDEN MISMATCH\ngolden:\n%s\nreport:\n%s\n", golden, report);
    tests_failed++;
  } else {
    tests_passed++;
  }
  return 0;
}

// R20.x: a codepoint no face draws fails all-or-nothing with the exact detail "missing glyph
// U+XXXX" and no partial runs (never tofu). U+0378 is unassigned in Unicode 16.
static void test_missing_glyph(void) {
  const pc_backend_api* api = pc_mupdf_backend_get_api();
  void* doc = nullptr;
  pc_status s = api->doc_open(fixture_path(), nullptr, &doc);
  ASSERT_PC_EXPECTED(PC_ERR_NONE, s);

  static const char* bad =
      "a\xcd\xb8"
      "b";
  pc_text_run* runs = nullptr;
  uint32_t count = 0;
  s = pc_text_fallback_runs(api, doc, bad, &runs, &count);
  ASSERT_PC_EXPECTED(PC_ERR_LIMIT, s);
  ASSERT_STR_EQ(s.detail, "missing glyph U+0378");
  ASSERT_INT_EQ((int)(size_t)runs, 0);
  ASSERT_INT_EQ((int)count, 0);

  api->doc_close(doc);
}

// R20.x: invalid UTF-8 is a reported failure, never a mangled run (ADR-0003: hostile input
// reports, never traps).
static void test_invalid_utf8(void) {
  const pc_backend_api* api = pc_mupdf_backend_get_api();
  void* doc = nullptr;
  pc_status s = api->doc_open(fixture_path(), nullptr, &doc);
  ASSERT_PC_EXPECTED(PC_ERR_NONE, s);

  static const char* bad_seq[] = {
      "\xc0\xaf",          // overlong 2-byte
      "\xe0\x80\xaf",      // overlong 3-byte
      "\xed\xa0\x80",      // UTF-16 surrogate
      "\x80",              // stray continuation
      "\xf4\x90\x80\x80",  // > U+10FFFF
  };
  pc_text_run* runs = nullptr;
  uint32_t count = 0;
  for (size_t i = 0; i < sizeof(bad_seq) / sizeof(bad_seq[0]); ++i) {
    s = pc_text_fallback_runs(api, doc, bad_seq[i], &runs, &count);
    ASSERT_PC_EXPECTED(PC_ERR_ARGUMENT, s);
    ASSERT_INT_EQ((int)(size_t)runs, 0);
  }

  api->doc_close(doc);
}
#endif  // PC_HAVE_MUPDF

// R20.3/R-M5 (and null R11.3): a backend that does not declare PC_CAP_FACE_COVERAGE must report
// capability not supported, never an empty "nothing to cover" that would render tofu in a strict
// sequence.
static void test_null_capability(void) {
  const pc_backend_api* api = pc_null_backend_get_api();
  void* doc = nullptr;
  pc_status s = api->doc_open("synthetic.pdf", nullptr, &doc);
  ASSERT_PC_EXPECTED(PC_ERR_NONE, s);

  pc_text_run* runs = nullptr;
  uint32_t count = 0;
  s = pc_text_fallback_runs(api, doc, "a\xcc\x81", &runs, &count);
  ASSERT_PC_EXPECTED(PC_ERR_CAPABILITY, s);
  ASSERT_INT_EQ((int)(size_t)runs, 0);

  api->doc_close(doc);
}

// R20.x: null arguments are a reported failure, never a trap.
static void test_arguments(void) {
  const pc_backend_api* api = pc_null_backend_get_api();
  void* doc = nullptr;
  pc_status s = api->doc_open("synthetic.pdf", nullptr, &doc);
  ASSERT_PC_EXPECTED(PC_ERR_NONE, s);

  pc_text_run* runs = nullptr;
  uint32_t count = 0;
  s = pc_text_fallback_runs(nullptr, doc, "x", &runs, &count);
  ASSERT_PC_EXPECTED(PC_ERR_ARGUMENT, s);
  s = pc_text_fallback_runs(api, nullptr, "x", &runs, &count);
  ASSERT_PC_EXPECTED(PC_ERR_ARGUMENT, s);
  s = pc_text_fallback_runs(api, doc, nullptr, &runs, &count);
  ASSERT_PC_EXPECTED(PC_ERR_ARGUMENT, s);
  s = pc_text_fallback_runs(api, doc, "x", nullptr, &count);
  ASSERT_PC_EXPECTED(PC_ERR_ARGUMENT, s);
  s = pc_text_fallback_runs(api, doc, "x", &runs, nullptr);
  ASSERT_PC_EXPECTED(PC_ERR_ARGUMENT, s);

  api->doc_close(doc);
}

int main(void) {
  test_null_capability();
  test_arguments();
#ifdef PC_HAVE_MUPDF
  test_missing_glyph();
  test_invalid_utf8();
  test_mupdf_runs_golden();
#endif

  printf("test_text_fallback: %d passed, %d failed\n", tests_passed, tests_failed);
  return tests_failed ? 1 : 0;
}