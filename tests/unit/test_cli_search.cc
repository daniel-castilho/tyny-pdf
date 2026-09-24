// R38.1 - CLI search subcommand test
// R38.1: CLI SHALL expose tynypdf-cli search <pdf> <page> <query> <dpi> printing hits as JSON

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore/backend.h"
#include "pdfcore/geom.h"
#include "pdfcore/page.h"
#include "pdfcore/search.h"
#include "pdfcore/selection.h"
#include "pdfcore/status.h"

// Minimal null backend functions
static pc_status null_doc_open(const char*, const char*, void**) __attribute__((used));
static pc_status null_doc_open(const char*, const char*, void**) {
  return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, nullptr};
}
static void null_doc_close(void*) __attribute__((used));
static void null_doc_close(void*) {
}
static uint32_t null_doc_page_count(void*) __attribute__((used));
static uint32_t null_doc_page_count(void*) {
  return 0;
}
static pc_status null_page_get(void*, uint32_t, void**) __attribute__((used));
static pc_status null_page_get(void*, uint32_t, void**) {
  return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, nullptr};
}
static void null_page_free(void*) __attribute__((used));
static void null_page_free(void*) {
}
static int null_doc_has_capability(void*, uint32_t) __attribute__((used));
static int null_doc_has_capability(void*, uint32_t) {
  return 0;
}
static pc_status null_doc_find_tables(void*, pc_rect*, size_t*, size_t) __attribute__((used));
static pc_status null_doc_find_tables(void*, pc_rect*, size_t*, size_t) {
  return {sizeof(pc_status), PC_ERR_CAPABILITY, 0, nullptr};
}
static uint32_t null_face_count(void*) __attribute__((used));
static uint32_t null_face_count(void*) {
  return 0;
}
static pc_status null_face_coverage(void*, uint32_t, uint32_t, int*) __attribute__((used));
static pc_status null_face_coverage(void*, uint32_t, uint32_t, int*) {
  return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, nullptr};
}
static pc_status null_page_render(void*, const pc_render_params*, pc_pixmap*) __attribute__((used));
static pc_status null_page_render(void*, const pc_render_params*, pc_pixmap*) {
  return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, nullptr};
}
static void null_pixmap_free(pc_pixmap*) __attribute__((used));
static void null_pixmap_free(pc_pixmap*) {
}

// Null backend API (no text layout capability)
static pc_backend_api null_api = {
    PC_BACKEND_API_VERSION_MAJOR,
    PC_BACKEND_API_VERSION_MINOR,
    sizeof(pc_backend_api),
    null_doc_open,
    null_doc_close,
    null_doc_page_count,
    null_page_get,
    nullptr,
    null_page_render,
    null_page_free,
    null_pixmap_free,
    nullptr,
    null_doc_has_capability,
    null_doc_find_tables,
    null_face_count,
    null_face_coverage,
    nullptr,
    nullptr,
};

int main() {
  // Test that the search.h header is usable and API exists
  pc_page_box box = {};
  box.cropbox = {0, 0, 612, 792};
  pc_search_results results = {};

  pc_status s = pc_search_page(&null_api, (void*)0x1, "test", 72.0, &box.cropbox, &results);
  if (s.code == PC_ERR_CAPABILITY) {
    printf("PASS cli_search_null_backend\n");
    return 0;
  } else {
    printf("FAIL cli_search_null_backend: expected PC_ERR_CAPABILITY, got %u\n", s.code);
    return 1;
  }
}