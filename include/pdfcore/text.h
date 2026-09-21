#ifndef PDFCORE_TEXT_H
#define PDFCORE_TEXT_H

#include <stddef.h>
#include <stdint.h>

#include "status.h"
#include "doc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_text_run {
  uint32_t size;
  float rect[4];
  uint32_t font_face;
  const char* utf8;
} pc_text_run;

// pc_doc_page_text: on success *out_runs owned by caller, release with pc_text_run_free(*out_runs, *out_count)
pc_status pc_doc_page_text(const pc_doc* doc, uint32_t page, pc_text_run** out_runs, uint32_t* out_count);

void pc_text_run_free(pc_text_run* runs, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif