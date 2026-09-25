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
  int render_widgets;  // abi 1.4 (story 7.4): draw form widgets - the pre-flatten half of
                       // the bake-equality claim (the post-flatten half is baked content)
} pc_render_params;

typedef struct pc_pixmap {
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  uint8_t* data;
  uint32_t format;
} pc_pixmap;

/// One cluster of page text and where it lives: a quad in user-space points
/// plus the UTF-8 byte range it covers in the page's text buffer returned by
/// the backend's page_text_layout (story 6.1, abi 1.2). Value type only; it
/// never references engine memory (R-M4).
typedef struct pc_text_box {
  pc_quad quad;
  uint32_t byte_offset;
  uint32_t byte_len;
} pc_text_box;

/// Page box of the held page. PC_ERR_NONE or PC_ERR_ARGUMENT.
pc_status pc_page_get_box(const pc_page* page, pc_page_box* out);

/// Render the page into *out_pixmap. PC_ERR_NONE, PC_ERR_ARGUMENT, PC_ERR_MEMORY, PC_ERR_CORRUPT.
/// The pixmap buffer is owned by the caller and released with pc_pixmap_free.
pc_status pc_page_render(const pc_page* page, const pc_render_params* params,
                         pc_pixmap* out_pixmap);

/// Free a pixmap produced by pc_page_render (R-M6: the backend allocated it).
void pc_pixmap_free(pc_pixmap* pixmap);

#ifdef __cplusplus
}
#endif

#endif