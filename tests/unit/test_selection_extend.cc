// R34.1 - selection extend test

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore/backend.h"
#include "pdfcore/geom.h"
#include "pdfcore/page.h"
#include "pdfcore/selection.h"
#include "pdfcore/status.h"

// Minimal null backend functions
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

// Mock backend for text layout
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

int main() {
  int failures = 0;

  // Test 1: Extend selection from anchor to target (forward)
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

    pc_page_box box = {};
    box.cropbox = {0, 0, 612, 792};

    pc_selection_result result = {};
    pc_status s = pc_selection_extend(&mock_api, &mb, 0, 3, 72.0, &box.cropbox, &result);
    if (s.code == PC_ERR_NONE && result.byte_offset == 0 && result.byte_len == 3) {
      printf("PASS extend_forward\n");
    } else {
      printf("FAIL extend_forward: code=%u, offset=%u, len=%u\n", s.code, result.byte_offset,
             result.byte_len);
      failures++;
    }
    free(mb.utf8);
    free(mb.boxes);
  }

  // Test 2: Extend selection from anchor to target (backward)
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

    pc_page_box box = {};
    box.cropbox = {0, 0, 612, 792};

    pc_selection_result result = {};
    pc_status s = pc_selection_extend(&mock_api, &mb, 3, 0, 72.0, &box.cropbox, &result);
    if (s.code == PC_ERR_NONE && result.byte_offset == 0 && result.byte_len == 3) {
      printf("PASS extend_backward\n");
    } else {
      printf("FAIL extend_backward: code=%u, offset=%u, len=%u\n", s.code, result.byte_offset,
             result.byte_len);
      failures++;
    }
    free(mb.utf8);
    free(mb.boxes);
  }

  // Test 3: Extend to same position (zero length)
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

    pc_page_box box = {};
    box.cropbox = {0, 0, 612, 792};

    pc_selection_result result = {};
    pc_status s = pc_selection_extend(&mock_api, &mb, 2, 2, 72.0, &box.cropbox, &result);
    if (s.code == PC_ERR_NONE && result.byte_offset == 2 && result.byte_len == 0) {
      printf("PASS extend_zero_length\n");
    } else {
      printf("FAIL extend_zero_length: code=%u, offset=%u, len=%u\n", s.code, result.byte_offset,
             result.byte_len);
      failures++;
    }
    free(mb.utf8);
    free(mb.boxes);
  }

  // Test 4: Null backend capability
  {
    pc_backend_api null_api = {
        PC_BACKEND_API_VERSION_MAJOR,
        PC_BACKEND_API_VERSION_MINOR,
        sizeof(pc_backend_api),
        null_doc_open,
        null_doc_close,
        null_doc_page_count,
        null_page_get,
        nullptr,
        null_page_render,
        null_page_free,
        null_pixmap_free,
        nullptr,
        null_doc_has_capability,
        null_doc_find_tables,
        null_face_count,
        null_face_coverage,
        nullptr,
        nullptr,
        nullptr,  // form_list_fields
        nullptr,  // form_list_free
        nullptr,  // form_fdf_export
        nullptr,  // form_fdf_import
        nullptr,  // form_fdf_free
        nullptr,  // form_flatten (abi 1.4)
    };

    pc_page_box box = {};
    box.cropbox = {0, 0, 612, 792};

    pc_selection_result result = {};
    pc_status s = pc_selection_extend(&null_api, (void*)0x1, 0, 1, 72.0, &box.cropbox, &result);
    if (s.code == PC_ERR_CAPABILITY) {
      printf("PASS null_backend_capability\n");
    } else {
      printf("FAIL null_backend_capability: code=%u\n", s.code);
      failures++;
    }
  }

  if (failures) {
    printf("FAIL: %d test(s) failed\n", failures);
    return 1;
  }

  printf("All R34.1 selection extend tests passed\n");
  return 0;
}