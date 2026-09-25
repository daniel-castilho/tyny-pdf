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
#include "pdfcore/uia.h"
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

// Story 7.3: the app owns the viewer state and the window handle together so the
// input callbacks can push the forms-focus announcement into the UIA state after
// every key (R51.2/R52.2). The window pointer is set once created.
struct app_state {
  tynypdf::viewer::viewer_state viewer = {};
  pc_window* window = nullptr;
};

// Story 7.3 (R52.2): a focused field switches the announced text to the field's
// announcement; no focus (or a viewer without forms) reverts to "Page N of M,
// zoom Z%" (R24.7). One sync point so every input path ends here.
static void sync_forms_announcement(app_state* app) {
  if (!app || !app->window)
    return;
  pc_uia_state* uia = pc_window_uia_state(app->window);
  if (!uia)
    return;
  wchar_t text[PC_UIA_ANNOUNCE_MAX] = {};
  pc_status s = tynypdf::viewer::viewer_forms_announcement(&app->viewer, text, PC_UIA_ANNOUNCE_MAX);
  pc_uia_state_set_forms_focus(uia, (s.code == PC_ERR_NONE) ? text : nullptr);
}

// Render callback: draws the current viewer page through the cache
static pc_status render_cb(void* /*d2d_context*/, void* user_data) {
  auto* app = static_cast<app_state*>(user_data);
  auto* st = &app->viewer;
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
  auto* app = static_cast<app_state*>(user_data);
  if (!app)
    return;
  // Use active page (0 for now; minimal viewer has no page navigation)
  tynypdf::viewer::viewer_on_click(&app->viewer, app->viewer.active_page, x, y, modifiers);
}

// Story 7.3 (R51.1): key callback forwards to the viewer, then syncs the a11y
// announcement - Tab/Shift+Tab move field focus, Space toggles a checkbox.
static void key_cb(int down, int vk, int modifiers, void* user_data) {
  auto* app = static_cast<app_state*>(user_data);
  if (!app)
    return;
  tynypdf::viewer::viewer_on_key(&app->viewer, app->viewer.active_page, vk, down, modifiers);
  sync_forms_announcement(app);
}

// Story 7.3 (R51.1): WM_CHAR text input into the focused field's buffer.
static void char_cb(uint32_t codepoint, void* user_data) {
  auto* app = static_cast<app_state*>(user_data);
  if (!app)
    return;
  tynypdf::viewer::viewer_on_char(&app->viewer, codepoint);
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
  app_state app = {};
  s = tynypdf::viewer::viewer_open(&app.viewer, api, pdf_path, 72, 256, 0);
  if (s.code != PC_ERR_NONE) {
    fprintf(stderr, "tynypdf: viewer open failed: %u %s\n", s.code, s.detail ? s.detail : "-");
    return 1;
  }

  pc_window_params params = {};
  params.width = 1024;
  params.height = 768;
  params.title = L"Tyny PDF";
  params.user_data = &app;

  pc_window_callbacks callbacks = {};
  callbacks.render = render_cb;
  callbacks.click = click_cb;
  callbacks.key = key_cb;
  callbacks.char_input = char_cb;

  pc_window* window = nullptr;
  s = pc_window_create(&params, &callbacks, &window);
  if (s.code != PC_ERR_NONE) {
    tynypdf::viewer::viewer_close(&app.viewer);
    fprintf(stderr, "tynypdf: window create failed: %u %s\n", s.code, s.detail ? s.detail : "-");
    return 1;
  }
  app.window = window;

  s = pc_window_run(window);
  if (s.code != PC_ERR_NONE) {
    fprintf(stderr, "tynypdf: message loop failed: %u %s\n", s.code, s.detail ? s.detail : "-");
  }
  pc_window_destroy(window);
  tynypdf::viewer::viewer_close(&app.viewer);
  return 0;
#else
  (void)argc;
  (void)argv;
  printf("tynypdf viewer - not yet implemented on this platform\n");
  return 0;
#endif
}
