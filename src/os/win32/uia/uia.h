// UI Automation provider — internal seam between the window implementation and
// the UIA provider (story 5.5, R24.7). Not part of include/pdfcore: this header
// is Windows-typed (HWND, LPARAM) and only src/os/win32 may include it. The
// public surface the viewer uses is include/pdfcore/uia.h (page/zoom state and
// the announced string); the provider here reads that state and translates it
// into the UIA tree a screen reader walks.
//
// Only src/os/win32/uia/uia.cc implements this symbol.

#pragma once

#include <windows.h>

#include "pdfcore/uia.h"

namespace tynypdf {
namespace win32 {

// Handles WM_GETOBJECT from a UIA client (lParam == UiaRootObjectId) and
// answers with UiaReturnRawElementProvider. Returns true when the message was
// consumed and *lresult holds the reply; false means DefWindowProc should see
// it (non-UIA object requests keep the system proxy path).
bool pc_uia_handle_wm_getobject(HWND hwnd, WPARAM wparam, LPARAM lparam, pc_uia_state* state,
                                LRESULT* lresult);

}  // namespace win32
}  // namespace tynypdf