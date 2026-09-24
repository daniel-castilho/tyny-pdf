// D2D blit of one cached tile (story 1.5, R30.1 present path). Windows-only.
// src/app/viewer/d2d_blit.cc
// The render callback hands an opaque ID2D1DeviceContext*; only this TU knows
// it is a Direct2D context, so the platform-neutral loop stays engine- and
// OS-free (ADR-0011 R-M10).
//
// One 256x256 bitmap per grid slot is created once and uploaded only when the
// slot's page generation changes; every later frame is a DrawImage from the
// GPU-resident surface. Re-copying all 192 tiles per frame measured 37.8ms
// p99 (R15.1's budget is 33ms) - the upload-on-change pattern is what a
// 60fps present loop actually needs, so it is the thing measured.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// Windows headers first to get proper type definitions
// clang-format off
#include <d2d1_1.h>
#include <d2d1_3.h>
// clang-format on

#include "d2d_blit.h"

#include <cstdlib>
#include <cstring>

#include "pdfcore/status.h"
#include "viewer.h"

namespace tynypdf {
namespace viewer {

namespace {

constexpr uint32_t kTilePx = 256;

// Fill `bgra` (256*256*4, zero-initialised) from an RGB(A) payload tile.
void fill_bgra(uint8_t* bgra, const tile_payload* payload, const uint8_t* rgb) {
  std::memset(bgra, 0, kTilePx * kTilePx * 4);
  if (payload->width == 0 || payload->height == 0 || payload->stride == 0) {
    return;
  }
  for (uint32_t y = 0; y < payload->height && y < kTilePx; ++y) {
    const uint8_t* src = rgb + (size_t)y * payload->stride;
    uint8_t* dst = bgra + (size_t)y * kTilePx * 4;
    for (uint32_t x = 0; x < payload->width && x < kTilePx; ++x) {
      dst[x * 4 + 0] = src[x * 4 + 2];  // B <- R
      dst[x * 4 + 1] = src[x * 4 + 1];  // G <- G
      dst[x * 4 + 2] = src[x * 4 + 0];  // R <- B
      dst[x * 4 + 3] = 255;             // opaque (engine fills alpha)
    }
  }
}

}  // namespace

pc_status d2d_blit_tile(void* d2d_context, const tile_payload* payload, int32_t col, int32_t row,
                        d2d_scratch_state* scratch) {
  if (!d2d_context || !payload || !scratch) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }
  if (col < 0 || col >= kGridCols || row < 0 || row >= kGridRows) {
    return {sizeof(pc_status), PC_ERR_RANGE, 0, "grid coordinate out of range"};
  }
  ID2D1DeviceContext* ctx = static_cast<ID2D1DeviceContext*>(d2d_context);
  d2d_tile_slot* slot = &scratch->slots[(size_t)row * kGridCols + (size_t)col];

  const uint8_t* rgb = reinterpret_cast<const uint8_t*>(payload + 1);
  const uint32_t bytes = kTilePx * kTilePx * 4;
  if (!scratch->bgra || scratch->bgra_capacity < bytes) {
    uint8_t* fresh = static_cast<uint8_t*>(std::realloc(scratch->bgra, bytes));
    if (!fresh) {
      return {sizeof(pc_status), PC_ERR_MEMORY, 0, "scratch realloc failed"};
    }
    scratch->bgra = fresh;
    scratch->bgra_capacity = bytes;
  }

  ID2D1Bitmap1* bitmap = static_cast<ID2D1Bitmap1*>(slot->bitmap);
  if (!bitmap) {
    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_NONE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96.0f, 96.0f);
    HRESULT hr =
        ctx->CreateBitmap(D2D1::SizeU(kTilePx, kTilePx), nullptr, kTilePx * 4, &props, &bitmap);
    if (FAILED(hr) || !bitmap) {
      return {sizeof(pc_status), PC_ERR_BACKEND, 0, "CreateBitmap failed"};
    }
    slot->bitmap = bitmap;        // slot owns the single ref (d2d_scratch_release)
    slot->last_gen = UINT32_MAX;  // force the first upload below
  }

  if (slot->last_gen != payload->page_gen) {
    fill_bgra(scratch->bgra, payload, rgb);
    HRESULT hr = bitmap->CopyFromMemory(nullptr, scratch->bgra, kTilePx * 4);
    if (FAILED(hr)) {
      return {sizeof(pc_status), PC_ERR_BACKEND, 0, "CopyFromMemory failed"};
    }
    slot->last_gen = payload->page_gen;
  }

  D2D1_POINT_2F at =
      D2D1::Point2F((float)(col * (int32_t)kTilePx), (float)(row * (int32_t)kTilePx));
  ctx->DrawImage(bitmap, at);
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

void d2d_scratch_release(d2d_scratch_state* scratch) {
  if (!scratch) {
    return;
  }
  for (size_t i = 0; i < sizeof(scratch->slots) / sizeof(scratch->slots[0]); ++i) {
    if (scratch->slots[i].bitmap) {
      static_cast<ID2D1Bitmap1*>(scratch->slots[i].bitmap)->Release();
      scratch->slots[i].bitmap = nullptr;
      scratch->slots[i].last_gen = UINT32_MAX;
    }
  }
  if (scratch->bgra) {
    std::free(scratch->bgra);
    scratch->bgra = nullptr;
    scratch->bgra_capacity = 0;
  }
}

}  // namespace viewer
}  // namespace tynypdf