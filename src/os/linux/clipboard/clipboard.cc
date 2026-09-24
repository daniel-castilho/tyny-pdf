// Clipboard implementation for Linux (stub - not available headless).
// src/os/linux/clipboard/clipboard.cc

#include "pdfcore/clipboard.h"

#include <cstdlib>

#include "pdfcore/status.h"

pc_status pc_clipboard_set_text(const char* utf8) {
  if (!utf8) {
    pc_status s = {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null text"};
    return s;
  }
  // Stub: not available on headless Linux
  pc_status s = {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0,
                 "clipboard not available on this platform"};
  return s;
}

pc_status pc_clipboard_get_text(char** out_utf8) {
  if (!out_utf8) {
    pc_status s = {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null out"};
    return s;
  }
  *out_utf8 = nullptr;
  pc_status s = {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0,
                 "clipboard not available on this platform"};
  return s;
}