#include <vector>
#include <cstdint>
#include <string>
#include <cstring>

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

std::vector<size_t> grapheme_breaks(const char* utf8, size_t len) {
  std::vector<size_t> breaks;
  breaks.push_back(0);
  size_t pos = 0;
  size_t adjusted_pos = 0;
  while (pos < len) {
    (void)next_codepoint(utf8, len, &pos);
    adjusted_pos += 1; // each base character counts as 1 in adjusted space
    while (pos < len) {
      size_t saved = pos;
      uint32_t cp = next_codepoint(utf8, len, &pos);
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

std::vector<size_t> caret_positions(const char* utf8, size_t len) {
  return grapheme_breaks(utf8, len);
}