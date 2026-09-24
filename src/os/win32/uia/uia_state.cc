// UI Automation document-state module for Tyny PDF — story 5.5 (R24.7).
// src/os/win32/uia/uia_state.cc
//
// Portable half of the a11y claim: the document state (page index, page count,
// zoom) and the announced string built from it. No Windows header and no engine
// handle live here, so the same file compiles into tynypdf-os-win32 on Windows
// and tynypdf-os-linux on Linux; the COM provider (uia.cc) and the UIA
// machinery (uiautomationcore) are win-only. This is why test_uia can run on
// the linux-core preset at all (ADR-0011 R-M10).

#include <wchar.h>

#include "pdfcore/status.h"
#include "pdfcore/uia.h"

struct pc_uia_state {
  int page_index;
  int page_count;
  int zoom_percent;
};

static void clamp_state(pc_uia_state* state) {
  if (state->page_count < 1)
    state->page_count = 1;
  if (state->page_index < 1)
    state->page_index = 1;
  if (state->page_index > state->page_count)
    state->page_index = state->page_count;
  if (state->zoom_percent < 1)
    state->zoom_percent = 100;
}

pc_status pc_uia_format_announcement(int page_index, int page_count, int zoom_percent, wchar_t* out,
                                     size_t out_cap) {
  if (!out && out_cap)
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null output buffer"};

  pc_uia_state tmp = {page_index, page_count, zoom_percent};
  clamp_state(&tmp);

  // The exact announced shape the docs/a11y scripts expect: "Page 1 of 5, zoom
  // 150%". Diffable byte-for-byte; the scripts quote this string.
  int written = swprintf(out, out_cap, L"Page %d of %d, zoom %d%%", tmp.page_index, tmp.page_count,
                         tmp.zoom_percent);
  if (written < 0 || (size_t)written >= out_cap)
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "announcement would overflow"};
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

pc_status pc_uia_state_create(pc_uia_state** out_state) {
  if (!out_state)
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null out_state"};
  *out_state = new pc_uia_state{1, 1, 100};
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

void pc_uia_state_update(pc_uia_state* state, int page_index, int page_count, int zoom_percent) {
  if (!state)
    return;
  state->page_index = page_index;
  state->page_count = page_count;
  state->zoom_percent = zoom_percent;
  clamp_state(state);
}

pc_status pc_uia_state_announcement(const pc_uia_state* state, wchar_t* out, size_t out_cap) {
  if (!state)
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null state"};
  return pc_uia_format_announcement(state->page_index, state->page_count, state->zoom_percent, out,
                                    out_cap);
}

void pc_uia_state_destroy(pc_uia_state* state) {
  delete state;
}