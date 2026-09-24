// UI Automation module for Tyny PDF — story 5.5 (M1 a11y criterion).
// The Windows UIA provider lives in src/os/win32/uia/uia.cc and implements
// IRawElementProviderFragment; this public surface is what the viewer and the
// headless test may call, so no IRawElementProvider* type leaks here (it is a
// COM type, ADR-0011 R-M10). Everything a screen reader announces crosses this
// header as a primitive (int, wchar_t) or an opaque handle.

#pragma once

#include <pdfcore/status.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque document state that drives the announcements. The viewer owns the
// object; the provider (src/os/win32/uia/uia.cc) reads it during WM_GETOBJECT.
typedef struct pc_uia_state pc_uia_state;

// Value of get_Value() on the window's UIA ValuePattern: the announced text,
// diffable against the docs/a11y scripts. Written as UTF-16; the buffer is
// sized for the longest plausible announcement ("Page 9999 of 9999, zoom
// 999%").
#define PC_UIA_ANNOUNCE_MAX 128

// The announcement shape is the exact string Narrator and NVDA are scripted to
// produce (docs/a11y/keyboard.md, narrator-nvda.md): "Page N of M, zoom Z%".
// It is built by pc_uia_format_announcement below, whose output is the
// diffable artefact the scripts verify.
pc_status pc_uia_format_announcement(int page_index, int page_count, int zoom_percent, wchar_t* out,
                                     size_t out_cap);

// Create the document state with page/zoom defaults. Returns PC_ERR_ARGUMENT
// for a null out parameter. The state starts at page 1 of 1, zoom 100%.
pc_status pc_uia_state_create(pc_uia_state** out_state);

// Update the state. page_index is 1-based; values outside 1..page_count are
// clamped to the nearest legal value rather than rejected, so a document that
// briefly reports an empty page range still announces "Page 1 of N".
void pc_uia_state_update(pc_uia_state* state, int page_index, int page_count, int zoom_percent);

// Current announced value of the state, formatted with
// pc_uia_format_announcement. Returns PC_ERR_ARGUMENT if the buffer is too
// small (cannot happen with PC_UIA_ANNOUNCE_MAX on valid input).
pc_status pc_uia_state_announcement(const pc_uia_state* state, wchar_t* out, size_t out_cap);

// Release the state created with pc_uia_state_create. Invalidates the pointer.
void pc_uia_state_destroy(pc_uia_state* state);

#ifdef __cplusplus
}
#endif