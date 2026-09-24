// R32.1 (abi 1.2, story 6.1) - hit-test a point against page text layout.
// Returns the text box (quad + byte range) containing the point, or PC_ERR_RANGE.

#include "pdfcore/selection.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/backend.h"
#include "pdfcore/geom.h"
#include "pdfcore/page.h"
#include "pdfcore/status.h"

static inline int point_in_quad(const pc_point* p, const pc_quad* q) {
  const float x = p->x;
  const float y = p->y;

  auto cross = [](float x1, float y1, float x2, float y2) -> float { return x1 * y2 - y1 * x2; };

  float c1 = cross(q->ur_x - q->ul_x, q->ur_y - q->ul_y, x - q->ul_x, y - q->ul_y);
  float c2 = cross(q->lr_x - q->ur_x, q->lr_y - q->ur_y, x - q->ur_x, y - q->ur_y);
  float c3 = cross(q->ll_x - q->lr_x, q->ll_y - q->lr_y, x - q->lr_x, y - q->lr_y);
  float c4 = cross(q->ul_x - q->ll_x, q->ul_y - q->ll_y, x - q->ll_x, y - q->ll_y);

  return (c1 >= 0 && c2 >= 0 && c3 >= 0 && c4 >= 0) || (c1 <= 0 && c2 <= 0 && c3 <= 0 && c4 <= 0);
}

pc_status pc_selection_hit_test(const pc_backend_api* api, void* page, const pc_point* device_point,
                                float dpi, const pc_rect* page_crop_box,
                                pc_selection_result* out_result) {
  if (!api || !page || !device_point || !page_crop_box || !out_result) {
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
    pc_status err = {sizeof(pc_status), PC_ERR_RANGE, 0, "no text on page"};
    return err;
  }

  float scale = dpi / 72.0f;
  pc_point page_point = {device_point->x / scale + page_crop_box->x0,
                         device_point->y / scale + page_crop_box->y0};

  for (uint32_t i = 0; i < count; ++i) {
    if (point_in_quad(&page_point, &boxes[i].quad)) {
      out_result->page_index = 0;
      out_result->quad = boxes[i].quad;
      out_result->byte_offset = boxes[i].byte_offset;
      out_result->byte_len = boxes[i].byte_len;
      api->page_text_layout_free(utf8, boxes);
      pc_status ok = {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
      return ok;
    }
  }

  api->page_text_layout_free(utf8, boxes);
  pc_status err = {sizeof(pc_status), PC_ERR_RANGE, 0, "point not in any text box"};
  return err;
}

void pc_selection_results_free(pc_selection_results* results) {
  if (results && results->items) {
    free(results->items);
    results->items = nullptr;
    results->count = 0;
  }
}

// Extend selection from anchor byte to target byte on the same page.
// Returns a selection covering the byte range between them.
pc_status pc_selection_extend(const pc_backend_api* api, void* page, uint32_t anchor_byte,
                              uint32_t target_byte, float /*dpi*/, const pc_rect* page_crop_box,
                              pc_selection_result* out_result) {
  if (!api || !page || !page_crop_box || !out_result) {
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
    pc_status err = {sizeof(pc_status), PC_ERR_RANGE, 0, "no text on page"};
    return err;
  }

  uint32_t start = anchor_byte < target_byte ? anchor_byte : target_byte;
  uint32_t end = anchor_byte < target_byte ? target_byte : anchor_byte;

  // Find the first box containing or after start, and the last box before or at end
  uint32_t first_box = count;
  uint32_t last_box = 0;
  if (start < end) {
    // Normal range: find boxes intersecting [start, end)
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t box_start = boxes[i].byte_offset;
      uint32_t box_end = boxes[i].byte_offset + boxes[i].byte_len;
      if (box_end > start && first_box == count) {
        first_box = i;
      }
      if (box_start < end) {
        last_box = i;
      }
    }
    if (first_box == count || last_box < first_box) {
      api->page_text_layout_free(utf8, boxes);
      pc_status err = {sizeof(pc_status), PC_ERR_RANGE, 0, "extend range outside text"};
      return err;
    }
  } else {
    // Zero-length selection at start == end
    // Find the box containing or at the position
    first_box = count;
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t box_end = boxes[i].byte_offset + boxes[i].byte_len;
      if (box_end > start) {
        first_box = i;
        break;
      }
    }
    last_box = first_box;
    if (first_box == count) {
      api->page_text_layout_free(utf8, boxes);
      pc_status err = {sizeof(pc_status), PC_ERR_RANGE, 0, "extend position outside text"};
      return err;
    }
  }

  // Compute union quad of all boxes in range
  pc_quad union_quad = boxes[first_box].quad;
  for (uint32_t i = first_box + 1; i <= last_box; ++i) {
    if (boxes[i].quad.ul_x < union_quad.ul_x)
      union_quad.ul_x = boxes[i].quad.ul_x;
    if (boxes[i].quad.ul_y < union_quad.ul_y)
      union_quad.ul_y = boxes[i].quad.ul_y;
    if (boxes[i].quad.ur_x > union_quad.ur_x)
      union_quad.ur_x = boxes[i].quad.ur_x;
    if (boxes[i].quad.ur_y > union_quad.ur_y)
      union_quad.ur_y = boxes[i].quad.ur_y;
    if (boxes[i].quad.ll_x < union_quad.ll_x)
      union_quad.ll_x = boxes[i].quad.ll_x;
    if (boxes[i].quad.ll_y < union_quad.ll_y)
      union_quad.ll_y = boxes[i].quad.ll_y;
    if (boxes[i].quad.lr_x > union_quad.lr_x)
      union_quad.lr_x = boxes[i].quad.lr_x;
    if (boxes[i].quad.lr_y > union_quad.lr_y)
      union_quad.lr_y = boxes[i].quad.lr_y;
  }

  out_result->page_index = 0;
  out_result->quad = union_quad;
  out_result->byte_offset = start;
  out_result->byte_len = end - start;

  api->page_text_layout_free(utf8, boxes);
  pc_status ok = {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
  return ok;
}

// Get the text content of a selection as UTF-8.
pc_status pc_selection_get_text(const pc_backend_api* api, void* page, uint32_t byte_offset,
                                uint32_t byte_len, char** out_utf8, uint32_t* out_byte_len) {
  if (!api || !page || !out_utf8 || !out_byte_len) {
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

  if (count == 0 || !utf8 || !utf8[0] || byte_offset + byte_len > strlen(utf8)) {
    if (utf8)
      api->page_text_layout_free(utf8, nullptr);
    if (boxes)
      api->page_text_layout_free(nullptr, boxes);
    pc_status err = {sizeof(pc_status), PC_ERR_RANGE, 0, "selection out of bounds"};
    return err;
  }

  *out_utf8 = (char*)malloc(byte_len + 1);
  if (!*out_utf8) {
    api->page_text_layout_free(utf8, boxes);
    pc_status err = {sizeof(pc_status), PC_ERR_MEMORY, 0, "allocation failed"};
    return err;
  }
  memcpy(*out_utf8, utf8 + byte_offset, byte_len);
  (*out_utf8)[byte_len] = '\0';
  *out_byte_len = byte_len;

  api->page_text_layout_free(utf8, boxes);
  pc_status ok = {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
  return ok;
}