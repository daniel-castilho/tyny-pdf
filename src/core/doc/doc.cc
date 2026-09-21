#include "pdfcore/doc.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore/backend.h"
#include "pdfcore/sha256.h"
#include "pdfcore/status.h"

struct pc_doc {
  const pc_backend_api* backend;
  void* backend_doc;
  uint32_t page_count;
  pc_page_box* page_boxes;
  char sha256_hex[65];
};

static void sha256_file(const char* path, char* out_hex) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    out_hex[0] = '\0';
    return;
  }
  pc_sha256 ctx;
  pc_sha256_init(&ctx);
  unsigned char buf[8192];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
    pc_sha256_update(&ctx, buf, n);
  }
  fclose(f);
  unsigned char hash[32];
  pc_sha256_final(&ctx, hash);
  for (int i = 0; i < 32; ++i) {
    sprintf(out_hex + i * 2, "%02x", hash[i]);
  }
  out_hex[64] = '\0';
}

pc_status pc_doc_open(const char* path, const char* password, pc_doc** out_doc) {
  if (!path || !out_doc) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }

  const pc_backend_api* backend = pc_null_backend_get_api();
  void* backend_doc = nullptr;
  pc_status s = backend->doc_open(path, password, &backend_doc);
  if (s.code != PC_ERR_NONE) {
    return s;
  }

  uint32_t count = backend->doc_page_count(backend_doc);
  pc_page_box* boxes = (pc_page_box*)std::calloc(count, sizeof(pc_page_box));
  if (!boxes) {
    backend->doc_close(backend_doc);
    return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM"};
  }

  for (uint32_t i = 0; i < count; ++i) {
    pc_page_box box = {};
    s = backend->page_get_box(backend_doc, i, &box);
    if (s.code != PC_ERR_NONE) {
      std::free(boxes);
      backend->doc_close(backend_doc);
      return s;
    }
    boxes[i] = box;
  }

  backend->doc_close(backend_doc);
  backend_doc = nullptr;

  pc_doc* doc = (pc_doc*)std::malloc(sizeof(pc_doc));
  if (!doc) {
    std::free(boxes);
    return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM"};
  }

  doc->backend = pc_null_backend_get_api();
  doc->backend_doc = nullptr;
  doc->page_count = count;
  doc->page_boxes = boxes;
  sha256_file(path, doc->sha256_hex);

  *out_doc = doc;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

uint32_t pc_doc_page_count(const pc_doc* doc) {
  return doc ? doc->page_count : 0;
}

pc_status pc_doc_page_get_box(const pc_doc* doc, uint32_t page_index, pc_page_box* out) {
  if (!doc || !out || page_index >= doc->page_count) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument or index out of range"};
  }
  *out = doc->page_boxes[page_index];
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

const char* pc_doc_sha256(const pc_doc* doc) {
  return doc ? doc->sha256_hex : nullptr;
}

void pc_doc_close(pc_doc* doc) {
  if (!doc)
    return;
  if (doc->page_boxes) {
    std::free(doc->page_boxes);
  }
  std::free(doc);
}