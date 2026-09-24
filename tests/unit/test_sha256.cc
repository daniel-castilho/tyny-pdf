// SHA-256 regression test — pc_sha256 must match the FIPS 180-4 vectors
// byte-for-byte. The original implementation passed none of them; it was
// found while pinning the story 1.5 corpus (test_corpus_contract), after the
// stale-report and text-break goldens had already absorbed wrong digests.
// Both goldens were regenerated from the fixed implementation; this test
// keeps the algorithm itself pinned so a golden is never again the only line
// of defence. Digests for the 56- and 129-byte cases were taken from
// sha256sum on this tree (the oracle, not the implementation under test).
// R5.2 (sidecar fingerprint), R30.2 (corpus pinning)
// tests/unit/test_sha256.cc

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pdfcore/sha256.h"

static int failures = 0;

static void to_hex(const uint8_t digest[32], char out[65]) {
  static const char digits[] = "0123456789abcdef";
  for (int i = 0; i < 32; ++i) {
    out[i * 2] = digits[digest[i] >> 4];
    out[i * 2 + 1] = digits[digest[i] & 0xF];
  }
  out[64] = '\0';
}

static void check(const char* what, const uint8_t* data, size_t len, const char* expected) {
  pc_sha256 ctx;
  pc_sha256_init(&ctx);
  if (len) {
    pc_sha256_update(&ctx, data, len);
  }
  uint8_t digest[32];
  pc_sha256_final(&ctx, digest);
  char hex[65];
  to_hex(digest, hex);
  if (strcmp(hex, expected) != 0) {
    fprintf(stderr, "FAIL: %s\n  got  %s\n  want %s\n", what, hex, expected);
    failures++;
  }
}

int main(void) {
  // FIPS 180-4 example vectors.
  check("empty", (const uint8_t*)"", 0,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  check("\"abc\"", (const uint8_t*)"abc", 3,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

  // 56-byte message: 0x80 lands at offset 56, so the length goes to a second
  // block and exercises the buffer_pos > 56 path in final.
  check("56-byte",
        (const uint8_t*)"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno", 56,
        "078c0dfc3278fd7759920f5cca94c6d55db2c694510f6e26a8fe5c5b50a4f417");

  // 129-byte message: two full blocks in update plus a 1-byte tail.
  {
    uint8_t big[129];
    memset(big, 'q', sizeof(big));
    check("129-byte", big, sizeof(big),
          "f0886d9cc70695b46d863ebbe7eb7675ca0c53bc4aec96fadfb85c5318d971b2");
  }

  // One million 'a' — the classic streaming/counter vector.
  {
    pc_sha256 ctx;
    pc_sha256_init(&ctx);
    uint8_t chunk[1000];
    memset(chunk, 'a', sizeof(chunk));
    for (int i = 0; i < 1000; ++i) {
      pc_sha256_update(&ctx, chunk, sizeof(chunk));
    }
    uint8_t digest[32];
    pc_sha256_final(&ctx, digest);
    char hex[65];
    to_hex(digest, hex);
    if (strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") != 0) {
      fprintf(stderr, "FAIL: million 'a'\n  got  %s\n", hex);
      failures++;
    }
  }

  // Incremental feeds across block boundaries must equal the one-shot hash.
  {
    uint8_t big[200];
    for (int i = 0; i < 200; ++i) {
      big[i] = (uint8_t)(i * 7);
    }
    pc_sha256 one, inc;
    pc_sha256_init(&one);
    pc_sha256_update(&one, big, sizeof(big));
    uint8_t d1[32];
    pc_sha256_final(&one, d1);
    pc_sha256_init(&inc);
    pc_sha256_update(&inc, big, 63);
    pc_sha256_update(&inc, big + 63, 100);
    pc_sha256_update(&inc, big + 163, 37);
    uint8_t d2[32];
    pc_sha256_final(&inc, d2);
    if (memcmp(d1, d2, 32) != 0) {
      fprintf(stderr, "FAIL: incremental != one-shot\n");
      failures++;
    }
  }

  if (failures) {
    fprintf(stderr, "sha256: FAIL (%d errors)\n", failures);
    return 1;
  }
  printf("sha256: PASS\n");
  return 0;
}