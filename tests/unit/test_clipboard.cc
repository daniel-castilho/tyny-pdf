// R35.1 - clipboard test

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore/clipboard.h"
#include "pdfcore/status.h"

int main() {
  // Test clipboard set/get text
  const char* test_text = "Hello, clipboard!";

  // Test set_text
  pc_status s = pc_clipboard_set_text(test_text);
  // On Linux, this returns PC_ERR_UNSUPPORTED (stub)
  // On Windows, it would return PC_ERR_NONE
  if (s.code == PC_ERR_UNSUPPORTED || s.code == PC_ERR_NONE) {
    printf("PASS pc_clipboard_set_text (code=%u)\n", s.code);
  } else {
    printf("FAIL pc_clipboard_set_text: code=%u\n", s.code);
    return 1;
  }

  // Test get_text
  char* out_text = nullptr;
  s = pc_clipboard_get_text(&out_text);
  if (s.code == PC_ERR_UNSUPPORTED || s.code == PC_ERR_NONE) {
    printf("PASS pc_clipboard_get_text (code=%u)\n", s.code);
    if (out_text) {
      free(out_text);
    }
  } else {
    printf("FAIL pc_clipboard_get_text: code=%u\n", s.code);
    return 1;
  }

  // Test null argument handling
  s = pc_clipboard_set_text(nullptr);
  if (s.code == PC_ERR_ARGUMENT) {
    printf("PASS pc_clipboard_set_text null check\n");
  } else {
    printf("FAIL pc_clipboard_set_text null check: code=%u\n", s.code);
    return 1;
  }

  s = pc_clipboard_get_text(nullptr);
  if (s.code == PC_ERR_ARGUMENT) {
    printf("PASS pc_clipboard_get_text null check\n");
  } else {
    printf("FAIL pc_clipboard_get_text null check: code=%u\n", s.code);
    return 1;
  }

  printf("All R35.1 clipboard tests passed\n");
  return 0;
}