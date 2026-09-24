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