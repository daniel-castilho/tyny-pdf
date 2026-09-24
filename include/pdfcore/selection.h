#ifndef PDFCORE_SELECTION_H
#define PDFCORE_SELECTION_H

#include "pdfcore/backend.h"
#include "pdfcore/geom.h"
#include "pdfcore/page.h"
#include "pdfcore/status.h"
#include "pdfcore/text.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_selection_result {
  uint32_t page_index;
  pc_quad quad;
  uint32_t byte_offset;
  uint32_t byte_len;
} pc_selection_result;

typedef struct pc_selection_results {
  pc_selection_result* items;
  uint32_t count;
} pc_selection_results;

// Hit-test a device-space point against page text layout.
pc_status pc_selection_hit_test(const pc_backend_api* api, void* page, const pc_point* device_point,
                                float dpi, const pc_rect* page_crop_box,
                                pc_selection_result* out_result);

// Extend selection from an anchor byte offset to a target byte offset on the same page.
// Returns the new selection covering [min(anchor, target), max(anchor, target)).
pc_status pc_selection_extend(const pc_backend_api* api, void* page, uint32_t anchor_byte,
                              uint32_t target_byte, float dpi, const pc_rect* page_crop_box,
                              pc_selection_result* out_result);

// Get the text content of a selection as UTF-8 (caller frees with pc_text_run_free).
pc_status pc_selection_get_text(const pc_backend_api* api, void* page, uint32_t byte_offset,
                                uint32_t byte_len, char** out_utf8, uint32_t* out_byte_len);

void pc_selection_results_free(pc_selection_results* results);

#ifdef __cplusplus
}
#endif

#endif