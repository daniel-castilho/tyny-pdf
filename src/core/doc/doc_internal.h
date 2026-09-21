#ifndef PDFCORE_DOC_INTERNAL_H
#define PDFCORE_DOC_INTERNAL_H

#include "pdfcore/doc.h"
#include "pdfcore/backend.h"
#include "pdfcore/page.h"
#include "pdfcore/annotation.h"

struct pc_doc {
  const pc_backend_api* backend;
  void* backend_doc;
  uint32_t page_count;
  pc_page_box* page_boxes;
  char sha256_hex[65];
  pc_annotation_list* annotations;
};

#endif