// Clipboard implementation for Windows.
// src/os/win32/clipboard/clipboard.cc

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "pdfcore/clipboard.h"

#include <windows.h>

#include <cstdlib>
#include <string>

#include "pdfcore/status.h"

pc_status pc_clipboard_set_text(const char* utf8) {
  if (!utf8) {
    return (pc_status){sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null text"};
  }

  if (!OpenClipboard(nullptr)) {
    return (pc_status){sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "OpenClipboard failed"};
  }

  EmptyClipboard();

  // Convert UTF-8 to UTF-16
  int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
  if (wlen == 0) {
    CloseClipboard();
    return (pc_status){sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "MultiByteToWideChar failed"};
  }

  HGLOBAL hmem = GlobalAlloc(GMEM_MOVEABLE, wlen * sizeof(wchar_t));
  if (!hmem) {
    CloseClipboard();
    return (pc_status){sizeof(pc_status), PC_ERR_MEMORY, 0, "GlobalAlloc failed"};
  }

  wchar_t* wstr = static_cast<wchar_t*>(GlobalLock(hmem));
  if (!wstr) {
    GlobalFree(hmem);
    CloseClipboard();
    return (pc_status){sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "GlobalLock failed"};
  }

  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, wlen);
  GlobalUnlock(hmem);

  if (!SetClipboardData(CF_UNICODETEXT, hmem)) {
    GlobalFree(hmem);
    CloseClipboard();
    return (pc_status){sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "SetClipboardData failed"};
  }

  CloseClipboard();
  return (pc_status){sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

pc_status pc_clipboard_get_text(char** out_utf8) {
  if (!out_utf8) {
    return (pc_status){sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null out"};
  }
  *out_utf8 = nullptr;

  if (!OpenClipboard(nullptr)) {
    return (pc_status){sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "OpenClipboard failed"};
  }

  HGLOBAL hmem = GetClipboardData(CF_UNICODETEXT);
  if (!hmem) {
    CloseClipboard();
    return (pc_status){sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "no Unicode text in clipboard"};
  }

  wchar_t* wstr = static_cast<wchar_t*>(GlobalLock(hmem));
  if (!wstr) {
    CloseClipboard();
    return (pc_status){sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "GlobalLock failed"};
  }

  // Convert UTF-16 to UTF-8
  int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
  if (len == 0) {
    GlobalUnlock(hmem);
    CloseClipboard();
    return (pc_status){sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "WideCharToMultiByte failed"};
  }

  char* utf8 = static_cast<char*>(std::malloc(len));
  if (!utf8) {
    GlobalUnlock(hmem);
    CloseClipboard();
    return (pc_status){sizeof(pc_status), PC_ERR_MEMORY, 0, "malloc failed"};
  }

  WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8, len, nullptr, nullptr);
  GlobalUnlock(hmem);
  CloseClipboard();

  *out_utf8 = utf8;
  return (pc_status){sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}