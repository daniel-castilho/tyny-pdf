// tynypdf viewer entry point - story 5.4: the window ABI gets its first
// caller. Composition only: create, run, destroy; the window module owns
// DPI awareness, the message loop and the timing log (R24.4-R24.6).
// Story 1.5: --bench drives the content viewer loop headless (R31.1) and
// --render-dpi-selftest reports the DPI probe. The non-Windows path stays
// the honest stub until a window backend exists.

#include <cstdio>

#ifdef _WIN32
#include <string>

#include "pdfcore/backend.h"
#include "pdfcore/import.h"
#include "pdfcore/window.h"
#include "viewer/viewer.h"

namespace tynypdf {
namespace win32 {
bool dpi_selftest(const char* out_dir);
}  // namespace win32
namespace bench {
int bench_main(const char* pdf_path);
}  // namespace bench
}  // namespace tynypdf

// Render callback: draws the current viewer page through the cache
static pc_status render_cb(void* /*d2d_context*/, void* user_data) {
  auto* st = static_cast<tynypdf::viewer::viewer_state*>(user_data);
  if (!st || !st->api || !st->doc) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null state"};
  }
  // Just clear for now - the real D2D blit is in d2d_blit.cc
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

// Click callback: forwards to viewer_on_click
static void click_cb(int button, int x, int y, int modifiers, void* user_data) {
  if (button != 1)
    return;  // only left click for selection
  auto* st = static_cast<tynypdf::viewer::viewer_state*>(user_data);
  if (!st)
    return;
  // Use active page (0 for now; minimal viewer has no page navigation)
  tynypdf::viewer::viewer_on_click(st, st->active_page, x, y, modifiers);
}

#endif

int main(int argc, char** argv) {
#ifdef _WIN32
  if (argc > 1 && std::string(argv[1]) == "--bench") {
    return tynypdf::bench::bench_main(argc > 2 ? argv[2] : nullptr);
  }

  if (argc > 1 && std::string(argv[1]) == "--dpi-selftest") {
    const char* out_dir = argc > 2 ? argv[2] : "tests/approvals";
    return tynypdf::win32::dpi_selftest(out_dir) ? 0 : 1;
  }

  // Load backend
  const pc_backend_api* api = nullptr;
  pc_status s =
      pc_backend_get_api(&api, PC_BACKEND_API_VERSION_MAJOR, PC_BACKEND_API_VERSION_MINOR);
  if (s.code != PC_ERR_NONE || !api) {
    fprintf(stderr, "tynypdf: backend load failed: %u %s\n", s.code, s.detail ? s.detail : "-");
    return 1;
  }

  // Open viewer (default to first arg or a test file)
  const char* pdf_path = argc > 1 ? argv[1] : "tests/fixtures/simple.pdf";
  tynypdf::viewer::viewer_state viewer = {};
  s = tynypdf::viewer::viewer_open(&viewer, api, pdf_path, 72, 256, 0);
  if (s.code != PC_ERR_NONE) {
    fprintf(stderr, "tynypdf: viewer open failed: %u %s\n", s.code, s.detail ? s.detail : "-");
    return 1;
  }

  pc_window_params params = {};
  params.width = 1024;
  params.height = 768;
  params.title = L"Tyny PDF";
  params.user_data = &viewer;

  pc_window_callbacks callbacks = {};
  callbacks.render = render_cb;
  callbacks.click = click_cb;

  pc_window* window = nullptr;
  s = pc_window_create(&params, &callbacks, &window);
  if (s.code != PC_ERR_NONE) {
    tynypdf::viewer::viewer_close(&viewer);
    fprintf(stderr, "tynypdf: window create failed: %u %s\n", s.code, s.detail ? s.detail : "-");
    return 1;
  }

  s = pc_window_run(window);
  if (s.code != PC_ERR_NONE) {
    fprintf(stderr, "tynypdf: message loop failed: %u %s\n", s.code, s.detail ? s.detail : "-");
  }
  pc_window_destroy(window);
  tynypdf::viewer::viewer_close(&viewer);
  return 0;
#else
  (void)argc;
  (void)argv;
  printf("tynypdf viewer - not yet implemented on this platform\n");
  return 0;
#endif
}
