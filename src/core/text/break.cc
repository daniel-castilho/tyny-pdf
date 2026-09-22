// Story 4.4 (R21.1-R21.3): UAX #29 extended grapheme clusters with the pt-BR
// exceptions the golden corpus pins. Pure codepoint arithmetic - no engine
// include (R-M8). A hostile byte sequence is a reported failure, never a
// half-decoded cluster (ADR-0003).
#include "break.h"

#include <cstdlib>

#include "pdfcore/status.h"
#include "pdfcore/text.h"

namespace pdfcore {

namespace {

struct Cp {
  uint32_t value;
  uint32_t byte_offset;
};

pc_status status_err(uint32_t code, const char* detail) {
  return {sizeof(pc_status), code, 0, detail};
}

// Strict UTF-8 decode: rejects overlong forms, surrogates and anything above
// U+10FFFF at the first offending byte, never advancing past it (ADR-0003:
// hostile input reports, it does not mangle into a plausible wrong cluster).
bool decode_cp(const char* s, uint32_t len, uint32_t* p, uint32_t* out_cp) {
  const uint8_t* u = reinterpret_cast<const uint8_t*>(s);
  if (*p >= len) {
    return false;
  }
  uint32_t b0 = u[*p];
  uint32_t need;
  uint32_t cp;
  if (b0 < 0x80) {
    *out_cp = b0;
    *p += 1;
    return true;
  }
  if ((b0 & 0xE0) == 0xC0) {
    need = 2;
    cp = b0 & 0x1Fu;
  } else if ((b0 & 0xF0) == 0xE0) {
    need = 3;
    cp = b0 & 0x0Fu;
  } else if ((b0 & 0xF8) == 0xF0) {
    need = 4;
    cp = b0 & 0x07u;
  } else {
    return false;
  }
  if (*p + need > len) {
    return false;
  }
  for (uint32_t i = 1; i < need; ++i) {
    if ((u[*p + i] & 0xC0) != 0x80) {
      return false;
    }
    cp = (cp << 6) | (u[*p + i] & 0x3Fu);
  }
  if (need == 2 && cp < 0x80) {
    return false;
  }
  if (need == 3 && cp < 0x800) {
    return false;
  }
  if (need == 4 && cp < 0x10000) {
    return false;
  }
  if (cp >= 0xD800 && cp <= 0xDFFF) {
    return false;
  }
  if (cp > 0x10FFFF) {
    return false;
  }
  *out_cp = cp;
  *p += need;
  return true;
}

bool is_extend(uint32_t cp) {
  return (cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x1AB0 && cp <= 0x1AFF) ||
         (cp >= 0x1DC0 && cp <= 0x1DFF) || (cp >= 0x20D0 && cp <= 0x20FF) ||
         (cp >= 0xFE20 && cp <= 0xFE2F);
}

// pt-BR letters: ASCII plus the precomposed Latin-1 Supplement / Latin
// Extended-A diacritics pt-BR text actually carries (R21.2).
bool is_letter(uint32_t cp) {
  if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z')) {
    return true;
  }
  return cp >= 0x00C0 && cp <= 0x017F && cp != 0x00D7;
}

// ABNT2 tilde dead key: `~ + a/o/n` composes into one accented run (R23.3).
bool is_abnt2_composition(uint32_t prev_base, uint32_t cp) {
  return prev_base == '~' &&
         (cp == 'a' || cp == 'A' || cp == 'o' || cp == 'O' || cp == 'n' || cp == 'N');
}

// No new cluster starts at `cp` when `prev_base` started the cluster to its
// left: UAX #29 GB3 (CR x LF) and GB9 (Extend/ZWJ), plus the two pt-BR rules
// the golden corpus pins - the ABNT2 dead-key composition and the hyphen
// binding to the letter it follows (guarda- never breaks between letter and
// hyphen, the caret steps over the pair, R21.2).
bool no_break_between(uint32_t prev_base, uint32_t cp) {
  if (prev_base == 0x000D && cp == 0x000A) {
    return true;
  }
  if (is_extend(cp) || cp == 0x200D) {
    return true;
  }
  if (is_abnt2_composition(prev_base, cp)) {
    return true;
  }
  if (cp == '-' && is_letter(prev_base)) {
    return true;
  }
  return false;
}

}  // namespace

pc_status break_positions(const char* utf8, uint32_t utf8_len, pc_text_boundary** out_boundaries,
                          uint32_t* out_count) {
  if (utf8 == nullptr || out_boundaries == nullptr || out_count == nullptr) {
    return status_err(PC_ERR_ARGUMENT, "null argument");
  }
  *out_boundaries = nullptr;
  *out_count = 0;
  if (utf8_len == 0) {
    return status_err(PC_ERR_NONE, nullptr);
  }

  Cp* cps = static_cast<Cp*>(std::malloc(utf8_len * sizeof(Cp)));
  if (cps == nullptr) {
    return status_err(PC_ERR_MEMORY, "OOM");
  }
  uint32_t cp_count = 0;
  uint32_t p = 0;
  while (p < utf8_len) {
    uint32_t cp = 0;
    uint32_t at = p;
    if (!decode_cp(utf8, utf8_len, &p, &cp)) {
      std::free(cps);
      return status_err(PC_ERR_ARGUMENT, "invalid UTF-8");
    }
    cps[cp_count].value = cp;
    cps[cp_count].byte_offset = at;
    ++cp_count;
  }

  uint32_t cluster_count = 0;
  uint32_t prev_base = 0;
  for (uint32_t i = 0; i < cp_count; ++i) {
    const Cp& cur = cps[i];
    if (i == 0 || !no_break_between(prev_base, cur.value)) {
      ++cluster_count;
      prev_base = cur.value;
    }
  }

  pc_text_boundary* boundaries =
      static_cast<pc_text_boundary*>(std::malloc(cluster_count * sizeof(pc_text_boundary)));
  if (boundaries == nullptr) {
    std::free(cps);
    return status_err(PC_ERR_MEMORY, "OOM");
  }
  prev_base = 0;
  uint32_t written = 0;
  for (uint32_t i = 0; i < cp_count; ++i) {
    const Cp& cur = cps[i];
    if (i == 0 || !no_break_between(prev_base, cur.value)) {
      boundaries[written].byte_offset = cur.byte_offset;
      boundaries[written].cluster_index = written;
      ++written;
      prev_base = cur.value;
    }
  }
  std::free(cps);
  *out_boundaries = boundaries;
  *out_count = written;
  return status_err(PC_ERR_NONE, nullptr);
}

void boundary_free(pc_text_boundary* boundaries, uint32_t count) {
  (void)count;
  std::free(boundaries);
}

}  // namespace pdfcore

extern "C" pc_status pc_text_break_positions(const char* utf8, uint32_t utf8_len,
                                             pc_text_boundary** out_boundaries,
                                             uint32_t* out_count) {
  return pdfcore::break_positions(utf8, utf8_len, out_boundaries, out_count);
}

extern "C" void pc_text_boundary_free(pc_text_boundary* boundaries, uint32_t count) {
  pdfcore::boundary_free(boundaries, count);
}
