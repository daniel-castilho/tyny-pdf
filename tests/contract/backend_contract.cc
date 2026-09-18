// The contract suite runs the same assertions against every installed backend
// (AGENTS.md, Testing Strategy): an unsupported capability must be reported as unsupported,
// never as empty. The backend is selected by argv[1] and registered once per backend in
// tests/CMakeLists.txt.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/backend.h"
#include "pdfcore/page.h"
#include "pdfcore/status.h"

static const pc_backend_api* g_api = nullptr;

static const char* fixture_path(void) {
#ifdef TEST_FIXTURE_DIR
  static char buf[4096];
  snprintf(buf, sizeof(buf), "%s/simple.pdf", TEST_FIXTURE_DIR);
  return buf;
#else
  return "tests/fixtures/simple.pdf";
#endif
}

// R11.1, R13.1 - a backend SHALL open a document and report its page count.
static int test_open_and_count(void) {
  void* doc = nullptr;
  pc_status s = g_api->doc_open(fixture_path(), nullptr, &doc);
  if (s.code != PC_ERR_NONE || !doc) {
    fprintf(stderr, "doc_open failed: code=%u detail=%s\n", s.code, s.detail ? s.detail : "-");
    return 1;
  }
  uint32_t count = g_api->doc_page_count(doc);
  if (count == 0) {
    fprintf(stderr, "page_count is 0\n");
    g_api->doc_close(doc);
    return 1;
  }
  g_api->doc_close(doc);
  return 0;
}

static int render_page(void* doc, uint32_t index, pc_pixmap* pixmap) {
  void* page = nullptr;
  pc_status s = g_api->page_get(doc, index, &page);
  if (s.code != PC_ERR_NONE || !page) {
    fprintf(stderr, "page_get(%u) failed: code=%u\n", index, s.code);
    return 1;
  }
  pc_render_params params = {};
  params.dpi = 72;
  params.clip = {0, 0, 100, 100};
  params.render_annots = 0;
  params.render_text = 1;
  pc_status r = g_api->page_render(page, &params, pixmap);
  g_api->page_free(page);
  if (r.code != PC_ERR_NONE) {
    fprintf(stderr, "page_render(%u) failed: code=%u detail=%s\n", index, r.code,
            r.detail ? r.detail : "-");
    return 1;
  }
  return 0;
}

// R11.1, R13.1 - a backend SHALL render a page into an owned 32-bit pixmap.
static int test_page_render(uint32_t index) {
  void* doc = nullptr;
  pc_status s = g_api->doc_open(fixture_path(), nullptr, &doc);
  if (s.code != PC_ERR_NONE || !doc) {
    return 1;
  }
  uint32_t count = g_api->doc_page_count(doc);
  if (index >= count) {
    g_api->doc_close(doc);
    return 0;
  }
  pc_pixmap pixmap = {};
  int rc = render_page(doc, index, &pixmap);
  if (rc == 0) {
    if (!pixmap.data || pixmap.width == 0 || pixmap.height == 0 ||
        pixmap.stride < pixmap.width * 4) {
      fprintf(stderr, "page_render(%u) returned an empty pixmap\n", index);
      rc = 1;
    } else {
      size_t size = (size_t)pixmap.height * pixmap.stride;
      int uniform = 1;
      for (size_t i = 1; i < size; ++i) {
        if (pixmap.data[i] != pixmap.data[0]) {
          uniform = 0;
          break;
        }
      }
      if (uniform) {
        fprintf(stderr, "page_render(%u) produced a uniform (blank) pixmap\n", index);
        rc = 1;
      }
    }
    g_api->pixmap_free(&pixmap);
  }
  g_api->doc_close(doc);
  return rc;
}

// R11.2, R13.2 - rendering the same page index twice SHALL yield identical pixels.
static int test_page_render_deterministic(uint32_t index) {
  void* doc = nullptr;
  pc_status s = g_api->doc_open(fixture_path(), nullptr, &doc);
  if (s.code != PC_ERR_NONE || !doc) {
    return 1;
  }
  uint32_t count = g_api->doc_page_count(doc);
  if (index >= count) {
    g_api->doc_close(doc);
    return 0;
  }
  pc_pixmap first = {};
  pc_pixmap second = {};
  int rc = render_page(doc, index, &first);
  if (rc == 0) {
    rc = render_page(doc, index, &second);
  }
  if (rc == 0) {
    if (first.width != second.width || first.height != second.height ||
        first.stride != second.stride || !first.data || !second.data ||
        memcmp(first.data, second.data, (size_t)first.height * first.stride) != 0) {
      fprintf(stderr, "page %u rendered differently twice\n", index);
      rc = 1;
    }
  }
  g_api->pixmap_free(&first);
  g_api->pixmap_free(&second);
  g_api->doc_close(doc);
  return rc;
}

int main(int argc, char** argv) {
  const char* name = argc > 1 ? argv[1] : "null";
  if (strcmp(name, "null") == 0) {
    g_api = &pc_null_backend_api;
#ifdef PC_HAVE_MUPDF
  } else if (strcmp(name, "mupdf") == 0) {
    g_api = pc_mupdf_backend_get_api();
#endif
  } else {
    fprintf(stderr, "unknown backend: %s\n", name);
    return 2;
  }

  printf("Testing backend: %s\n", name);
  int failures = 0;
  failures += test_open_and_count();
  for (uint32_t i = 0; i < 5; ++i) {
    failures += test_page_render(i);
    failures += test_page_render_deterministic(i);
  }
  if (failures) {
    fprintf(stderr, "FAIL: %d test(s) failed\n", failures);
    return 1;
  }
  printf("All tests passed\n");
  return 0;
}
