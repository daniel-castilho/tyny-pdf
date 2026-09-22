#include "fallback.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore/status.h"
#include "pdfcore/text.h"

namespace pdfcore {

namespace {

struct CodePoint {
  uint32_t value;
  uint32_t byte_offset;
  uint32_t win_face;
};

pc_status status_err(uint32_t code, const char* detail) {
  return {sizeof(pc_status), code, 0, detail};
}

// Decode one UTF-8 codepoint at `s`. Advances *p by the byte length. Returns false on invalid
// UTF-8 without advancing beyond the offending byte. Hostile input must be rejected, never
// mangled into a useful-looking wrong run (ADR-0003).
bool decode_cp(const char* s, size_t len, size_t* p, uint32_t* out_cp) {
  const uint8_t* u = reinterpret_cast<const uint8_t*>(s);
  if (*p >= len)
    return false;
  uint8_t b0 = u[*p];
  if (b0 < 0x80) {
    *out_cp = b0;
    *p += 1;
    return true;
  }
  int extra = 0;
  uint32_t cp = 0;
  if ((b0 & 0xE0) == 0xC0) {
    extra = 1;
    cp = b0 & 0x1F;
  } else if ((b0 & 0xF0) == 0xE0) {
    extra = 2;
    cp = b0 & 0x0F;
  } else if ((b0 & 0xF8) == 0xF0) {
    extra = 3;
    cp = b0 & 0x07;
  } else {
    return false;
  }
  if (*p + extra >= len)
    return false;
  for (int i = 1; i <= extra; ++i) {
    uint8_t b = u[*p + i];
    if ((b & 0xC0) != 0x80)
      return false;
    cp = (cp << 6) | (b & 0x3F);
  }
  // Reject overlong encodings against the assembled value: a shorter-than-needed byte count that
  // still decodes cleanly is exactly what a hostile PDF uses to smuggle a filtered codepoint.
  uint32_t min_cp = extra == 1 ? 0x80U : (extra == 2 ? 0x800U : 0x10000U);
  if (cp < min_cp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
    return false;
  *out_cp = cp;
  *p += extra + 1;
  return true;
}

const char* missing_glyph_detail(uint32_t cp) {
  static char detail[64];
  std::snprintf(detail, sizeof(detail), "missing glyph U+%04X", cp);
  return detail;
}

}  // namespace

pc_status fallback_runs(const pc_backend_api& backend, void* backend_doc, const char* utf8,
                        pc_text_run** out_runs, uint32_t* out_count) {
  if (out_runs)
    *out_runs = nullptr;
  if (out_count)
    *out_count = 0;
  if (!out_runs || !out_count || !utf8 || !backend_doc) {
    return status_err(PC_ERR_ARGUMENT, "null argument");
  }
  if (!backend.doc_has_capability ||
      backend.doc_has_capability(backend_doc, PC_CAP_FACE_COVERAGE) != 1) {
    return status_err(PC_ERR_CAPABILITY, "capability not supported");
  }
  uint32_t face_count = backend.face_count ? backend.face_count(backend_doc) : 0;
  if (face_count == 0) {
    return status_err(PC_ERR_CAPABILITY, "no faces declared");
  }

  // Decode the full string first: invalid UTF-8 is a reported failure before any probe work, and
  // never a partially-shaped run.
  struct CPList {
    struct CodePoint* items;
    uint32_t n;
    uint32_t cap;
  } cps = {nullptr, 0, 0};
  size_t len = std::strlen(utf8);
  size_t p = 0;
  while (p < len) {
    size_t start = p;
    uint32_t cp = 0;
    if (!decode_cp(utf8, len, &p, &cp)) {
      std::free(cps.items);
      return status_err(PC_ERR_ARGUMENT, "invalid UTF-8");
    }
    if (cps.n == cps.cap) {
      uint32_t ncap = cps.cap ? cps.cap * 2 : 16;
      struct CodePoint* grown =
          (struct CodePoint*)std::realloc(cps.items, (size_t)ncap * sizeof(struct CodePoint));
      if (!grown) {
        std::free(cps.items);
        return status_err(PC_ERR_MEMORY, "OOM");
      }
      cps.items = grown;
      cps.cap = ncap;
    }
    cps.items[cps.n].value = cp;
    cps.items[cps.n].byte_offset = (uint32_t)start;
    cps.items[cps.n].win_face = 0;
    cps.n += 1;
  }

  // Pick the first covering face per codepoint. All-or-nothing on the first missing glyph: no
  // partial runs and no tofu rendered downstream (R20.2).
  for (uint32_t i = 0; i < cps.n; ++i) {
    bool covered = false;
    for (uint32_t f = 0; f < face_count; ++f) {
      int has = 0;
      pc_status s = backend.face_coverage(backend_doc, f, cps.items[i].value, &has);
      if (s.code == PC_ERR_CAPABILITY) {
        std::free(cps.items);
        return s;  // negotiated yes, then withdrawn at use - backend defect, report not guess
      }
      if (s.code != PC_ERR_NONE) {
        std::free(cps.items);
        s.detail = s.detail ? s.detail : "face probe failed";
        return s;
      }
      if (has) {
        cps.items[i].win_face = f;
        covered = true;
        break;
      }
    }
    if (!covered) {
      pc_status miss = status_err(PC_ERR_LIMIT, "missing glyph");
      miss.detail = missing_glyph_detail(cps.items[i].value);
      std::free(cps.items);
      return miss;
    }
  }

  // Segment into maximal runs of consecutive codepoints with the same winning face.
  uint32_t run_count = 0;
  if (cps.n > 0) {
    run_count = 1;
    for (uint32_t i = 1; i < cps.n; ++i) {
      if (cps.items[i].win_face != cps.items[i - 1].win_face)
        run_count += 1;
    }
  }
  if (run_count == 0) {
    std::free(cps.items);
    return status_err(PC_ERR_NONE, nullptr);  // empty input: zero runs, nothing allocated
  }

  pc_text_run* runs = (pc_text_run*)std::calloc(run_count, sizeof(pc_text_run));
  if (!runs) {
    std::free(cps.items);
    return status_err(PC_ERR_MEMORY, "OOM");
  }

  uint32_t ri = 0;
  uint32_t run_start = 0;
  for (uint32_t i = 1; i <= cps.n; ++i) {
    bool ends = (i == cps.n) || (cps.items[i].win_face != cps.items[i - 1].win_face);
    if (!ends)
      continue;
    uint32_t run_end = i;  // exclusive over codepoints
    uint32_t byte_begin = cps.items[run_start].byte_offset;
    uint32_t byte_end = run_end < cps.n ? cps.items[run_end].byte_offset : (uint32_t)len;
    uint32_t byte_len = byte_end - byte_begin;
    char* sub = (char*)std::malloc((size_t)byte_len + 1);
    if (!sub) {
      for (uint32_t k = 0; k < ri; ++k) std::free(runs[k].utf8);
      std::free(runs);
      std::free(cps.items);
      return status_err(PC_ERR_MEMORY, "OOM");
    }
    std::memcpy(sub, utf8 + byte_begin, byte_len);
    sub[byte_len] = '\0';
    runs[ri].size = (uint32_t)(run_end - run_start);
    runs[ri].face = cps.items[run_start].win_face;
    runs[ri].utf8 = sub;
    runs[ri].byte_len = byte_len;
    ri += 1;
    run_start = run_end;
  }

  std::free(cps.items);
  *out_runs = runs;
  *out_count = run_count;
  return status_err(PC_ERR_NONE, nullptr);
}

void run_free(pc_text_run* runs, uint32_t count) {
  if (!runs)
    return;
  for (uint32_t i = 0; i < count; ++i) std::free(runs[i].utf8);
  std::free(runs);
}

}  // namespace pdfcore

// Public C ABI (include/pdfcore/text.h). The policy lives in the core; the backend only answers
// face coverage (R-M8).
extern "C" pc_status pc_text_fallback_runs(const pc_backend_api* backend, void* backend_doc,
                                           const char* utf8, pc_text_run** out_runs,
                                           uint32_t* out_count) {
  if (!backend) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }
  return pdfcore::fallback_runs(*backend, backend_doc, utf8, out_runs, out_count);
}

extern "C" void pc_text_run_free(pc_text_run* runs, uint32_t count) {
  pdfcore::run_free(runs, count);
}