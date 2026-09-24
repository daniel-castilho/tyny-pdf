// tynypdf viewer entry point - story 5.4: the window ABI gets its first
// caller. Composition only: create, run, destroy; the window module owns
// DPI awareness, the message loop and the timing log (R24.4-R24.6).
// Story 1.5: --bench drives the content viewer loop headless (R31.1) and
// --render-dpi-selftest reports the DPI probe. The non-Windows path stays
// the honest stub until a window backend exists.

#include <cstdio>

#ifdef _WIN32
#include <string>

#include "pdfcore/window.h"

namespace tynypdf {
namespace win32 {
bool dpi_selftest(const char* out_dir);
}  // namespace win32
namespace bench {
int bench_main(const char* pdf_path);
}  // namespace bench
}  // namespace tynypdf
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

  pc_window_params params = {};
  params.width = 1024;
  params.height = 768;
  params.title = L"Tyny PDF";
  params.user_data = nullptr;

  pc_window_callbacks callbacks = {};
  pc_window* window = nullptr;
  pc_status st = pc_window_create(&params, &callbacks, &window);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "tynypdf: window create failed: %u %s\n", st.code, st.detail ? st.detail : "-");
    return 1;
  }

  st = pc_window_run(window);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "tynypdf: message loop failed: %u %s\n", st.code, st.detail ? st.detail : "-");
  }
  pc_window_destroy(window);
  return 0;
#else
  (void)argc;
  (void)argv;
  printf("tynypdf viewer - not yet implemented on this platform\n");
  return 0;
#endif
}
