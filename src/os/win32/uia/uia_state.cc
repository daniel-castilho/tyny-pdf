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
  // Story 7.3 (R52.2): the forms-focus announcement replaces the page announcement
  // while non-empty; empty reverts. wchar_t so the provider copies it straight out.
  wchar_t focus_text[PC_UIA_ANNOUNCE_MAX];
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

  // focus_text is irrelevant to the page/zoom path; {} satisfies the strict
  // initializer gate the tree builds with.
  pc_uia_state tmp = {page_index, page_count, zoom_percent, {}};
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
  *out_state = new pc_uia_state{1, 1, 100, {}};
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

void pc_uia_state_set_forms_focus(pc_uia_state* state, const wchar_t* announcement) {
  if (!state)
    return;
  if (!announcement || !announcement[0]) {
    state->focus_text[0] = L'\0';
    return;
  }
  int i = 0;
  for (; i < PC_UIA_ANNOUNCE_MAX - 1 && announcement[i]; ++i) {
    state->focus_text[i] = announcement[i];
  }
  state->focus_text[i] = L'\0';
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
  // R52.2: the forms-focus announcement wins while set; the provider reads this one
  // function, so get_Value switches shapes without any COM change.
  if (state->focus_text[0]) {
    if (!out && out_cap)
      return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null output buffer"};
    int written = swprintf(out, out_cap, L"%ls", state->focus_text);
    if (written < 0 || (size_t)written >= out_cap)
      return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "announcement would overflow"};
    return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
  }
  return pc_uia_format_announcement(state->page_index, state->page_count, state->zoom_percent, out,
                                    out_cap);
}

void pc_uia_state_destroy(pc_uia_state* state) {
  delete state;
}