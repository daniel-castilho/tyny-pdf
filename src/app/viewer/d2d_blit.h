// D2D blit of one cached tile (story 1.5, R30.1 present path). Windows-only.
// src/app/viewer/d2d_blit.h
// This TU is compiled only where the window ABI exists; the platform-neutral
// loop (viewer.cc) never includes it.

#pragma once

#include <stdint.h>

#include "pdfcore/status.h"

namespace tynypdf {
namespace viewer {

struct tile_payload;  // defined in viewer.h

// The viewer grid is 16x12 tiles (the 4000x3000 R15.1 region at 256px tiles).
constexpr int32_t kGridCols = 16;
constexpr int32_t kGridRows = 12;

// One GPU-resident tile surface. The bitmap is uploaded only when the tile's
// page generation changes (payloads are stamped by viewer_demand); every
// frame after that is a pure DrawImage, which is what R15.1's sustained-blit
// budget measures. Address identity alone cannot key the upload: a page reset
// frees and re-mallocs payloads, and a reused address would otherwise blit
// the previous page's pixels.
struct d2d_tile_slot {
  void* bitmap = nullptr;          // ID2D1Bitmap1*
  uint32_t last_gen = UINT32_MAX;  // payload->page_gen last uploaded
};

// Persistent staging state for the blit (one per viewer, reused every frame).
struct d2d_scratch_state {
  d2d_tile_slot slots[kGridCols * kGridRows];
  uint8_t* bgra = nullptr;  // 256*256*4 staging swap buffer
  uint32_t bgra_capacity = 0;
};

// Convert a tile payload (RGB, opaque alpha) to a BGRA D2D bitmap and draw it
// at (col*256, row*256). PC_ERR_NONE or PC_ERR_MEMORY/PC_ERR_BACKEND, or
// PC_ERR_RANGE for a grid coordinate outside the 16x12 cachemap.
pc_status d2d_blit_tile(void* d2d_context,  // ID2D1DeviceContext*
                        const tile_payload* payload, int32_t col, int32_t row,
                        d2d_scratch_state* scratch);

void d2d_scratch_release(d2d_scratch_state* scratch);

}  // namespace viewer
}  // namespace tynypdf