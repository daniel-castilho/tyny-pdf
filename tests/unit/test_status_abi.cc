#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/status.h"

// The API freeze (ADR-0003): every pc_error constant matches its golden number in
// tests/golden/status_enum.txt. Golden drift and header drift are both build failures. A
// renumbering like PC_ERR_NONE 0 -> 99 trips these pins (throwaway proof in epic-3-dod §3.5).
#define PIN_STATUS(name, expected)                                                        \
  do {                                                                                    \
    if ((int)(name) != (expected)) {                                                      \
      fprintf(stderr, "Mismatch: " #name " = %d expected %d\n", (int)(name), (expected)); \
      errors++;                                                                           \
    }                                                                                     \
  } while (0)

int main(void) {
  FILE* f = fopen(TEST_GOLDEN_DIR "/status_enum.txt", "r");
  if (!f) {
    fprintf(stderr, "Cannot open golden file\n");
    return 1;
  }

  char line[256];
  int expected = 0;
  int errors = 0;

  while (fgets(line, sizeof(line), f)) {
    char* eq = strchr(line, '=');
    if (!eq)
      continue;
    *eq = '\0';
    int val = atoi(eq + 1);
    if (val != expected) {
      fprintf(stderr, "Mismatch: %s expected %d got %d\n", line, expected, val);
      errors++;
    }
    expected++;
  }
  fclose(f);
  // 0..16 = 17 constants. A golden file that lost or grew a trailing entry changes this count,
  // so the golden cannot silently keep an outdated tail.
  if (expected != 17) {
    fprintf(stderr, "Golden covers %d constants, expected 17\n", expected);
    errors++;
  }

  PIN_STATUS(PC_ERR_NONE, 0);
  PIN_STATUS(PC_ERR_ARGUMENT, 1);
  PIN_STATUS(PC_ERR_UNSUPPORTED, 2);
  PIN_STATUS(PC_ERR_BACKEND, 3);
  PIN_STATUS(PC_ERR_CORRUPT, 4);
  PIN_STATUS(PC_ERR_IO, 5);
  PIN_STATUS(PC_ERR_MEMORY, 6);
  PIN_STATUS(PC_ERR_CRYPTO, 7);
  PIN_STATUS(PC_ERR_PERMISSION, 8);
  PIN_STATUS(PC_ERR_DAMAGED, 9);
  PIN_STATUS(PC_ERR_PASSWORD, 10);
  PIN_STATUS(PC_ERR_RANGE, 11);
  PIN_STATUS(PC_ERR_LIMIT, 12);
  PIN_STATUS(PC_ERR_STATE, 13);
  PIN_STATUS(PC_ERR_VERSION, 14);
  PIN_STATUS(PC_ERR_FEATURE, 15);
  PIN_STATUS(PC_ERR_CAPABILITY, 16);

  if (errors) {
    fprintf(stderr, "FAIL: %d mismatches\n", errors);
    return 1;
  }

  pc_status s = PC_STATUS_INIT;
  if (s.size != sizeof(pc_status)) {
    fprintf(stderr, "size mismatch\n");
    return 1;
  }
  if (!pc_status_is_ok(&s)) {
    fprintf(stderr, "PC_STATUS_INIT not ok\n");
    return 1;
  }

  pc_status_set(&s, PC_ERR_ARGUMENT, 42, "test detail");
  if (s.code != PC_ERR_ARGUMENT || s.detail_id != 42 || strcmp(s.detail, "test detail") != 0) {
    fprintf(stderr, "pc_status_set failed\n");
    return 1;
  }

  printf("status_abi: PASS\n");
  return 0;
}