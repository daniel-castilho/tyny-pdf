#include "caret.h"
#include <vector>
#include <cstdint>
#include <string>
#include <cstring>

size_t caret_left(const char* utf8, size_t len, size_t current_pos) {
  auto breaks = grapheme_breaks(utf8, len);
  for (int i = static_cast<int>(breaks.size()) - 1; i >= 0; --i) {
    if (breaks[i] < current_pos) {
      return breaks[i];
    }
  }
  return 0;
}

size_t caret_right(const char* utf8, size_t len, size_t current_pos) {
  auto breaks = grapheme_breaks(utf8, len);
  for (size_t i = 0; i < breaks.size(); ++i) {
    if (breaks[i] > current_pos) {
      return breaks[i];
    }
  }
  return len;
}