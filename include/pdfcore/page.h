#ifndef PDFCORE_PAGE_H
#define PDFCORE_PAGE_H

#include <stddef.h>
#include <stdint.h>

#include "geom.h"
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_page pc_page;

typedef struct pc_render_params {
  uint32_t dpi;
  pc_rect clip;
  int render_annots;
  int render_text;
} pc_render_params;

typedef struct pc_pixmap {
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  uint8_t* data;
  uint32_t format;
} pc_pixmap;

pc_status pc_page_get_box(const pc_page* page, pc_page_box* out);

pc_status pc_page_render(const pc_page* page, const pc_render_params* params,
                         pc_pixmap* out_pixmap);

void pc_pixmap_free(pc_pixmap* pixmap);

#ifdef __cplusplus
}
#endif

#endif