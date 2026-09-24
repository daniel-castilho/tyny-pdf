// R37.x (abi 1.2, story 6.3) - search text on a page and return highlight quads.
// Case-insensitive UTF-8 search, returns non-overlapping hits sorted by byte offset.

#include "pdfcore/search.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/backend.h"
#include "pdfcore/geom.h"
#include "pdfcore/selection.h"
#include "pdfcore/status.h"

static inline int utf8_to_lower_copy(const char* src, size_t len, char* dst) {
  // Simple ASCII case-insensitive; for full Unicode would need ICU or similar.
  // This is a minimal implementation for ASCII queries.
  for (size_t i = 0; i < len; ++i) {
    unsigned char c = (unsigned char)src[i];
    dst[i] = (char)(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
  }
  return (int)len;
}

static int find_next(const char* haystack, size_t haystack_len, const char* needle,
                     size_t needle_len, size_t start) {
  if (needle_len == 0 || needle_len > haystack_len)
    return -1;
  for (size_t i = start; i + needle_len <= haystack_len; ++i) {
    int match = 1;
    for (size_t j = 0; j < needle_len; ++j) {
      unsigned char hc = (unsigned char)haystack[i + j];
      unsigned char nc = (unsigned char)needle[j];
      char hl = (hc >= 'A' && hc <= 'Z') ? hc + ('a' - 'A') : hc;
      char nl = (nc >= 'A' && nc <= 'Z') ? nc + ('a' - 'A') : nc;
      if (hl != nl) {
        match = 0;
        break;
      }
    }
    if (match)
      return (int)i;
  }
  return -1;
}

pc_status pc_search_page(const pc_backend_api* api, void* page, const char* query, float /*dpi*/,
                         const pc_rect* page_crop_box, pc_search_results* out_results) {
  if (!api || !page || !query || !page_crop_box || !out_results) {
    pc_status s = {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
    return s;
  }

  if (!api->page_text_layout || !api->page_text_layout_free) {
    pc_status s = {sizeof(pc_status), PC_ERR_CAPABILITY, 0, "text layout not available"};
    return s;
  }

  char* utf8 = nullptr;
  pc_text_box* boxes = nullptr;
  uint32_t count = 0;
  pc_status s = api->page_text_layout(page, &utf8, &boxes, &count);
  if (s.code != PC_ERR_NONE) {
    if (utf8)
      api->page_text_layout_free(utf8, nullptr);
    if (boxes)
      api->page_text_layout_free(nullptr, boxes);
    return s;
  }

  if (count == 0 || !utf8 || !utf8[0]) {
    if (utf8)
      api->page_text_layout_free(utf8, nullptr);
    if (boxes)
      api->page_text_layout_free(nullptr, boxes);
    out_results->items = nullptr;
    out_results->count = 0;
    pc_status ok = {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
    return ok;
  }

  size_t utf8_len = strlen(utf8);
  size_t query_len = strlen(query);

  // Count hits first
  uint32_t hit_count = 0;
  size_t pos = 0;
  while (pos < utf8_len) {
    int found = find_next(utf8, utf8_len, query, query_len, pos);
    if (found < 0)
      break;
    hit_count++;
    pos = (size_t)found + query_len;
  }

  pc_search_hit* hits = nullptr;
  if (hit_count > 0) {
    hits = (pc_search_hit*)malloc(hit_count * sizeof(pc_search_hit));
    if (!hits) {
      api->page_text_layout_free(utf8, boxes);
      pc_status err = {sizeof(pc_status), PC_ERR_MEMORY, 0, "allocation failed"};
      return err;
    }

    pos = 0;
    uint32_t hit_idx = 0;
    while (pos < utf8_len && hit_idx < hit_count) {
      int found = find_next(utf8, utf8_len, query, query_len, pos);
      if (found < 0)
        break;

      // Find the box containing this byte offset
      for (uint32_t i = 0; i < count; ++i) {
        uint32_t box_start = boxes[i].byte_offset;
        uint32_t box_end = boxes[i].byte_offset + boxes[i].byte_len;
        if ((uint32_t)found >= box_start && (uint32_t)found < box_end) {
          hits[hit_idx].quad = boxes[i].quad;
          hits[hit_idx].byte_offset = (uint32_t)found;
          hits[hit_idx].byte_len = (uint32_t)query_len;
          hit_idx++;
          break;
        }
      }
      pos = (size_t)found + query_len;
    }
  }

  api->page_text_layout_free(utf8, boxes);

  out_results->items = hits;
  out_results->count = hit_count;

  pc_status ok = {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
  return ok;
}

void pc_search_results_free(pc_search_results* results) {
  if (results && results->items) {
    free(results->items);
    results->items = nullptr;
    results->count = 0;
  }
}