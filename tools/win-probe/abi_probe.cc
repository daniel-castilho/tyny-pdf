// abi_probe.cc - a sentinel for the story 1.4 public surface (ADR-0010 section 3).
//
// It declares and defines exactly the M0 minimal C API as recorded in epic-1-stories.md
// story 1.4 (pc_doc_open, pc_doc_page_count, pc_page_render), with C linkage, so the two
// toolchains emit identical symbol tables and a drift between them fails the CI diff
// (nm -C --defined-only, both targets). This is deliberately NOT the real header set:
// include/pdfcore/{status.h,doc.h,page.h,render.h,backend.h} ships with story 1.4, at
// which point this file is replaced by a test that consumes the true surface.
//
// The pc_status shape mirrors ADR-0003 section 1 (struct_size checked first, detail
// pointing into static storage) so the ABI lesson is captured even in the mirror.

#include <stdint.h>

extern "C" {

typedef enum pc_error {
  PC_ERR_NONE = 0,
  PC_ERR_ARGUMENT = 1,
  PC_ERR_UNSUPPORTED = 2,
  PC_ERR_BACKEND = 3
} pc_error;

typedef struct {
  uint32_t size;
  pc_error code;
  uint32_t detail_id;
  const char* detail;
} pc_status;

pc_status pc_doc_open(const char* path, void** out);
pc_status pc_doc_close(void* doc);
pc_status pc_doc_page_count(void* doc, uint32_t* out);
pc_status pc_page_render(void* doc, uint32_t index, void* out_surface);

}  // extern "C"

pc_status pc_doc_open(const char*, void**) {
  pc_status s = {sizeof(pc_status), PC_ERR_BACKEND, 0, "abi probe placeholder"};
  return s;
}

pc_status pc_doc_close(void*) {
  pc_status s = {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
  return s;
}

pc_status pc_doc_page_count(void*, uint32_t* out) {
  *out = 1;
  pc_status s = {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
  return s;
}

pc_status pc_page_render(void*, uint32_t, void*) {
  pc_status s = {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
  return s;
}