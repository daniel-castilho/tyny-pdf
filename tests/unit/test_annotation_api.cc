// R22.1 R22.2 R22.3

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

static void test_annotation_list_create_free(void) {
  pc_annotation_list* list = pc_annotation_list_create();
  ASSERT_NOT_NULL(list);
  pc_annotation_list_free(list);
}

static void test_annotation_create(void) {
  pc_rect rect = {10, 10, 100, 100};
  pc_annotation* annot = pc_annotation_create(PC_ANNOT_HIGHLIGHT, 0, &rect);
  ASSERT_NOT_NULL(annot);
  ASSERT_INT_EQ(annot->type, PC_ANNOT_HIGHLIGHT);
  ASSERT_INT_EQ(annot->page_index, 0);
  ASSERT_INT_EQ(strlen(annot->id), 10); // RFC 4648 base32 ID
  pc_annotation_free(annot);
}

static void test_annotation_list_add_remove(void) {
  pc_annotation_list* list = pc_annotation_list_create();
  pc_rect rect = {10, 10, 100, 100};
  pc_annotation* annot = pc_annotation_create(PC_ANNOT_TEXT, 0, &rect);
  
  // Save ID before removal
  char saved_id[11];
  strncpy(saved_id, annot->id, 10);
  saved_id[10] = '\0';
  
  pc_status s = pc_annotation_list_add(list, annot);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);
  ASSERT_INT_EQ(list->count, 1);
  
  // Find by ID
  pc_annotation* found = pc_annotation_list_find(list, annot->id);
  ASSERT_NOT_NULL(found);
  ASSERT_INT_EQ(found, annot);
  
  // Remove
  s = pc_annotation_list_remove(list, annot->id);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);
  ASSERT_INT_EQ(list->count, 0);
  
  // Not found after removal - use saved ID
  pc_annotation* not_found = pc_annotation_list_find(list, saved_id);
  ASSERT_INT_EQ((size_t)not_found, 0);
  
  pc_annotation_list_free(list);
}

static void test_annotation_query_by_page(void) {
  pc_annotation_list* list = pc_annotation_list_create();
  
  pc_rect r1 = {10, 10, 100, 100};
  pc_rect r2 = {20, 20, 200, 200};
  pc_rect r3 = {30, 30, 300, 300};
  
  pc_annotation* a1 = pc_annotation_create(PC_ANNOT_TEXT, 0, &r1);
  pc_annotation* a2 = pc_annotation_create(PC_ANNOT_HIGHLIGHT, 0, &r2);
  pc_annotation* a3 = pc_annotation_create(PC_ANNOT_TEXT, 1, &r3);
  
  pc_annotation_list_add(list, a1);
  pc_annotation_list_add(list, a2);
  pc_annotation_list_add(list, a3);
  
  size_t page0_count = pc_annotation_list_count_for_page(list, 0);
  size_t page1_count = pc_annotation_list_count_for_page(list, 1);
  
  ASSERT_INT_EQ(page0_count, 2);
  ASSERT_INT_EQ(page1_count, 1);
  
  pc_annotation* out[10];
  pc_annotation_list_get_for_page(list, 0, out, 10);
  ASSERT_INT_EQ(out[0]->page_index, 0);
  ASSERT_INT_EQ(out[1]->page_index, 0);
  
  pc_annotation_list_free(list);
}

static void test_annotation_id_format(void) {
  pc_rect rect = {0,0,0,0};
  pc_annotation* annot = pc_annotation_create(PC_ANNOT_TEXT, 0, &rect);
  ASSERT_INT_EQ(strlen(annot->id), 10);
  
  // Check all chars are in base32 alphabet
  const char* base32 = "abcdefghijkmnopqrstuvwxyz234567";
  for (int i = 0; i < 10; ++i) {
    int found = 0;
    for (int j = 0; base32[j]; ++j) {
      if (base32[j] == annot->id[i]) { found = 1; break; }
    }
    ASSERT_INT_EQ(found, 1);
  }
  pc_annotation_free(annot);
}

int main(void) {
  test_annotation_list_create_free();
  test_annotation_create();
  test_annotation_list_add_remove();
  test_annotation_query_by_page();
  test_annotation_id_format();
  
  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}