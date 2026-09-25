// R37.1, R37.2 - search unit tests (synthetic layouts)

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore/backend.h"
#include "pdfcore/geom.h"
#include "pdfcore/page.h"
#include "pdfcore/search.h"
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
}

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

// Complete mock backend API
static pc_backend_api mock_api = {
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

static void cleanup(MockBackend* mb) {
  if (mb) {
    free(mb->utf8);
    free(mb->boxes);
    mb->utf8 = nullptr;
    mb->boxes = nullptr;
  }
}

// R37.1, R37.2: test synthetic layouts with case-insensitive search
static void run_test(const char* name, MockBackend* mb, const char* query,
                     const pc_search_hit* expected_hits, uint32_t expected_count) {
  pc_page_box box = {};
  box.cropbox = {0, 0, 612, 792};

  pc_search_results results = {};
  pc_status s = pc_search_page(&mock_api, mb, query, 72.0, &box.cropbox, &results);

  int pass = 1;
  if (s.code != PC_ERR_NONE) {
    printf("FAIL %s: unexpected error code %u\n", name, s.code);
    pass = 0;
  } else if (results.count != expected_count) {
    printf("FAIL %s: expected %u hits, got %u\n", name, expected_count, results.count);
    pass = 0;
  } else {
    for (uint32_t i = 0; i < expected_count; ++i) {
      if (results.items[i].byte_offset != expected_hits[i].byte_offset ||
          results.items[i].byte_len != expected_hits[i].byte_len ||
          results.items[i].quad.ul_x != expected_hits[i].quad.ul_x ||
          results.items[i].quad.ul_y != expected_hits[i].quad.ul_y ||
          results.items[i].quad.ur_x != expected_hits[i].quad.ur_x ||
          results.items[i].quad.ur_y != expected_hits[i].quad.ur_y ||
          results.items[i].quad.ll_x != expected_hits[i].quad.ll_x ||
          results.items[i].quad.ll_y != expected_hits[i].quad.ll_y ||
          results.items[i].quad.lr_x != expected_hits[i].quad.lr_x ||
          results.items[i].quad.lr_y != expected_hits[i].quad.lr_y) {
        printf("FAIL %s: hit %u mismatch\n", name, i);
        pass = 0;
        break;
      }
    }
  }
  if (pass) {
    printf("PASS %s\n", name);
  } else {
    exit(1);
  }
  pc_search_results_free(&results);
}

int main() {
  // Test 1: Simple single hit
  {
    MockBackend mb = {};
    mb.utf8 = strdup("Hello world");
    mb.count = 2;
    mb.boxes = (pc_text_box*)calloc(2, sizeof(pc_text_box));
    mb.boxes[0].quad = {100, 100, 150, 100, 100, 120, 150, 120};
    mb.boxes[0].byte_offset = 0;
    mb.boxes[0].byte_len = 5;
    mb.boxes[1].quad = {160, 100, 210, 100, 160, 120, 210, 120};
    mb.boxes[1].byte_offset = 6;
    mb.boxes[1].byte_len = 5;

    pc_search_hit expected = {{100, 100, 150, 100, 100, 120, 150, 120}, 0, 5};
    run_test("single_hit", &mb, "Hello", &expected, 1);
    cleanup(&mb);
  }

  // Test 2: Multiple hits (case-insensitive)
  {
    MockBackend mb = {};
    mb.utf8 = strdup("Lorem ipsum dolor lorem");
    mb.count = 4;
    mb.boxes = (pc_text_box*)calloc(4, sizeof(pc_text_box));
    mb.boxes[0].quad = {100, 100, 150, 100, 100, 120, 150, 120};
    mb.boxes[0].byte_offset = 0;
    mb.boxes[0].byte_len = 5;
    mb.boxes[1].quad = {160, 100, 210, 100, 160, 120, 210, 120};
    mb.boxes[1].byte_offset = 6;
    mb.boxes[1].byte_len = 5;
    mb.boxes[2].quad = {220, 100, 270, 100, 220, 120, 270, 120};
    mb.boxes[2].byte_offset = 12;
    mb.boxes[2].byte_len = 5;
    mb.boxes[3].quad = {280, 100, 330, 100, 280, 120, 330, 120};
    mb.boxes[3].byte_offset = 18;
    mb.boxes[3].byte_len = 5;

    pc_search_hit expected[2] = {{{100, 100, 150, 100, 100, 120, 150, 120}, 0, 5},
                                 {{280, 100, 330, 100, 280, 120, 330, 120}, 18, 5}};
    run_test("multiple_hits_case_insensitive", &mb, "lorem", expected, 2);
    cleanup(&mb);
  }

  // Test 3: No hits
  {
    MockBackend mb = {};
    mb.utf8 = strdup("Hello world");
    mb.count = 2;
    mb.boxes = (pc_text_box*)calloc(2, sizeof(pc_text_box));
    mb.boxes[0].quad = {100, 100, 150, 100, 100, 120, 150, 120};
    mb.boxes[0].byte_offset = 0;
    mb.boxes[0].byte_len = 5;
    mb.boxes[1].quad = {160, 100, 210, 100, 160, 120, 210, 120};
    mb.boxes[1].byte_offset = 6;
    mb.boxes[1].byte_len = 5;

    run_test("no_hits", &mb, "xyz", nullptr, 0);
    cleanup(&mb);
  }

  // Test 4: Empty page
  {
    MockBackend mb = {};
    mb.utf8 = strdup("");
    mb.count = 0;
    mb.boxes = nullptr;

    run_test("empty_page", &mb, "hello", nullptr, 0);
    cleanup(&mb);
  }

  // Test 5: Null backend capability
  {
    pc_page_box box = {};
    box.cropbox = {0, 0, 612, 792};

    pc_search_results results = {};
    pc_status s = pc_search_page(&null_api, (void*)0x1, "hello", 72.0, &box.cropbox, &results);
    if (s.code == PC_ERR_CAPABILITY) {
      printf("PASS null_backend_capability\n");
    } else {
      printf("FAIL null_backend_capability: code=%u\n", s.code);
      return 1;
    }
  }

  // Test 6: Null arguments
  {
    MockBackend mb = {};
    mb.utf8 = strdup("Hello");
    mb.count = 1;
    mb.boxes = (pc_text_box*)calloc(1, sizeof(pc_text_box));
    mb.boxes[0].quad = {100, 100, 150, 100, 100, 120, 150, 120};
    mb.boxes[0].byte_offset = 0;
    mb.boxes[0].byte_len = 5;

    pc_page_box box = {};
    box.cropbox = {0, 0, 612, 792};
    pc_search_results results = {};

    pc_status s = pc_search_page(nullptr, &mb, "hello", 72.0, &box.cropbox, &results);
    if (s.code != PC_ERR_ARGUMENT) {
      printf("FAIL null_api_argument\n");
      return 1;
    }

    s = pc_search_page(&mock_api, nullptr, "hello", 72.0, &box.cropbox, &results);
    if (s.code != PC_ERR_ARGUMENT) {
      printf("FAIL null_page_argument\n");
      return 1;
    }

    s = pc_search_page(&mock_api, &mb, nullptr, 72.0, &box.cropbox, &results);
    if (s.code != PC_ERR_ARGUMENT) {
      printf("FAIL null_query_argument\n");
      return 1;
    }

    s = pc_search_page(&mock_api, &mb, "hello", 72.0, nullptr, &results);
    if (s.code != PC_ERR_ARGUMENT) {
      printf("FAIL null_crop_box_argument\n");
      return 1;
    }

    s = pc_search_page(&mock_api, &mb, "hello", 72.0, &box.cropbox, nullptr);
    if (s.code != PC_ERR_ARGUMENT) {
      printf("FAIL null_out_results_argument\n");
      return 1;
    }
    cleanup(&mb);
  }

  // Test 7: Query spanning multiple boxes (each character is a box)
  {
    MockBackend mb = {};
    mb.utf8 = strdup("Hi");
    mb.count = 2;
    mb.boxes = (pc_text_box*)calloc(2, sizeof(pc_text_box));
    mb.boxes[0].quad = {100, 100, 110, 100, 100, 120, 110, 120};
    mb.boxes[0].byte_offset = 0;
    mb.boxes[0].byte_len = 1;
    mb.boxes[1].quad = {110, 100, 120, 100, 110, 120, 120, 120};
    mb.boxes[1].byte_offset = 1;
    mb.boxes[1].byte_len = 1;

    pc_search_hit expected = {{100, 100, 110, 100, 100, 120, 110, 120}, 0, 1};  // only first char
    run_test("partial_hit", &mb, "H", &expected, 1);
    cleanup(&mb);
  }

  printf("\nAll search tests passed\n");
  return 0;
}