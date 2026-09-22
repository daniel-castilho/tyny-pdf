// Story 4.4 (R23.1-R23.3): caret moves one grapheme cluster per step. The
// stop set is the cluster starts from break.h plus the end of the buffer, so
// the caret never lands inside a combining sequence, inside an ABNT2
// composition, or between a letter and its hyphen (R21.2).
#include "caret.h"

#include <cstdlib>

#include "break.h"
#include "pdfcore/status.h"
#include "pdfcore/text.h"

namespace pdfcore {

namespace {

pc_status status_err(uint32_t code, const char* detail) {
  return {sizeof(pc_status), code, 0, detail};
}

// Walk the cluster starts looking for the stop adjacent to `byte_pos` in the
// requested direction. At the edges the caret stays put (0 left of 0, len
// right of len) so a caller can hold the key without an overflow.
enum class Direction { kLeft, kRight };

pc_status caret_step(Direction dir, const char* utf8, uint32_t utf8_len, uint32_t byte_pos,
                     uint32_t* out_pos) {
  if (utf8 == nullptr || out_pos == nullptr) {
    return status_err(PC_ERR_ARGUMENT, "null argument");
  }
  if (byte_pos > utf8_len) {
    return status_err(PC_ERR_ARGUMENT, "position beyond buffer");
  }
  pc_text_boundary* boundaries = nullptr;
  uint32_t count = 0;
  pc_status s = break_positions(utf8, utf8_len, &boundaries, &count);
  if (s.code != PC_ERR_NONE) {
    return s;
  }
  uint32_t result = byte_pos;
  if (dir == Direction::kLeft) {
    result = 0;
    for (uint32_t i = 0; i < count; ++i) {
      if (boundaries[i].byte_offset < byte_pos) {
        result = boundaries[i].byte_offset;
      }
    }
  } else {
    result = utf8_len;
    for (uint32_t i = 0; i < count; ++i) {
      if (boundaries[i].byte_offset > byte_pos) {
        result = boundaries[i].byte_offset;
        break;
      }
    }
  }
  std::free(boundaries);
  *out_pos = result;
  return status_err(PC_ERR_NONE, nullptr);
}

}  // namespace

pc_status caret_left(const char* utf8, uint32_t utf8_len, uint32_t byte_pos, uint32_t* out_left) {
  return caret_step(Direction::kLeft, utf8, utf8_len, byte_pos, out_left);
}

pc_status caret_right(const char* utf8, uint32_t utf8_len, uint32_t byte_pos, uint32_t* out_right) {
  return caret_step(Direction::kRight, utf8, utf8_len, byte_pos, out_right);
}

}  // namespace pdfcore

extern "C" pc_status pc_caret_left(const char* utf8, uint32_t utf8_len, uint32_t byte_pos,
                                   uint32_t* out_left) {
  return pdfcore::caret_left(utf8, utf8_len, byte_pos, out_left);
}

extern "C" pc_status pc_caret_right(const char* utf8, uint32_t utf8_len, uint32_t byte_pos,
                                    uint32_t* out_right) {
  return pdfcore::caret_right(utf8, utf8_len, byte_pos, out_right);
}
