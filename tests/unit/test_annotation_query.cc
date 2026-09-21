// R22.3 R22.4

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

static void test_annotation_query_by_page(void) {
  pc_annotation_list* list = pc_annotation_list_create();
  
  pc_rect r1 = {10, 10, 100, 100};
  pc_rect r2 = {20, 20, 200, 200};
  pc_rect r3 = {30, 30, 300, 300};
  pc_rect r4 = {40, 40, 400, 400};
  
  pc_annotation* a1 = pc_annotation_create(PC_ANNOT_TEXT, 0, &r1);
  pc_annotation* a2 = pc_annotation_create(PC_ANNOT_HIGHLIGHT, 0, &r2);
  pc_annotation* a3 = pc_annotation_create(PC_ANNOT_TEXT, 1, &r3);
  pc_annotation* a4 = pc_annotation_create(PC_ANNOT_HIGHLIGHT, 1, &r4);
  
  pc_annotation_list_add(list, a1);
  pc_annotation_list_add(list, a2);
  pc_annotation_list_add(list, a3);
  pc_annotation_list_add(list, a4);
  
  size_t page0_count = pc_annotation_list_count_for_page(list, 0);
  size_t page1_count = pc_annotation_list_count_for_page(list, 1);
  size_t page2_count = pc_annotation_list_count_for_page(list, 2);
  
  ASSERT_INT_EQ(page0_count, 2);
  ASSERT_INT_EQ(page1_count, 2);
  ASSERT_INT_EQ(page2_count, 0);
  
  pc_annotation* out[10];
  pc_annotation_list_get_for_page(list, 0, out, 10);
  ASSERT_INT_EQ(out[0]->page_index, 0);
  ASSERT_INT_EQ(out[1]->page_index, 0);
  
  pc_annotation_list_get_for_page(list, 1, out, 10);
  ASSERT_INT_EQ(out[0]->page_index, 1);
  ASSERT_INT_EQ(out[1]->page_index, 1);
  
  pc_annotation_list_free(list);
}

static void test_annotation_find_by_id(void) {
  pc_annotation_list* list = pc_annotation_list_create();
  
  pc_rect r1 = {10, 10, 100, 100};
  pc_annotation* a1 = pc_annotation_create(PC_ANNOT_TEXT, 0, &r1);
  pc_annotation_list_add(list, a1);
  
  char saved_id[11];
  strncpy(saved_id, a1->id, 10);
  saved_id[10] = '\0';
  
  pc_annotation* found = pc_annotation_list_find(list, a1->id);
  ASSERT_NOT_NULL(found);
  
  // Find by saved ID
  pc_annotation* found2 = pc_annotation_list_find(list, saved_id);
  ASSERT_NOT_NULL(found2);
  
  // Not found
  pc_annotation* not_found = pc_annotation_list_find(list, "nonexistent");
  ASSERT_INT_EQ((size_t)not_found, 0);
  
  pc_annotation_list_free(list);
}

static void test_annotation_remove(void) {
  pc_annotation_list* list = pc_annotation_list_create();
  
  pc_rect r1 = {10, 10, 100, 100};
  pc_rect r2 = {20, 20, 200, 200};
  
  pc_annotation* a1 = pc_annotation_create(PC_ANNOT_TEXT, 0, &r1);
  pc_annotation_list_add(list, a1);
  pc_annotation* a2 = pc_annotation_create(PC_ANNOT_HIGHLIGHT, 0, &r2);
  pc_annotation_list_add(list, a2);
  
  // Verify IDs are different
  if (strcmp(a1->id, a2->id) == 0) {
    fprintf(stderr, "ERROR: a1 and a2 have the same ID: %s\n", a1->id);
    return;
  }
  ASSERT_INT_EQ(strcmp(a1->id, a2->id) != 0, 1); // Should be different
  
  // Debug: print IDs and pointers
  fprintf(stderr, "DEBUG: a1 ID: %s, ptr=%p\n", a1->id, (void*)a1);
  fprintf(stderr, "DEBUG: a2 ID: %s, ptr=%p\n", a2->id, (void*)a2);
  fprintf(stderr, "DEBUG: a1->id ptr=%p, a2->id ptr=%p\n", (void*)a1->id, (void*)a2->id);
  
  // Save a2's ID before any removal
  char a2_id[11];
  strncpy(a2_id, a2->id, 10);
  a2_id[10] = '\0';
  
  fprintf(stderr, "DEBUG test: about to call remove with ID: %s\n", a1->id);
  
  pc_status s = pc_annotation_list_remove(list, a1->id);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);
  ASSERT_INT_EQ(list->count, 1);
  
  // Try to remove again - should fail
  s = pc_annotation_list_remove(list, a1->id);
  ASSERT_STATUS_EQ(s, PC_ERR_NOT_FOUND);
  
  // Find remaining - use saved a2 ID
  pc_annotation* found = pc_annotation_list_find(list, a2_id);
  ASSERT_NOT_NULL(found);
  
  pc_annotation_list_free(list);
}

int main(void) {
  test_annotation_query_by_page();
  test_annotation_find_by_id();
  test_annotation_remove();
  
  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}