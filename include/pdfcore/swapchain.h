// Swapchain module for Tyny PDF — engine-free C ABI (ADR-0011 R-M10).
// src/os/win32/swapchain/ owns the DXGI swapchain + D2D target bitmap.
// No backend/engine includes; the C ABI exposes only void* and pc_status.

#pragma once

#include <pdfcore/status.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque swapchain handle.
typedef struct pc_swapchain pc_swapchain;

// Swapchain creation parameters. All handles are opaque void*.
typedef struct {
  void* hwnd;         // HWND of the target window (Windows)
  int width;          // Initial width in pixels
  int height;         // Initial height in pixels
  float dpi_scale;    // DPI scale factor (1.0 = 96 DPI)
  void* d3d_device;   // Optional ID3D11Device* (created internally if null)
  void* d2d_factory;  // Optional ID2D1Factory3* (created internally if null)
} pc_swapchain_params;

// Create the swapchain for the given window.
pc_status pc_swapchain_create(const pc_swapchain_params* params, pc_swapchain** out_swapchain);

// Resize the swapchain to new dimensions.
pc_status pc_swapchain_resize(pc_swapchain* swapchain, int width, int height, float dpi_scale);

// Begin drawing. out_context receives the D2D device context (void* on
// all platforms; ID2D1DeviceContext* on Windows). Caller draws, then
// calls pc_swapchain_end_draw.
pc_status pc_swapchain_begin_draw(pc_swapchain* swapchain, void** out_context);

// End drawing and present the frame. Returns PC_ERR_NONE on success,
// PC_ERR_UNSUPPORTED on device loss (DXGI_ERROR_DEVICE_REMOVED/_RESET).
pc_status pc_swapchain_end_draw(pc_swapchain* swapchain);

// Get the current swapchain size.
void pc_swapchain_get_size(pc_swapchain* swapchain, int* out_width, int* out_height);

// Get the DPI scale factor.
float pc_swapchain_get_dpi_scale(pc_swapchain* swapchain);

// Destroy the swapchain and all associated resources.
void pc_swapchain_destroy(pc_swapchain* swapchain);

#ifdef __cplusplus
}
#endif
