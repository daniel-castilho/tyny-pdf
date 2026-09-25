// Window module for Tyny PDF — Per-Monitor V2 DPI, DComp visual, message loop.
// No backend/engine includes (ADR-0011 R-M10). Engine-free C ABI:
// the public surface is only void*, pc_status, wchar_t, int and float.

#pragma once

#include <pdfcore/status.h>
#include <pdfcore/uia.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque window handle.
typedef struct pc_window pc_window;

// Render callback: draw a frame using the provided D2D device context
// (opaque void*; ID2D1DeviceContext* on Windows). Return PC_ERR_NONE on
// success; any other code aborts the frame. Called from the message loop
// between BeginDraw() and EndDraw().
typedef pc_status (*pc_window_render_callback)(void* d2d_context, void* user_data);

// Callback for DPI change.
typedef void (*pc_window_dpi_callback)(float dpi_scale, void* user_data);

// Callback for window size change.
typedef void (*pc_window_size_callback)(int width, int height, void* user_data);

// Window event callbacks.
typedef struct pc_window_callbacks {
  pc_window_render_callback render;
  pc_window_dpi_callback dpi_changed;
  pc_window_size_callback size_changed;
  // ABI 1.2 (story 6.1): mouse click callback. button: 1=left, 2=right, 3=middle.
  // x, y are client coordinates in device pixels. modifiers: bit 0=Shift, 1=Ctrl, 2=Alt.
  typedef void (*pc_window_click_callback)(int button, int x, int y, int modifiers,
                                           void* user_data);
  // Keyboard callback. down: 1=press, 0=release. vk is Windows virtual-key code.
  // modifiers: bit 0=Shift, 1=Ctrl, 2=Alt.
  typedef void (*pc_window_key_callback)(int down, int vk, int modifiers, void* user_data);
  pc_window_click_callback click;
  pc_window_key_callback key;
  // ABI 1.4 (story 7.3): text-input callback from WM_CHAR. codepoint is one Unicode
  // scalar value (a WM_CHAR UTF-16 code unit, or the combined surrogate pair when the
  // window procedure assembles one). Used for form text entry (R51.1).
  typedef void (*pc_window_char_callback)(uint32_t codepoint, void* user_data);
  pc_window_char_callback char_input;
} pc_window_callbacks;

// Window creation parameters.
typedef struct {
  int width;             // Initial client width in pixels
  int height;            // Initial client height in pixels
  const wchar_t* title;  // Window title (UTF-16)
  void* user_data;       // Optional user data passed to callbacks
} pc_window_params;

// Create a Per-Monitor V2 DPI-aware window with DComp visual and DXGI
// swapchain. The window owns the D3D11 device, DComp device, and the
// D2D factory/device/context plus the DXGI swapchain. The caller
// provides a render callback that draws using the D2D device context.
pc_status pc_window_create(const pc_window_params* params, const pc_window_callbacks* callbacks,
                           pc_window** out_window);

// Run the window message loop. Blocks until WM_QUIT.
pc_status pc_window_run(pc_window* window);

// Request a redraw (posts WM_PAINT). Thread-safe.
pc_status pc_window_request_redraw(pc_window* window);

// Get the current DPI scale factor (1.0 = 96 DPI).
float pc_window_get_dpi_scale(pc_window* window);

// Get current client size in pixels.
void pc_window_get_size(pc_window* window, int* out_width, int* out_height);

// ABI 1.4 (story 7.3): the a11y document state the UIA provider announces (R24.7).
// The composition root pushes forms-focus announcements into it after each key
// (R51.2/R52.2). Read-only borrow: the window retains ownership.
pc_uia_state* pc_window_uia_state(pc_window* window);

// Destroy the window and all associated resources.
void pc_window_destroy(pc_window* window);

#ifdef __cplusplus
}
#endif
