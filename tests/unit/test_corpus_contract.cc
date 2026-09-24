// Corpus contract test (story 1.5, R30.2) — the 1000-page generated corpus is
// a pinned artefact: the same 40-hex sha and the same page count every time
// it is regenerated, opened here through the mupdf backend exactly as
// tools/bench-measure.sh will open it. A regeneration that shifts either the
// hash or the page count must fail this test before it inflates any
// benchmark claim.
// R30.2
// tests/unit/test_corpus_contract.cc

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include "pdfcore/backend.h"
#include "pdfcore/sha256.h"
#include "pdfcore/status.h"

static const char* kCorpus = TEST_CORPUS_DIR "/corpus-1000p.pdf";
static const char kShaHex[] = "e8da98f3516ccc4cda87ca4d43ec29711fdd11eeb6c5acb8e031dfe48ea2368c";

static void to_hex(const uint8_t digest[32], char out[65]) {
  static const char digits[] = "0123456789abcdef";
  for (int i = 0; i < 32; ++i) {
    out[i * 2] = digits[digest[i] >> 4];
    out[i * 2 + 1] = digits[digest[i] & 0xF];
  }
  out[64] = '\0';
}

int main(void) {
  int errors = 0;

  // Located relative to the workspace; tests run from build/linux-core.
  FILE* f = fopen(kCorpus, "rb");
  if (!f) {
    fprintf(stderr, "FAIL: cannot open %s\n", kCorpus);
    return 1;
  }
  std::vector<uint8_t> bytes;
  uint8_t chunk[8192];
  size_t n;
  while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
    bytes.insert(bytes.end(), chunk, chunk + n);
  }
  fclose(f);
  if (bytes.empty()) {
    fprintf(stderr, "FAIL: empty corpus\n");
    return 1;
  }

  pc_sha256 ctx;
  pc_sha256_init(&ctx);
  pc_sha256_update(&ctx, bytes.data(), bytes.size());
  uint8_t digest[32];
  pc_sha256_final(&ctx, digest);
  char hex[65];
  to_hex(digest, hex);
  if (strcmp(hex, kShaHex) != 0) {
    fprintf(stderr, "FAIL: corpus sha256 differs\n  got  %s\n  want %s\n", hex, kShaHex);
    errors++;
  }

  void* doc = nullptr;
  pc_status st = pc_mupdf_backend_get_api()->doc_open(kCorpus, nullptr, &doc);
  if (st.code != PC_ERR_NONE || !doc) {
    fprintf(stderr, "FAIL: mupdf open (%u %s)\n", st.code, st.detail ? st.detail : "-");
    errors++;
  } else {
    uint32_t pages = pc_mupdf_backend_get_api()->doc_page_count(doc);
    if (pages != 1000) {
      fprintf(stderr, "FAIL: page count %u != 1000\n", pages);
      errors++;
    }
    pc_mupdf_backend_get_api()->doc_close(doc);
  }

  if (errors) {
    fprintf(stderr, "corpus_contract: FAIL (%d errors)\n", errors);
    return 1;
  }
  printf("corpus_contract: PASS\n");
  return 0;
}