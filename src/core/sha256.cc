#include "pdfcore/sha256.h"
#include <cstring>

static const uint32_t k[64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHR(x, n) ((x) >> (n))
#define SIG0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define SIG1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define EP0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ SHR(x, 3))
#define EP1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ SHR(x, 10))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

void pc_sha256_init(pc_sha256* ctx) {
  ctx->state[0] = 0x6a09e667;
  ctx->state[1] = 0xbb67ae85;
  ctx->state[2] = 0x3c6ef372;
  ctx->state[3] = 0xa54ff53a;
  ctx->state[4] = 0x510e527f;
  ctx->state[5] = 0x9b05688c;
  ctx->state[6] = 0x1f83d9ab;
  ctx->state[7] = 0x5be0cd19;
  ctx->count = 0;
}

void pc_sha256_update(pc_sha256* ctx, const uint8_t* data, size_t len) {
  size_t i = 0;
  size_t buffer_pos = (size_t)(ctx->count % 64);
  ctx->count += len;

  while (i < len) {
    size_t space = 64 - buffer_pos;
    size_t copy = len - i < space ? len - i : space;
    memcpy(&ctx->buffer[buffer_pos], &data[i], copy);
    i += copy;
    buffer_pos += copy;

    if (buffer_pos == 64) {
      uint32_t w[64];
      for (int j = 0; j < 16; ++j) {
        w[j] = (uint32_t)ctx->buffer[j * 4] << 24 |
               (uint32_t)ctx->buffer[j * 4 + 1] << 16 |
               (uint32_t)ctx->buffer[j * 4 + 2] << 8 |
               (uint32_t)ctx->buffer[j * 4 + 3];
      }
      for (int j = 16; j < 64; ++j) {
        uint32_t s0 = ROTR(w[j - 15], 7) ^ ROTR(w[j - 15], 18) ^ (w[j - 15] >> 3);
        uint32_t s1 = ROTR(w[j - 2], 17) ^ ROTR(w[j - 2], 19) ^ (w[j - 2] >> 10);
        w[j] = w[j - 16] + s0 + w[j - 7] + s1;
      }

      uint32_t a = ctx->state[0];
      uint32_t b = ctx->state[1];
      uint32_t c = ctx->state[2];
      uint32_t d = ctx->state[3];
      uint32_t e = ctx->state[4];
      uint32_t f = ctx->state[5];
      uint32_t g = ctx->state[6];
      uint32_t h = ctx->state[7];

      for (int j = 0; j < 64; ++j) {
        uint32_t t1 = h + EP1(e) + CH(e, f, g) + k[j] + w[j];
        uint32_t t2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
      }

      ctx->state[0] += a;
      ctx->state[1] += b;
      ctx->state[2] += c;
      ctx->state[3] += d;
      ctx->state[4] += e;
      ctx->state[5] += f;
      ctx->state[6] += g;
      ctx->state[7] += h;

      buffer_pos = 0;
    }
  }
}

void pc_sha256_final(pc_sha256* ctx, uint8_t digest[32]) {
  size_t buffer_pos = (size_t)(ctx->count % 64);
  ctx->buffer[buffer_pos++] = 0x80;
  if (buffer_pos > 56) {
    while (buffer_pos < 64) ctx->buffer[buffer_pos++] = 0;
    buffer_pos = 0;
    uint32_t w[64];
    for (int j = 0; j < 16; ++j) {
      w[j] = (uint32_t)ctx->buffer[j * 4] << 24 |
             (uint32_t)ctx->buffer[j * 4 + 1] << 16 |
             (uint32_t)ctx->buffer[j * 4 + 2] << 8 |
             (uint32_t)ctx->buffer[j * 4 + 3];
    }
    for (int j = 16; j < 64; ++j) {
      uint32_t s0 = ROTR(w[j - 15], 7) ^ ROTR(w[j - 15], 18) ^ (w[j - 15] >> 3);
      uint32_t s1 = ROTR(w[j - 2], 17) ^ ROTR(w[j - 2], 19) ^ (w[j - 2] >> 10);
      w[j] = w[j - 16] + s0 + w[j - 7] + s1;
    }
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
    for (int j = 0; j < 64; ++j) {
      uint32_t t1 = h + EP1(e) + CH(e, f, g) + k[j] + w[j];
      uint32_t t2 = EP0(a) + MAJ(a, b, c);
      h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
  }
  while (buffer_pos < 56) ctx->buffer[buffer_pos++] = 0;
  uint64_t bits = ctx->count * 8;
  ctx->buffer[56] = (uint8_t)(bits >> 56);
  ctx->buffer[57] = (uint8_t)(bits >> 48);
  ctx->buffer[58] = (uint8_t)(bits >> 40);
  ctx->buffer[59] = (uint8_t)(bits >> 32);
  ctx->buffer[60] = (uint8_t)(bits >> 24);
  ctx->buffer[61] = (uint8_t)(bits >> 16);
  ctx->buffer[62] = (uint8_t)(bits >> 8);
  ctx->buffer[63] = (uint8_t)bits;

  uint32_t w[64];
  for (int j = 0; j < 16; ++j) {
    w[j] = (uint32_t)ctx->buffer[j * 4] << 24 |
           (uint32_t)ctx->buffer[j * 4 + 1] << 16 |
           (uint32_t)ctx->buffer[j * 4 + 2] << 8 |
           (uint32_t)ctx->buffer[j * 4 + 3];
  }
  for (int j = 16; j < 64; ++j) {
    uint32_t s0 = ROTR(w[j - 15], 7) ^ ROTR(w[j - 15], 18) ^ (w[j - 15] >> 3);
    uint32_t s1 = ROTR(w[j - 2], 17) ^ ROTR(w[j - 2], 19) ^ (w[j - 2] >> 10);
    w[j] = w[j - 16] + s0 + w[j - 7] + s1;
  }

  uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
  uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
  for (int j = 0; j < 64; ++j) {
    uint32_t t1 = h + EP1(e) + CH(e, f, g) + k[j] + w[j];
    uint32_t t2 = EP0(a) + MAJ(a, b, c);
    h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
  }
  ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
  ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;

  for (int i = 0; i < 8; ++i) {
    digest[i * 4] = (uint8_t)(ctx->state[i] >> 24);
    digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
    digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
    digest[i * 4 + 3] = (uint8_t)ctx->state[i];
  }
}