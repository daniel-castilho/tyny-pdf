#ifndef PDFCORE_TEXT_INTERNAL_H
#define PDFCORE_TEXT_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>

// Internal C++ functions for text normalization
std::string normalize_for_search(std::string_view);
std::string normalize_for_anchor(std::string_view);

#endif