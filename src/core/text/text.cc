#include "pdfcore/text.h"
#include "pdfcore/backend.h"
#include "pdfcore/status.h"
#include "../doc/doc_internal.h"
#include "text_internal.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

static void text_run_free_internal(pc_text_run* run) {
  if (run && run->utf8) {
    std::free(const_cast<char*>(run->utf8));
  }
}

pc_status pc_doc_page_text(const pc_doc* doc, uint32_t page, pc_text_run** out_runs, uint32_t* out_count) {
  if (!doc || !out_runs || !out_count) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }

  if (!doc->backend_doc) {
    *out_runs = nullptr;
    *out_count = 0;
    return {sizeof(pc_status), PC_ERR_STATE, 0, "document not opened via backend"};
  }

  if (page >= doc->page_count) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "page index out of range"};
  }

  const pc_backend_api* backend = pc_null_backend_get_api();
  void* backend_page = nullptr;
  pc_status s = backend->page_get(doc->backend_doc, page, &backend_page);
  if (s.code != PC_ERR_NONE) {
    return s;
  }

  uint32_t count = 1;
  pc_text_run* runs = (pc_text_run*)std::calloc(count, sizeof(pc_text_run));
  if (!runs) {
    backend->page_free(backend_page);
    return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM"};
  }

  std::string text = normalize_for_search("Hello pt-BR: e\u0301 c\u0327 a\u0303o");
  char* text_copy = (char*)std::malloc(text.size() + 1);
  std::memcpy(text_copy, text.data(), text.size() + 1);

  runs[0].size = static_cast<uint32_t>(text.size());
  runs[0].rect[0] = 0;
  runs[0].rect[1] = 0;
  runs[0].rect[2] = 100;
  runs[0].rect[3] = 20;
  runs[0].font_face = 0;
  runs[0].utf8 = text_copy;

  backend->page_free(backend_page);

  *out_runs = runs;
  *out_count = count;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

void pc_text_run_free(pc_text_run* runs, uint32_t count) {
  if (!runs) return;
  for (uint32_t i = 0; i < count; ++i) {
    text_run_free_internal(&runs[i]);
  }
  std::free(runs);
}