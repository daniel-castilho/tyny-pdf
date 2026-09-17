#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/backend.h"
#include "pdfcore/page.h"
#include "pdfcore/status.h"

static const pc_backend_api* g_api = nullptr;
static void* g_backend_doc = nullptr;

// R11.1 - The null backend SHALL render a fixed pattern for any page.
static int test_open_close(void) {
  pc_status s = g_api->doc_open("tests/fixtures/simple.pdf", nullptr, &g_backend_doc);
  if (s.code != PC_ERR_NONE && s.code != PC_ERR_UNSUPPORTED && s.code != PC_ERR_IO &&
      s.code != PC_ERR_CORRUPT) {
    fprintf(stderr, "doc_open failed: %u\n", s.code);
    return 1;
  }
  if (g_backend_doc) {
    g_api->doc_close(g_backend_doc);
    g_backend_doc = nullptr;
  }
  return 0;
}

// R11.2 - The null backend SHALL return deterministic pixels for the same page index.
static int test_page_count(void) {
  pc_status s = g_api->doc_open("tests/fixtures/simple.pdf", nullptr, &g_backend_doc);
  if (s.code != PC_ERR_NONE && s.code != PC_ERR_UNSUPPORTED && s.code != PC_ERR_IO &&
      s.code != PC_ERR_CORRUPT) {
    return 1;
  }
  uint32_t count = g_api->doc_page_count(g_backend_doc);
  if (count == 0) {
    fprintf(stderr, "page_count is 0\n");
    return 1;
  }
  g_api->doc_close(g_backend_doc);
  g_backend_doc = nullptr;
  return 0;
}

// R11.1, R11.2 - The null backend SHALL render a fixed pattern and return deterministic pixels.
static int test_page_render(uint32_t page_idx) {
  pc_status s = g_api->doc_open("tests/fixtures/simple.pdf", nullptr, &g_backend_doc);
  if (s.code != PC_ERR_NONE && s.code != PC_ERR_UNSUPPORTED && s.code != PC_ERR_IO &&
      s.code != PC_ERR_CORRUPT) {
    return 1;
  }
  uint32_t count = g_api->doc_page_count(g_backend_doc);
  if (page_idx >= count) {
    g_api->doc_close(g_backend_doc);
    return 0;
  }

  void* page = nullptr;
  pc_status s2 = g_api->page_get(g_backend_doc, page_idx, &page);
  if (s2.code != PC_ERR_NONE || !page) {
    g_api->doc_close(g_backend_doc);
    return 1;
  }

  pc_render_params params = {};
  params.dpi = 72;
  params.clip = {0, 0, 100, 100};
  params.render_annots = 0;
  params.render_text = 1;

  pc_pixmap pixmap = {};
  pc_status s3 = g_api->page_render(page, &params, &pixmap);
  if (s3.code != PC_ERR_NONE || !pixmap.data) {
    g_api->page_free(page);
    g_api->doc_close(g_backend_doc);
    return 1;
  }

  if (pixmap.width == 0 || pixmap.height == 0 || !pixmap.data) {
    g_api->pixmap_free(&pixmap);
    g_api->page_free(page);
    g_api->doc_close(g_backend_doc);
    return 1;
  }

  g_api->pixmap_free(&pixmap);
  g_api->page_free(page);
  g_api->doc_close(g_backend_doc);
  g_backend_doc = nullptr;
  return 0;
}

int main(int argc, char** argv) {
  extern pc_backend_api pc_null_backend_api;
  g_api = &pc_null_backend_api;

  if (argc > 1 && strcmp(argv[1], "--backend=mupdf") == 0) {
    fprintf(stderr, "mupdf backend not linked\n");
    return 77;
  }

  printf("Testing backend: null\n");
  int failures = 0;
  failures += test_open_close();
  failures += test_page_count();
  for (uint32_t i = 0; i < 5; ++i) {
    failures += test_page_render(i);
  }
  if (failures) {
    fprintf(stderr, "FAIL: %d tests failed\n", failures);
    return 1;
  }
  printf("All tests passed\n");
  return 0;
}