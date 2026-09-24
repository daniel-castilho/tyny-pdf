// SHA-256 (FIPS 180-4) for sidecar staleness and corpus pinning.
// src/core/sha256.cc
// Public-domain compact implementation kept byte-canonical: the digest must
// match sha256sum for any input (tests/unit/test_corpus_contract.cc pins the
// 1000-page corpus; docs/lessons.md records how a wrong digest ate a month).

#include "pdfcore/sha256.h"

#include <cstring>

namespace {

constexpr uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr(uint32_t x, uint32_t n) {
  return (x >> n) | (x << (32 - n));
}

void compress(uint32_t state[8], const uint8_t block[64]) {
  uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
           ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
  }
  for (int i = 16; i < 64; ++i) {
    uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
  uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
  for (int i = 0; i < 64; ++i) {
    uint32_t t1 =
        h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + kK[i] + w[i];
    uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

}  // namespace

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
  std::memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

void pc_sha256_update(pc_sha256* ctx, const uint8_t* data, size_t len) {
  size_t fill = (size_t)(ctx->count % 64);
  ctx->count += len;

  size_t i = 0;
  if (fill && len) {
    size_t n = 64 - fill;
    if (n > len) {
      n = len;
    }
    std::memcpy(&ctx->buffer[fill], data, n);
    fill += n;
    i += n;
    if (fill == 64) {
      compress(ctx->state, ctx->buffer);
      fill = 0;
    }
  }

  while (i + 64 <= len) {
    compress(ctx->state, data + i);
    i += 64;
  }
  if (i < len) {
    std::memcpy(ctx->buffer, data + i, len - i);
  }
}

void pc_sha256_final(pc_sha256* ctx, uint8_t digest[32]) {
  uint64_t bits = ctx->count * 8;
  size_t fill = (size_t)(ctx->count % 64);
  ctx->buffer[fill++] = 0x80;
  if (fill > 56) {
    std::memset(&ctx->buffer[fill], 0, 64 - fill);
    compress(ctx->state, ctx->buffer);
    fill = 0;
  }
  std::memset(&ctx->buffer[fill], 0, 56 - fill);
  ctx->buffer[56] = (uint8_t)(bits >> 56);
  ctx->buffer[57] = (uint8_t)(bits >> 48);
  ctx->buffer[58] = (uint8_t)(bits >> 40);
  ctx->buffer[59] = (uint8_t)(bits >> 32);
  ctx->buffer[60] = (uint8_t)(bits >> 24);
  ctx->buffer[61] = (uint8_t)(bits >> 16);
  ctx->buffer[62] = (uint8_t)(bits >> 8);
  ctx->buffer[63] = (uint8_t)bits;
  compress(ctx->state, ctx->buffer);

  for (int i = 0; i < 8; ++i) {
    digest[i * 4] = (uint8_t)(ctx->state[i] >> 24);
    digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
    digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
    digest[i * 4 + 3] = (uint8_t)ctx->state[i];
  }
}