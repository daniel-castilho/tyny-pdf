#ifndef PDFCORE_BACKEND_H
#define PDFCORE_BACKEND_H

#include <stddef.h>
#include <stdint.h>

#include "doc.h"
#include "page.h"
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_backend_api pc_backend_api;

struct pc_backend_api {
  uint32_t abi_major;
  uint32_t abi_minor;
  uint32_t struct_size;

  pc_status (*doc_open)(const char* path, const char* password, void** out_backend_doc);
  void (*doc_close)(void* backend_doc);
  uint32_t (*doc_page_count)(void* backend_doc);
  pc_status (*page_get)(void* backend_doc, uint32_t index, void** out_backend_page);
  pc_status (*page_render)(void* backend_page, const pc_render_params* params,
                           pc_pixmap* out_pixmap);
  void (*page_free)(void* backend_page);
  void (*pixmap_free)(pc_pixmap* pixmap);
  const char* (*get_last_error)(void* backend_doc);
};

#define PC_BACKEND_API_INIT \
  { 1, 0, sizeof(pc_backend_api) }

extern pc_backend_api pc_null_backend_api;
const pc_backend_api* pc_null_backend_get_api(void);

extern pc_backend_api pc_mupdf_backend_api;
const pc_backend_api* pc_mupdf_backend_get_api(void);

#ifdef __cplusplus
}
#endif

#endif