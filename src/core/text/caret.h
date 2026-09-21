#ifndef PDFCORE_CARET_INTERNAL_H
#define PDFCORE_CARET_INTERNAL_H

#include <stddef.h>
#include <vector>

// Returns grapheme break positions in "adjusted" space where each codepoint counts as 1
std::vector<size_t> grapheme_breaks(const char* utf8, size_t len);

// Caret movement functions - positions are in adjusted space
size_t caret_left(const char* utf8, size_t len, size_t current_pos);
size_t caret_right(const char* utf8, size_t len, size_t current_pos);

#endif