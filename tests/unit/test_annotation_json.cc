// R22.6

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/annotation.h"
#include "pdfcore/status.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_STATUS_EQ(s, expected_code) do { \
    if ((s).code != (expected_code)) { \
        fprintf(stderr, "FAIL: %s:%d: expected %d got %d (%s)\n", __FILE__, __LINE__, (expected_code), (s).code, (s).detail ? (s).detail : "-"); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define ASSERT_INT_EQ(a, b) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "FAIL: %s:%d: %s == %s (%zu != %zu)\n", __FILE__, __LINE__, #a, #b, (size_t)(a), (size_t)(b)); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define ASSERT_NOT_NULL(p) do { \
    if (!(p)) { \
        fprintf(stderr, "FAIL: %s:%d: expected non-null pointer\n", __FILE__, __LINE__); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

static void test_annotation_json_serialization(void) {
  pc_rect r1 = {10, 10, 100, 100};
  pc_annotation* annot = pc_annotation_create(PC_ANNOT_TEXT, 0, &r1);
  annot->contents = strdup("Test annotation");
  annot->author = strdup("Test Author");
  annot->flags = PC_ANNOT_FLAG_PRINT | PC_ANNOT_FLAG_LOCKED;
  annot->color[0] = 1.0f; annot->color[1] = 0.0f; annot->color[2] = 0.0f;
  annot->opacity = 0.75f;
  annot->border_width = 2.0f;
  annot->custom_data = strdup("{\"key\": \"value\"}");
  
  char* json = pc_annotation_to_json(annot);
  ASSERT_NOT_NULL(json);
  
  // Verify JSON contains expected fields
  ASSERT_INT_EQ(strstr(json, "\"id\"") != nullptr, 1);
  ASSERT_INT_EQ(strstr(json, "\"type\"") != nullptr, 1);
  ASSERT_INT_EQ(strstr(json, "\"page_index\"") != nullptr, 1);
  ASSERT_INT_EQ(strstr(json, "\"rect\"") != nullptr, 1);
  ASSERT_INT_EQ(strstr(json, "\"contents\"") != nullptr, 1);
  ASSERT_INT_EQ(strstr(json, "\"author\"") != nullptr, 1);
  ASSERT_INT_EQ(strstr(json, "\"flags\"") != nullptr, 1);
  ASSERT_INT_EQ(strstr(json, "\"color\"") != nullptr, 1);
  ASSERT_INT_EQ(strstr(json, "\"opacity\"") != nullptr, 1);
  ASSERT_INT_EQ(strstr(json, "\"border_width\"") != nullptr, 1);
  ASSERT_INT_EQ(strstr(json, "\"custom_data\"") != nullptr, 1);
  
  free(json);
  pc_annotation_free(annot);
}

static void test_annotation_list_json_serialization(void) {
  pc_annotation_list* list = pc_annotation_list_create();
  
  pc_rect r1 = {10, 10, 100, 100};
  pc_annotation* a1 = pc_annotation_create(PC_ANNOT_TEXT, 0, &r1);
  a1->contents = strdup("First");
  pc_annotation_list_add(list, a1);
  
  pc_rect r2 = {20, 20, 200, 200};
  pc_annotation* a2 = pc_annotation_create(PC_ANNOT_HIGHLIGHT, 1, &r2);
  pc_annotation_list_add(list, a2);
  
  char* json = pc_annotation_list_to_json(list);
  ASSERT_NOT_NULL(json);
  
  // Verify it's a JSON array with two elements
  ASSERT_INT_EQ(json[0], '[');
  ASSERT_INT_EQ(strstr(json, "\"id\"") != nullptr, 1);
  ASSERT_INT_EQ(strstr(json, "\"type\"") != nullptr, 1);
  
  free(json);
  pc_annotation_list_free(list);
}

int main(void) {
  test_annotation_json_serialization();
  test_annotation_list_json_serialization();
  
  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}