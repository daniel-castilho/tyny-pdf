// Search module for Tyny PDF — story 6.3.
// Platform-neutral C ABI: finds text and returns highlight quads.

#pragma once

#include <pdfcore/selection.h>
#include <pdfcore/status.h>

#ifdef __cplusplus
extern "C" {
#endif

// One search hit: a quad + byte range in the page's UTF-8 text.
typedef struct pc_search_hit {
  pc_quad quad;
  uint32_t byte_offset;
  uint32_t byte_len;
} pc_search_hit;

// Search results array (caller frees with pc_search_results_free).
typedef struct pc_search_results {
  pc_search_hit* items;
  uint32_t count;
} pc_search_results;

// Search a page for `query` (UTF-8, case-insensitive).
// Returns all non-overlapping hits as quads + byte ranges.
// Results are sorted by byte_offset ascending.
// Errors:
//  - PC_ERR_ARGUMENT: null api / page / query / out_results / crop_box
//  - PC_ERR_CAPABILITY: backend doesn't declare PC_CAP_TEXT_LAYOUT
//  - PC_ERR_MEMORY: allocation failure
pc_status pc_search_page(const pc_backend_api* api, void* page, const char* query, float dpi,
                         const pc_rect* page_crop_box, pc_search_results* out_results);

// Free results returned by pc_search_page.
void pc_search_results_free(pc_search_results* results);

#ifdef __cplusplus
}
#endif