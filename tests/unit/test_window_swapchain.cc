#include <stdio.h>

#include <type_traits>

#include "pdfcore/status.h"
#include "pdfcore/swapchain.h"
#include "pdfcore/window.h"

// R25.1, R15.2, R24.2, R24.3: the window and swapchain public ABI is engine-free and
// Windows-free (ADR-0011 R-M10), so this TU builds on Linux with no Windows header
// and no backend linked: an HWND, HRESULT or engine type that leaks into
// include/pdfcore breaks this build. The Windows implementations
// (src/os/win32/window/, src/os/win32/swapchain/) are compiled by the win-cross-x64
// build; this test pins the portable surface both sides must keep. Signatures are
// pinned by address in unevaluated context, so no Windows-only body is linked.

// R15.2: the pc_window entry points keep their exact signatures.
static_assert(
    std::is_same_v<decltype(&pc_window_create),
                   pc_status (*)(const pc_window_params*, const pc_window_callbacks*, pc_window**)>,
    "pc_window_create drifted (R15.2)");
static_assert(std::is_same_v<decltype(&pc_window_run), pc_status (*)(pc_window*)>,
              "pc_window_run drifted (R15.2)");
static_assert(std::is_same_v<decltype(&pc_window_request_redraw), pc_status (*)(pc_window*)>,
              "pc_window_request_redraw drifted (R15.2)");
static_assert(std::is_same_v<decltype(&pc_window_get_dpi_scale), float (*)(pc_window*)>,
              "pc_window_get_dpi_scale drifted (R15.2)");
static_assert(std::is_same_v<decltype(&pc_window_get_size), void (*)(pc_window*, int*, int*)>,
              "pc_window_get_size drifted (R15.2)");
static_assert(std::is_same_v<decltype(&pc_window_destroy), void (*)(pc_window*)>,
              "pc_window_destroy drifted (R15.2)");

// R25.1: the pc_swapchain entry points keep their exact signatures.
static_assert(std::is_same_v<decltype(&pc_swapchain_create),
                             pc_status (*)(const pc_swapchain_params*, pc_swapchain**)>,
              "pc_swapchain_create drifted (R25.1)");
static_assert(
    std::is_same_v<decltype(&pc_swapchain_resize), pc_status (*)(pc_swapchain*, int, int, float)>,
    "pc_swapchain_resize drifted (R25.1)");
static_assert(
    std::is_same_v<decltype(&pc_swapchain_begin_draw), pc_status (*)(pc_swapchain*, void**)>,
    "pc_swapchain_begin_draw drifted (R25.1)");
static_assert(std::is_same_v<decltype(&pc_swapchain_end_draw), pc_status (*)(pc_swapchain*)>,
              "pc_swapchain_end_draw drifted (R25.1)");
static_assert(std::is_same_v<decltype(&pc_swapchain_get_size), void (*)(pc_swapchain*, int*, int*)>,
              "pc_swapchain_get_size drifted (R25.1)");
static_assert(std::is_same_v<decltype(&pc_swapchain_get_dpi_scale), float (*)(pc_swapchain*)>,
              "pc_swapchain_get_dpi_scale drifted (R25.1)");
static_assert(std::is_same_v<decltype(&pc_swapchain_destroy), void (*)(pc_swapchain*)>,
              "pc_swapchain_destroy drifted (R25.1)");

// R-M10's type set for the ABI: handles are void*, the title is wchar_t, the
// context handed to the render callback is void* (ID2D1DeviceContext* on Windows).
static_assert(std::is_same_v<decltype(pc_window_params{}.width), int>);
static_assert(std::is_same_v<decltype(pc_window_params{}.height), int>);
static_assert(std::is_same_v<decltype(pc_window_params{}.title), const wchar_t*>);
static_assert(std::is_same_v<decltype(pc_window_params{}.user_data), void*>);
static_assert(std::is_same_v<decltype(pc_window_callbacks{}.render), pc_window_render_callback>);
static_assert(std::is_same_v<decltype(pc_window_callbacks{}.dpi_changed), pc_window_dpi_callback>);
static_assert(
    std::is_same_v<decltype(pc_window_callbacks{}.size_changed), pc_window_size_callback>);
static_assert(std::is_same_v<pc_window_render_callback, pc_status (*)(void*, void*)>);
static_assert(std::is_same_v<decltype(pc_swapchain_params{}.hwnd), void*>);
static_assert(std::is_same_v<decltype(pc_swapchain_params{}.width), int>);
static_assert(std::is_same_v<decltype(pc_swapchain_params{}.height), int>);
static_assert(std::is_same_v<decltype(pc_swapchain_params{}.dpi_scale), float>);
static_assert(std::is_same_v<decltype(pc_swapchain_params{}.d3d_device), void*>);
static_assert(std::is_same_v<decltype(pc_swapchain_params{}.d2d_factory), void*>);

namespace {

pc_status stub_render_ok(void* d2d_context, void* user_data) {
  (void)d2d_context;
  int* calls = static_cast<int*>(user_data);
  ++*calls;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

pc_status stub_render_fail(void* d2d_context, void* user_data) {
  (void)d2d_context;
  (void)user_data;
  return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "stub failure"};
}

void stub_dpi(float dpi_scale, void* user_data) {
  *static_cast<float*>(user_data) = dpi_scale;
}

void stub_size(int width, int height, void* user_data) {
  int* out = static_cast<int*>(user_data);
  out[0] = width;
  out[1] = height;
}

}  // namespace

int main(void) {
  int errors = 0;

  // Zero-initialized params are valid inputs: every field is a value or a
  // nullable pointer, so {} must compile and be accepted shape-wise.
  pc_window_params wp = {};
  wp.width = 1024;
  wp.height = 768;
  wp.title = nullptr;
  wp.user_data = nullptr;

  pc_swapchain_params sp = {};
  sp.hwnd = nullptr;
  sp.width = 1024;
  sp.height = 768;
  sp.dpi_scale = 1.5f;
  sp.d3d_device = nullptr;
  sp.d2d_factory = nullptr;

  // R15.2: a render callback that reports success carries pc_status with
  // PC_ERR_NONE and the frozen struct size; the message loop proceeds.
  pc_window_callbacks cb = {};
  cb.render = &stub_render_ok;
  cb.dpi_changed = &stub_dpi;
  cb.size_changed = &stub_size;

  int render_calls = 0;
  pc_status ok = cb.render(nullptr, &render_calls);
  if (ok.code != PC_ERR_NONE || ok.size != sizeof(pc_status) || render_calls != 1) {
    fprintf(stderr, "FAIL: success render callback contract\n");
    errors++;
  }

  // A render callback that fails reports its pc_status code; the frame aborts.
  cb.render = &stub_render_fail;
  pc_status bad = cb.render(nullptr, nullptr);
  if (bad.code == PC_ERR_NONE) {
    fprintf(stderr, "FAIL: failure render callback reported success\n");
    errors++;
  }

  // R15.2: the DPI callback receives the scale, the size callback the client size.
  float got_dpi = 0.0f;
  cb.dpi_changed = &stub_dpi;
  cb.dpi_changed(1.25f, &got_dpi);
  if (got_dpi != 1.25f) {
    fprintf(stderr, "FAIL: dpi callback did not receive the scale\n");
    errors++;
  }

  int got_size[2] = {0, 0};
  cb.size_changed = &stub_size;
  cb.size_changed(400, 300, got_size);
  if (got_size[0] != 400 || got_size[1] != 300) {
    fprintf(stderr, "FAIL: size callback did not receive the client size\n");
    errors++;
  }

  // The params a caller hands to create flow to the callbacks unchanged.
  int from_window_params[2] = {0, 0};
  cb.size_changed(wp.width, wp.height, from_window_params);
  if (from_window_params[0] != 1024 || from_window_params[1] != 768) {
    fprintf(stderr, "FAIL: window params did not flow through the size callback\n");
    errors++;
  }

  int from_swapchain_params[2] = {0, 0};
  cb.size_changed(sp.width, sp.height, from_swapchain_params);
  if (from_swapchain_params[0] != 1024 || from_swapchain_params[1] != 768) {
    fprintf(stderr, "FAIL: swapchain params did not flow through the size callback\n");
    errors++;
  }

  float dpi_from_params = 0.0f;
  cb.dpi_changed(sp.dpi_scale, &dpi_from_params);
  if (dpi_from_params != 1.5f) {
    fprintf(stderr, "FAIL: swapchain dpi_scale did not flow through the dpi callback\n");
    errors++;
  }

  if (errors) {
    fprintf(stderr, "window_swapchain: FAIL (%d)\n", errors);
    return 1;
  }
  printf("window_swapchain: PASS\n");
  return 0;
}
