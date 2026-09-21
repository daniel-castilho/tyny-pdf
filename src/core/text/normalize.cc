#include "pdfcore/text.h"
#include "pdfcore/sha256.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

static inline bool is_whitespace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static std::string normalize_whitespace(std::string_view s) {
  std::string result;
  result.reserve(s.size());
  bool in_space = false;
  for (char c : s) {
    if (is_whitespace(c)) {
      if (!in_space) {
        result.push_back(' ');
        in_space = true;
      }
    } else {
      result.push_back(c);
      in_space = false;
    }
  }
  if (!result.empty() && result.back() == ' ') {
    result.pop_back();
  }
  if (!result.empty() && result.front() == ' ') {
    result.erase(0, 1);
  }
  return result;
}

// Simple composition table for common Latin characters with combining marks
static uint32_t compose(uint32_t base, uint32_t combining) {
  // e + acute = e\u0301 (U+00E9)
  if (base == 0x0065 && combining == 0x0301) return 0x00E9;
  if (base == 0x0045 && combining == 0x0301) return 0x00C9;
  // c + cedilla = c\u0327 (U+00E7)
  if (base == 0x0063 && combining == 0x0327) return 0x00E7;
  if (base == 0x0043 && combining == 0x0327) return 0x00C7;
  // a + tilde = a\u0303 (U+00E3)
  if (base == 0x0061 && combining == 0x0303) return 0x00E3;
  if (base == 0x0041 && combining == 0x0303) return 0x00C3;
  // o + acute = o\u0301 (U+00F3)
  if (base == 0x006F && combining == 0x0301) return 0x00F3;
  if (base == 0x004F && combining == 0x0301) return 0x00D3;
  // a + acute = a\u0301 (U+00E1)
  if (base == 0x0061 && combining == 0x0301) return 0x00E1;
  if (base == 0x0041 && combining == 0x0301) return 0x00C1;
  // e + grave = e\u0300 (U+00E8)
  if (base == 0x0065 && combining == 0x0300) return 0x00E8;
  if (base == 0x0045 && combining == 0x0300) return 0x00C8;
  // n + tilde = n\u0303 (U+00F1)
  if (base == 0x006E && combining == 0x0303) return 0x00F1;
  if (base == 0x004E && combining == 0x0303) return 0x00D1;
  return 0;
}

static bool is_combining_mark(uint32_t cp) {
  return (cp >= 0x0300 && cp <= 0x036F) ||
         (cp >= 0x1AB0 && cp <= 0x1AFF) ||
         (cp >= 0x1DC0 && cp <= 0x1DFF) ||
         (cp >= 0x20D0 && cp <= 0x20FF) ||
         (cp >= 0xFE20 && cp <= 0xFE2F);
}

static std::string nfkc_normalize(std::string_view s) {
  // First pass: decode UTF-8 to codepoints
  std::vector<uint32_t> codepoints;
  codepoints.reserve(s.size());
  
  for (size_t i = 0; i < s.size(); ) {
    unsigned char c = s[i];
    if (c < 0x80) {
      codepoints.push_back(c);
      ++i;
    } else if ((c & 0xE0) == 0xC0) {
      if (i + 1 < s.size()) {
        unsigned char c2 = s[i + 1];
        if ((c2 & 0xC0) == 0x80) {
          uint32_t cp = ((c & 0x1F) << 6) | (c2 & 0x3F);
          codepoints.push_back(cp);
          i += 2;
        } else {
          codepoints.push_back(c);
          ++i;
        }
      } else {
        codepoints.push_back(c);
        ++i;
      }
    } else if ((c & 0xF0) == 0xE0) {
      if (i + 2 < s.size()) {
        unsigned char c2 = s[i + 1], c3 = s[i + 2];
        if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
          uint32_t cp = ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
          codepoints.push_back(cp);
          i += 3;
        } else {
          codepoints.push_back(c);
          ++i;
        }
      } else {
        codepoints.push_back(c);
        ++i;
      }
    } else if ((c & 0xF8) == 0xF0) {
      if (i + 3 < s.size()) {
        unsigned char c2 = s[i + 1], c3 = s[i + 2], c4 = s[i + 3];
        if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80 && (c4 & 0xC0) == 0x80) {
          uint32_t cp = ((c & 0x07) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
          codepoints.push_back(cp);
          i += 4;
        } else {
          codepoints.push_back(c);
          ++i;
        }
      } else {
        codepoints.push_back(c);
        ++i;
      }
    } else {
      codepoints.push_back(c);
      ++i;
    }
  }

  // Second pass: compose base + combining marks
  std::vector<uint32_t> composed;
  composed.reserve(codepoints.size());
  
  for (size_t i = 0; i < codepoints.size(); ++i) {
    uint32_t cp = codepoints[i];
    if (!composed.empty() && is_combining_mark(cp)) {
      uint32_t composed_cp = compose(composed.back(), cp);
      if (composed_cp != 0) {
        composed.back() = composed_cp;
        continue;
      }
    }
    composed.push_back(cp);
  }

  // Third pass: casefold (simple ASCII only for now)
  for (uint32_t& cp : composed) {
    if (cp >= 0x41 && cp <= 0x5A) { // A-Z
      cp += 0x20; // to lowercase
    }
  }

  // Encode back to UTF-8
  std::string result;
  result.reserve(composed.size() * 3);
  for (uint32_t cp : composed) {
    if (cp < 0x80) {
      result.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
      result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
      result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      result.push_back(static_cast<char>(0xF0 | (cp >> 18)));
      result.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }

  return result;
}

std::string normalize_for_search(std::string_view s) {
  std::string nfkc = nfkc_normalize(s);
  std::string ws = normalize_whitespace(nfkc);
  return ws;
}

std::string normalize_for_anchor(std::string_view s) {
  return normalize_for_search(s);
}