#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

#include <type_traits>

#include "pdfcore/status.h"
#include "pdfcore/uia.h"

// R24.7 (story 5.5): the a11y document state (page, zoom) and the announced
// string it formats are engine-free and Windows-free, so the whole claim is
// tested on Linux with no Windows header and no backend linked: the header must
// leak neither (ADR-0011 R-M10), and the pure state module
// (src/os/win32/uia/uia_state.cc) is what this TU links.
//
// This TU deliberately does not include uiautomationcore.h or the COM provider
// (uia.cc); it pins the portable contract the provider builds on, mirroring
// how test_window_swapchain pins the window ABI.

// R24.7: the announced value is a UTF-16 buffer whose size leaves room for the
// longest plausible announcement. The constant lives in the public header, so a
// shrink that would truncate "Page 9999 of 9999, zoom 999%" fails this TU.
static_assert(PC_UIA_ANNOUNCE_MAX >= 128, "announcement buffer shrank (R24.7)");

// R24.7: the state entry points keep their exact portable signatures; a Windows
// type (HWND, IRawElementProvider*, BSTR) leaking here breaks the build.
static_assert(std::is_same_v<decltype(&pc_uia_state_create), pc_status (*)(pc_uia_state**)>,
              "pc_uia_state_create drifted (R24.7)");
static_assert(
    std::is_same_v<decltype(&pc_uia_state_update), void (*)(pc_uia_state*, int, int, int)>,
    "pc_uia_state_update drifted (R24.7)");
static_assert(std::is_same_v<decltype(&pc_uia_state_announcement),
                             pc_status (*)(const pc_uia_state*, wchar_t*, size_t)>,
              "pc_uia_state_announcement drifted (R24.7)");
static_assert(std::is_same_v<decltype(&pc_uia_state_destroy), void (*)(pc_uia_state*)>,
              "pc_uia_state_destroy drifted (R24.7)");
static_assert(std::is_same_v<decltype(&pc_uia_format_announcement),
                             pc_status (*)(int, int, int, wchar_t*, size_t)>,
              "pc_uia_format_announcement drifted (R24.7)");

// R24.7: the exact announced string the docs/a11y scripts diff against.
// "Page 1 of 5, zoom 150%" is quoted in docs/a11y/keyboard.md and
// docs/a11y/narrator-nvda.md; if this string changes the scripts change too,
// so the assertion is here to keep the two in lock step.
static int check_announcement(int page, int count, int zoom, const wchar_t* expected) {
  wchar_t buf[PC_UIA_ANNOUNCE_MAX] = {};
  pc_status st = pc_uia_format_announcement(page, count, zoom, buf, PC_UIA_ANNOUNCE_MAX);
  if (st.code != PC_ERR_NONE || wcscmp(buf, expected) != 0) {
    fprintf(stderr, "FAIL: announcement(%d,%d,%d) != [%ls]\n", page, count, zoom, expected);
    return 1;
  }
  return 0;
}

static int run() {
  int errors = 0;

  // The headless scripts' scenario: five pages, second page, 150% zoom.
  errors += check_announcement(2, 5, 150, L"Page 2 of 5, zoom 150%");

  // The kickoff M5 example is the first page of a five-page document.
  errors += check_announcement(1, 5, 150, L"Page 1 of 5, zoom 150%");

  // 100% zoom omits a decimal: "zoom 100%" not "zoom 100.0%".
  errors += check_announcement(1, 1, 100, L"Page 1 of 1, zoom 100%");

  // Out-of-range pages clamp to the nearest legal value, never announce 0 or a
  // page past the end (a transient empty document still says "Page 1 of 1").
  errors += check_announcement(0, 5, 150, L"Page 1 of 5, zoom 150%");
  errors += check_announcement(9, 5, 150, L"Page 5 of 5, zoom 150%");
  errors += check_announcement(1, 0, 150, L"Page 1 of 1, zoom 150%");

  // State object: create, update, announce; update clamps like the formatter.
  pc_uia_state* state = nullptr;
  pc_status st = pc_uia_state_create(&state);
  if (st.code != PC_ERR_NONE || !state) {
    fprintf(stderr, "FAIL: pc_uia_state_create\n");
    return 1;
  }
  wchar_t buf[PC_UIA_ANNOUNCE_MAX] = {};
  st = pc_uia_state_announcement(state, buf, PC_UIA_ANNOUNCE_MAX);
  if (st.code != PC_ERR_NONE || wcscmp(buf, L"Page 1 of 1, zoom 100%") != 0) {
    fprintf(stderr, "FAIL: default state announcement\n");
    errors++;
  }
  pc_uia_state_update(state, 2, 5, 150);
  st = pc_uia_state_announcement(state, buf, PC_UIA_ANNOUNCE_MAX);
  if (st.code != PC_ERR_NONE || wcscmp(buf, L"Page 2 of 5, zoom 150%") != 0) {
    fprintf(stderr, "FAIL: updated state announcement\n");
    errors++;
  }
  // Update clamps the same way the free function does.
  pc_uia_state_update(state, -1, 0, 0);
  st = pc_uia_state_announcement(state, buf, PC_UIA_ANNOUNCE_MAX);
  if (st.code != PC_ERR_NONE || wcscmp(buf, L"Page 1 of 1, zoom 100%") != 0) {
    fprintf(stderr, "FAIL: clamped state announcement\n");
    errors++;
  }
  pc_uia_state_destroy(state);

  // A too-small buffer reports an error, never a truncation that the scripts
  // would hear as the wrong sentence.
  wchar_t small[4] = {};
  st = pc_uia_format_announcement(1, 1, 100, small, 4);
  if (st.code == PC_ERR_NONE) {
    fprintf(stderr, "FAIL: truncated announcement reported success\n");
    errors++;
  }

  if (errors) {
    fprintf(stderr, "uia_state: FAIL (%d)\n", errors);
    return 1;
  }
  printf("uia_state: PASS\n");
  return 0;
}

int main() {
  return run();
}