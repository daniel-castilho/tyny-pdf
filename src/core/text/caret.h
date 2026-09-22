#ifndef PDFCORE_TEXT_CARET_H
#define PDFCORE_TEXT_CARET_H

#include <stdint.h>

#include "pdfcore/status.h"

namespace pdfcore {

// R23.1-R23.3: caret movement over one grapheme cluster. One step crosses a
// combining sequence (`e + U+0301` moves from 0 to the next cluster, never a
// stop on the combining mark), never stops inside an ABNT2 `~ + a`
// composition, and never stops between a letter and the hyphen it carries.
// Stop set: every cluster start plus the end of the buffer.
pc_status caret_left(const char* utf8, uint32_t utf8_len, uint32_t byte_pos, uint32_t* out_left);
pc_status caret_right(const char* utf8, uint32_t utf8_len, uint32_t byte_pos, uint32_t* out_right);

}  // namespace pdfcore

#endif
