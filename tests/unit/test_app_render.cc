#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// R10.1 - The viewer SHALL open a PDF document and display page 1.

#include "pdfcore.h"

static const char* get_env_or_die(const char* name) {
  const char* val = std::getenv(name);
  if (!val) {
    fprintf(stderr, "Missing required environment variable: %s\n", name);
    std::exit(1);
  }
  return val;
}

static void cleanup(const char* path) {
  remove(path);
}

int main(void) {
  const char* fixture_dir = get_env_or_die("TEST_FIXTURE_DIR");
  std::string fixture = std::string(fixture_dir) + "/simple.pdf";
  const char* out_path = "/tmp/test_app_render_out.png";
  int rc = 0;

  cleanup(out_path);

  // R10.1: open document and render page 1 via pdfcore (null backend)
  const pc_backend_api* backend_api = nullptr;
  extern pc_backend_api pc_null_backend_api;
  backend_api = &pc_null_backend_api;

  void* backend_doc = nullptr;
  pc_status s = backend_api->doc_open(fixture.c_str(), nullptr, &backend_doc);
  if (s.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: doc_open failed with code %u\n", s.code);
    rc = 1;
  }

  uint32_t count = 0;
  if (rc == 0) {
    count = backend_api->doc_page_count(backend_doc);
    if (count == 0) {
      fprintf(stderr, "FAIL: document has no pages\n");
      rc = 1;
    }
  }

  void* page_ptr = nullptr;
  if (rc == 0) {
    pc_status s2 = backend_api->page_get(backend_doc, 0, &page_ptr);
    if (s2.code != PC_ERR_NONE || !page_ptr) {
      fprintf(stderr, "FAIL: page_get failed\n");
      rc = 1;
    }
  }

  if (rc == 0) {
    pc_render_params params = {};
    params.dpi = 72;
    params.clip = {0, 0, 100, 100};
    params.render_annots = 0;
    params.render_text = 1;

    pc_pixmap pixmap = {};
    pc_status s3 = backend_api->page_render(page_ptr, &params, &pixmap);
    if (s3.code != PC_ERR_NONE || !pixmap.data) {
      fprintf(stderr, "FAIL: page_render failed\n");
      rc = 1;
    }

    if (pixmap.width != 100 || pixmap.height != 100) {
      fprintf(stderr, "FAIL: unexpected pixmap size %ux%u\n", pixmap.width, pixmap.height);
      rc = 1;
    }

    backend_api->pixmap_free(&pixmap);
  }

  if (page_ptr)
    backend_api->page_free(page_ptr);
  if (backend_doc)
    backend_api->doc_close(backend_doc);

  if (rc == 0) {
    printf("PASS: app can open PDF and render page 1\n");
  } else {
    fprintf(stderr, "FAIL: app render test failed\n");
  }
  return rc;
}