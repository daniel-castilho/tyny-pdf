#ifndef PDFCORE_SELECTION_H
#define PDFCORE_SELECTION_H

#include "pdfcore/backend.h"
#include "pdfcore/geom.h"
#include "pdfcore/page.h"
#include "pdfcore/status.h"

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

pc_status pc_selection_hit_test(const pc_backend_api* api, void* page, const pc_point* device_point,
                                float dpi, const pc_rect* page_crop_box,
                                pc_selection_result* out_result);

void pc_selection_results_free(pc_selection_results* results);

#ifdef __cplusplus
}
#endif

#endif