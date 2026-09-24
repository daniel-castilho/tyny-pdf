// Clipboard module for Tyny PDF — story 6.2 (R35.1).
// Platform-neutral C ABI. Windows implementation in src/os/win32/clipboard/.
// No engine includes (ADR-0011 R-M10).

#pragma once

#include <pdfcore/status.h>

#ifdef __cplusplus
extern "C" {
#endif

// Set the clipboard text content (UTF-8). Returns PC_ERR_NONE on success,
// PC_ERR_ARGUMENT for null text, PC_ERR_UNSUPPORTED if clipboard unavailable.
pc_status pc_clipboard_set_text(const char* utf8);

// Get the clipboard text content as UTF-8. Returns PC_ERR_NONE with *out_utf8
// allocated (caller frees with free()), PC_ERR_ARGUMENT for null out_utf8,
// PC_ERR_UNSUPPORTED if clipboard unavailable or empty. The returned buffer
// is NUL-terminated.
pc_status pc_clipboard_get_text(char** out_utf8);

#ifdef __cplusplus
}
#endif