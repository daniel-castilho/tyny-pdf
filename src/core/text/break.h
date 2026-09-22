#ifndef PDFCORE_TEXT_BREAK_H
#define PDFCORE_TEXT_BREAK_H

#include <stdint.h>

#include "pdfcore/status.h"
#include "pdfcore/text.h"

namespace pdfcore {

// R21.1-R21.3: split a UTF-8 string into UAX #29 extended grapheme clusters
// with the pt-BR exceptions the golden corpus pins. Lives in src/core next to
// fallback.h (R-M8): pure codepoint arithmetic over the byte buffer, no
// engine include. A hostile byte sequence is a reported failure, never a
// half-decoded cluster (ADR-0003).
pc_status break_positions(const char* utf8, uint32_t utf8_len, pc_text_boundary** out_boundaries,
                          uint32_t* out_count);

void boundary_free(pc_text_boundary* boundaries, uint32_t count);

}  // namespace pdfcore

#endif
