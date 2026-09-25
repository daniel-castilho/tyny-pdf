// R32.1, R32.2 - hit-test unit tests (synthetic layouts)

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore/backend.h"
#include "pdfcore/geom.h"
#include "pdfcore/page.h"
#include "pdfcore/selection.h"
#include "pdfcore/status.h"

// Mock backend that returns predefined text layout
struct MockBackend {
  char* utf8;
  pc_text_box* boxes;
  uint32_t count;
};

static pc_status mock_page_text_layout(void* page, char** out_utf8, pc_text_box** out_boxes,
                                       uint32_t* out_count) __attribute__((used));
static pc_status mock_page_text_layout(void* page, char** out_utf8, pc_text_box** out_boxes,
                                       uint32_t* out_count) {
  (void)page;
  MockBackend* mb = static_cast<MockBackend*>(page);
  *out_utf8 = mb->utf8;
  *out_boxes = mb->boxes;
  *out_count = mb->count;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

static void mock_page_text_layout_free(char* utf8, pc_text_box* boxes) __attribute__((used));
static void mock_page_text_layout_free(char* utf8, pc_text_box* boxes) {
  (void)utf8;
  (void)boxes;
  // No-op: owned by test
}

// Minimal null backend functions for unused vtable entries
static pc_status null_doc_open(const char*, const char*, void**) __attribute__((used));
static pc_status null_doc_open(const char*, const char*, void**) {
  return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, nullptr};
}
static void null_doc_close(void*) __attribute__((used));
static void null_doc_close(void*) {
}
static uint32_t null_doc_page_count(void*) __attribute__((used));
static uint32_t null_doc_page_count(void*) {
  return 0;
}
static pc_status null_page_get(void*, uint32_t, void**) __attribute__((used));
static pc_status null_page_get(void*, uint32_t, void**) {
  return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, nullptr};
}
static void null_page_free(void*) __attribute__((used));
static void null_page_free(void*) {
}
static int null_doc_has_capability(void*, uint32_t) __attribute__((used));
static int null_doc_has_capability(void*, uint32_t) {
  return 0;
}
static pc_status null_doc_find_tables(void*, pc_rect*, size_t*, size_t) __attribute__((used));
static pc_status null_doc_find_tables(void*, pc_rect*, size_t*, size_t) {
  return {sizeof(pc_status), PC_ERR_CAPABILITY, 0, nullptr};
}
static uint32_t null_face_count(void*) __attribute__((used));
static uint32_t null_face_count(void*) {
  return 0;
}
static pc_status null_face_coverage(void*, uint32_t, uint32_t, int*) __attribute__((used));
static pc_status null_face_coverage(void*, uint32_t, uint32_t, int*) {
  return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, nullptr};
}
static pc_status null_page_render(void*, const pc_render_params*, pc_pixmap*) __attribute__((used));
static pc_status null_page_render(void*, const pc_render_params*, pc_pixmap*) {
  return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, nullptr};
}
static void null_pixmap_free(pc_pixmap*) __attribute__((used));
static void null_pixmap_free(pc_pixmap*) {
}

// Complete mock backend API
static pc_backend_api mock_api = {
    PC_BACKEND_API_VERSION_MAJOR,
    PC_BACKEND_API_VERSION_MINOR,
    sizeof(pc_backend_api),
    null_doc_open,
    null_doc_close,
    null_doc_page_count,
    null_page_get,
    nullptr,  // page_get_box
    null_page_render,
    null_page_free,
    null_pixmap_free,
    nullptr,  // get_last_error
    null_doc_has_capability,
    null_doc_find_tables,
    null_face_count,
    null_face_coverage,
    mock_page_text_layout,
    mock_page_text_layout_free,
    nullptr,  // form_list_fields
    nullptr,  // form_list_free
    nullptr,  // form_fdf_export
    nullptr,  // form_fdf_import
    nullptr,  // form_fdf_free
    nullptr,  // form_flatten (abi 1.4)
};

// Null backend API (no text layout capability)
static pc_backend_api null_api = {
    PC_BACKEND_API_VERSION_MAJOR,
    PC_BACKEND_API_VERSION_MINOR,
    sizeof(pc_backend_api),
    null_doc_open,
    null_doc_close,
    null_doc_page_count,
    null_page_get,
    nullptr,  // page_get_box
    null_page_render,
    null_page_free,
    null_pixmap_free,
    nullptr,  // get_last_error
    null_doc_has_capability,
    null_doc_find_tables,
    null_face_count,
    null_face_coverage,
    nullptr,  // page_text_layout
    nullptr,  // page_text_layout_free
    nullptr,  // form_list_fields
    nullptr,  // form_list_free
    nullptr,  // form_fdf_export
    nullptr,  // form_fdf_import
    nullptr,  // form_fdf_free
    nullptr,  // form_flatten (abi 1.4)
};

static void run_test(const char* name, MockBackend* mb, const pc_point* device_point, float dpi,
                     const pc_rect* crop_box, const pc_selection_result* expected,
                     pc_error expected_code) {
  pc_selection_result result = {};
  pc_status s = pc_selection_hit_test(&mock_api, mb, device_point, dpi, crop_box, &result);

  int pass = 1;
  if (s.code != expected_code) {
    printf("FAIL %s: expected code %u, got %u\n", name, expected_code, s.code);
    pass = 0;
  } else if (expected_code == PC_ERR_NONE) {
    if (result.byte_offset != expected->byte_offset || result.byte_len != expected->byte_len ||
        result.quad.ul_x != expected->quad.ul_x || result.quad.ul_y != expected->quad.ul_y ||
        result.quad.ur_x != expected->quad.ur_x || result.quad.ur_y != expected->quad.ur_y ||
        result.quad.ll_x != expected->quad.ll_x || result.quad.ll_y != expected->quad.ll_y ||
        result.quad.lr_x != expected->quad.lr_x || result.quad.lr_y != expected->quad.lr_y) {
      printf("FAIL %s: result mismatch\n", name);
      pass = 0;
    }
  }
  if (pass) {
    printf("PASS %s\n", name);
  } else {
    exit(1);
  }
}

static pc_selection_result make_expected(const pc_text_box* box, uint32_t byte_offset,
                                         uint32_t byte_len) {
  pc_selection_result r = {};
  r.quad = box->quad;
  r.byte_offset = byte_offset;
  r.byte_len = byte_len;
  return r;
}

int main() {
  // Test 1: Single line, point inside first character
  {
    MockBackend mb = {};
    mb.utf8 = strdup("Hello");
    mb.count = 5;
    mb.boxes = (pc_text_box*)calloc(5, sizeof(pc_text_box));
    for (int i = 0; i < 5; ++i) {
      mb.boxes[i].quad.ul_x = 100.0 + i * 10.0;
      mb.boxes[i].quad.ul_y = 100.0;
      mb.boxes[i].quad.ur_x = 110.0 + i * 10.0;
      mb.boxes[i].quad.ur_y = 100.0;
      mb.boxes[i].quad.ll_x = 100.0 + i * 10.0;
      mb.boxes[i].quad.ll_y = 120.0;
      mb.boxes[i].quad.lr_x = 110.0 + i * 10.0;
      mb.boxes[i].quad.lr_y = 120.0;
      mb.boxes[i].byte_offset = i;
      mb.boxes[i].byte_len = 1;
    }

    pc_point pt = {105.0, 110.0};     // Inside first char at 100-110, 100-120
    pc_rect crop = {0, 0, 612, 792};  // Letter size
    pc_selection_result expected = make_expected(&mb.boxes[0], 0, 1);

    run_test("single_line_first_char", &mb, &pt, 72.0, &crop, &expected, PC_ERR_NONE);
    free(mb.utf8);
    free(mb.boxes);
  }

  // Test 2: Single line, point inside middle character
  {
    MockBackend mb = {};
    mb.utf8 = strdup("Hello");
    mb.count = 5;
    mb.boxes = (pc_text_box*)calloc(5, sizeof(pc_text_box));
    for (int i = 0; i < 5; ++i) {
      mb.boxes[i].quad.ul_x = 100.0 + i * 10.0;
      mb.boxes[i].quad.ul_y = 100.0;
      mb.boxes[i].quad.ur_x = 110.0 + i * 10.0;
      mb.boxes[i].quad.ur_y = 100.0;
      mb.boxes[i].quad.ll_x = 100.0 + i * 10.0;
      mb.boxes[i].quad.ll_y = 120.0;
      mb.boxes[i].quad.lr_x = 110.0 + i * 10.0;
      mb.boxes[i].quad.lr_y = 120.0;
      mb.boxes[i].byte_offset = i;
      mb.boxes[i].byte_len = 1;
    }

    pc_point pt = {125.0, 110.0};  // 3rd char (index 2) at 120-130, 100-120
    pc_rect crop = {0, 0, 612, 792};
    pc_selection_result expected = make_expected(&mb.boxes[2], 2, 1);

    run_test("single_line_middle_char", &mb, &pt, 72.0, &crop, &expected, PC_ERR_NONE);
    free(mb.utf8);
    free(mb.boxes);
  }

  // Test 3: Point outside any text box
  {
    MockBackend mb = {};
    mb.utf8 = strdup("Hello");
    mb.count = 5;
    mb.boxes = (pc_text_box*)calloc(5, sizeof(pc_text_box));
    for (int i = 0; i < 5; ++i) {
      mb.boxes[i].quad.ul_x = 100.0 + i * 10.0;
      mb.boxes[i].quad.ul_y = 100.0;
      mb.boxes[i].quad.ur_x = 110.0 + i * 10.0;
      mb.boxes[i].quad.ur_y = 100.0;
      mb.boxes[i].quad.ll_x = 100.0 + i * 10.0;
      mb.boxes[i].quad.ll_y = 120.0;
      mb.boxes[i].quad.lr_x = 110.0 + i * 10.0;
      mb.boxes[i].quad.lr_y = 120.0;
      mb.boxes[i].byte_offset = i;
      mb.boxes[i].byte_len = 1;
    }

    pc_point pt = {500.0, 500.0};  // Far from text (text is at 100-150, 100-120)
    pc_rect crop = {0, 0, 612, 792};
    pc_selection_result expected = {};

    run_test("point_outside_text", &mb, &pt, 72.0, &crop, &expected, PC_ERR_RANGE);
    free(mb.utf8);
    free(mb.boxes);
  }

  // Test 4: Null backend (capability not supported)
  {
    pc_point pt = {100.0, 100.0};
    pc_rect crop = {0, 0, 612, 792};
    pc_selection_result result = {};

    // Use a dummy page pointer (non-null) to test capability check
    void* dummy_page = (void*)0x1;
    pc_status s = pc_selection_hit_test(&null_api, dummy_page, &pt, 72.0, &crop, &result);
    if (s.code == PC_ERR_CAPABILITY) {
      printf("PASS null_backend_capability\n");
    } else {
      printf("FAIL null_backend_capability: expected PC_ERR_CAPABILITY, got %u\n", s.code);
      return 1;
    }
  }

  // Test 5: Null arguments
  {
    MockBackend mb = {};
    mb.utf8 = strdup("Hi");
    mb.count = 2;
    mb.boxes = (pc_text_box*)calloc(2, sizeof(pc_text_box));
    pc_point pt = {100.0, 100.0};
    pc_rect crop = {0, 0, 612, 792};
    pc_selection_result result = {};

    pc_status s = pc_selection_hit_test(&mock_api, nullptr, &pt, 72.0, &crop, &result);
    if (s.code == PC_ERR_ARGUMENT) {
      printf("PASS null_page_argument\n");
    } else {
      printf("FAIL null_page_argument: expected PC_ERR_ARGUMENT, got %u\n", s.code);
      return 1;
    }

    s = pc_selection_hit_test(&mock_api, &mb, nullptr, 72.0, &crop, &result);
    if (s.code == PC_ERR_ARGUMENT) {
      printf("PASS null_device_point_argument\n");
    } else {
      printf("FAIL null_device_point_argument: expected PC_ERR_ARGUMENT, got %u\n", s.code);
      return 1;
    }

    s = pc_selection_hit_test(&mock_api, &mb, &pt, 72.0, nullptr, &result);
    if (s.code == PC_ERR_ARGUMENT) {
      printf("PASS null_crop_box_argument\n");
    } else {
      printf("FAIL null_crop_box_argument: expected PC_ERR_ARGUMENT, got %u\n", s.code);
      return 1;
    }

    s = pc_selection_hit_test(&mock_api, &mb, &pt, 72.0, &crop, nullptr);
    if (s.code == PC_ERR_ARGUMENT) {
      printf("PASS null_out_result_argument\n");
    } else {
      printf("FAIL null_out_result_argument: expected PC_ERR_ARGUMENT, got %u\n", s.code);
      return 1;
    }
    free(mb.utf8);
    free(mb.boxes);
  }

  // Test 6: Empty page (no text)
  {
    MockBackend mb = {};
    mb.utf8 = strdup("");
    mb.count = 0;
    mb.boxes = nullptr;
    pc_point pt = {100.0, 100.0};
    pc_rect crop = {0, 0, 612, 792};
    pc_selection_result result = {};

    pc_status s = pc_selection_hit_test(&mock_api, &mb, &pt, 72.0, &crop, &result);
    if (s.code == PC_ERR_RANGE) {
      printf("PASS empty_page\n");
    } else {
      printf("FAIL empty_page: expected PC_ERR_RANGE, got %u\n", s.code);
      return 1;
    }
    free(mb.utf8);
  }

  // Test 7: Crop box with non-zero origin
  {
    MockBackend mb = {};
    mb.utf8 = strdup("Test");
    mb.count = 4;
    mb.boxes = (pc_text_box*)calloc(4, sizeof(pc_text_box));
    for (int i = 0; i < 4; ++i) {
      mb.boxes[i].quad.ul_x = 100.0 + i * 10.0;
      mb.boxes[i].quad.ul_y = 100.0;
      mb.boxes[i].quad.ur_x = 110.0 + i * 10.0;
      mb.boxes[i].quad.ur_y = 100.0;
      mb.boxes[i].quad.ll_x = 100.0 + i * 10.0;
      mb.boxes[i].quad.ll_y = 120.0;
      mb.boxes[i].quad.lr_x = 110.0 + i * 10.0;
      mb.boxes[i].quad.lr_y = 120.0;
      mb.boxes[i].byte_offset = i;
      mb.boxes[i].byte_len = 1;
    }

    // Crop box with non-zero origin
    // page_point = device_point/scale + crop_box
    // To hit page x=105 with crop_box.x0=100: 105 = device_x/1 + 100 => device_x = 5
    pc_point pt = {5.0, 10.0};
    pc_rect crop = {100, 100, 512, 692};  // CropBox offset
    pc_selection_result expected = make_expected(&mb.boxes[0], 0, 1);

    run_test("crop_box_offset", &mb, &pt, 72.0, &crop, &expected, PC_ERR_NONE);
    free(mb.utf8);
    free(mb.boxes);
  }

  printf("\nAll tests passed\n");
  return 0;
}