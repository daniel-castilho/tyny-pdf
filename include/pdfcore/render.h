#ifndef PDFCORE_RENDER_H
#define PDFCORE_RENDER_H

#include <stddef.h>
#include <stdint.h>

#include "doc.h"
#include "page.h"
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum pc_render_backend {
  PC_RENDER_BACKEND_NULL = 0,
  PC_RENDER_BACKEND_MUPDF = 1,
} pc_render_backend;

typedef struct pc_render_context pc_render_context;

pc_status pc_render_context_create(pc_render_backend backend, pc_render_context** out_ctx);
void pc_render_context_destroy(pc_render_context* ctx);

pc_status pc_doc_open_with_ctx(pc_render_context* ctx, const char* path, const char* password,
                               pc_doc** out_doc);

#ifdef __cplusplus
}
#endif

#endif