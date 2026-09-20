#ifndef PDFCORE_SHA256_H
#define PDFCORE_SHA256_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_sha256 {
  uint32_t state[8];
  uint64_t count;
  uint8_t buffer[64];
} pc_sha256;

void pc_sha256_init(pc_sha256* ctx);
void pc_sha256_update(pc_sha256* ctx, const uint8_t* data, size_t len);
void pc_sha256_final(pc_sha256* ctx, uint8_t digest[32]);

#ifdef __cplusplus
}
#endif

#endif