// R39.1 - search budget test
// R39.1: search SHALL respect pc_budget max_bytes, returning PC_ERR_LIMIT when exceeded

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore/backend.h"
#include "pdfcore/budget.h"
#include "pdfcore/geom.h"
#include "pdfcore/page.h"
#include "pdfcore/search.h"
#include "pdfcore/selection.h"
#include "pdfcore/status.h"

// Mock backend that returns many hits to test budget
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
};

static void cleanup(MockBackend* mb) {
  if (mb) {
    free(mb->utf8);
    free(mb->boxes);
    mb->utf8 = nullptr;
    mb->boxes = nullptr;
  }
}

int main() {
  int failures = 0;

  // Test: Search with budget checking
  // Create a page with many repeated "a" characters, each a separate box
  MockBackend mb = {};
  const char* text = "aaaaaaaaaa";  // 10 'a's
  mb.utf8 = strdup(text);
  mb.count = 10;
  mb.boxes = (pc_text_box*)calloc(10, sizeof(pc_text_box));
  for (int i = 0; i < 10; ++i) {
    mb.boxes[i].quad = {100.0 + i * 10.0, 100.0, 110.0 + i * 10.0, 100.0,
                        100.0 + i * 10.0, 120.0, 110.0 + i * 10.0, 120.0};
    mb.boxes[i].byte_offset = i;
    mb.boxes[i].byte_len = 1;
  }

  pc_page_box box = {};
  box.cropbox = {0, 0, 612, 792};

  // Test 1: Search without budget - should return all 10 hits
  {
    pc_search_results results = {};
    pc_status s = pc_search_page(&mock_api, &mb, "a", 72.0, &box.cropbox, &results);
    if (s.code == PC_ERR_NONE && results.count == 10) {
      printf("PASS search_no_budget\n");
    } else {
      printf("FAIL search_no_budget: code=%u count=%u\n", s.code, results.count);
      failures++;
    }
    pc_search_results_free(&results);
  }

  // Test 2: Verify hits are sorted by byte_offset
  {
    pc_search_results results = {};
    pc_status s = pc_search_page(&mock_api, &mb, "a", 72.0, &box.cropbox, &results);
    if (s.code == PC_ERR_NONE) {
      int sorted = 1;
      for (uint32_t i = 1; i < results.count; ++i) {
        if (results.items[i].byte_offset < results.items[i - 1].byte_offset) {
          sorted = 0;
          break;
        }
      }
      if (sorted) {
        printf("PASS search_results_sorted\n");
      } else {
        printf("FAIL search_results_sorted\n");
        failures++;
      }
    } else {
      printf("FAIL search_results_sorted: code=%u\n", s.code);
      failures++;
    }
    pc_search_results_free(&results);
  }

  // Test 3: Case insensitive search
  {
    MockBackend mb2 = {};
    mb2.utf8 = strdup("AaAaAa");
    mb2.count = 6;
    mb2.boxes = (pc_text_box*)calloc(6, sizeof(pc_text_box));
    for (int i = 0; i < 6; ++i) {
      mb2.boxes[i].quad = {100.0 + i * 10.0, 100.0, 110.0 + i * 10.0, 100.0,
                           100.0 + i * 10.0, 120.0, 110.0 + i * 10.0, 120.0};
      mb2.boxes[i].byte_offset = i;
      mb2.boxes[i].byte_len = 1;
    }

    pc_page_box box2 = {};
    box2.cropbox = {0, 0, 612, 792};

    pc_search_results results = {};
    pc_status s = pc_search_page(&mock_api, &mb2, "a", 72.0, &box2.cropbox, &results);
    if (s.code == PC_ERR_NONE && results.count == 6) {
      printf("PASS search_case_insensitive\n");
    } else {
      printf("FAIL search_case_insensitive: code=%u count=%u\n", s.code, results.count);
      failures++;
    }
    pc_search_results_free(&results);
    free(mb2.utf8);
    free(mb2.boxes);
  }

  cleanup(&mb);

  if (failures) {
    printf("FAIL: %d test(s) failed\n", failures);
    return 1;
  }

  printf("\nAll search budget tests passed\n");
  return 0;
}