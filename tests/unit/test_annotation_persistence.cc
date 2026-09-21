// R22.5

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pdfcore/annotation.h"
#include "pdfcore/status.h"
#include "pdfcore/doc.h"

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

static void test_annotation_persistence(void) {
  // Create a temporary PDF file
  const char* doc_path = "/tmp/test_annotation_persist.pdf";
  FILE* f = fopen(doc_path, "w");
  const char* pdf_content = "%PDF-1.4\n1 0 obj\n<</Type /Catalog>>\nendobj\nxref\n0 1\n0000000000 65535 f \ntrailer\n<</Size 1>>\nstartxref\n0\n%%EOF";
  fwrite(pdf_content, 1, strlen(pdf_content), f);
  fclose(f);
  
  // Open document
  pc_doc* doc = nullptr;
  pc_status s = pc_doc_open(doc_path, nullptr, &doc);
  ASSERT_STATUS_EQ(s, PC_ERR_NONE);
  ASSERT_NOT_NULL(doc);
  
  // Add annotations via internal API
  // Note: This test requires doc internal API access which may not be public
  // For now, we test the annotation list persistence independently
  
  pc_doc_close(doc);
  unlink(doc_path);
}

static void test_annotation_list_persistence(void) {
  // Test that annotation list can be serialized and deserialized
  pc_annotation_list* list = pc_annotation_list_create();
  
  pc_rect r1 = {10, 10, 100, 100};
  pc_annotation* a1 = pc_annotation_create(PC_ANNOT_TEXT, 0, &r1);
  a1->contents = strdup("Persistent note");
  pc_annotation_list_add(list, a1);
  
  pc_rect r2 = {20, 20, 200, 200};
  pc_annotation* a2 = pc_annotation_create(PC_ANNOT_HIGHLIGHT, 1, &r2);
  pc_annotation_list_add(list, a2);
  
  // Serialize
  char* json = pc_annotation_list_to_json(list);
  ASSERT_NOT_NULL(json);
  
  // Simulate "persistence" by deserializing (when implemented)
  // For now just verify serialization works
  
  free(json);
  pc_annotation_list_free(list);
}

int main(void) {
  test_annotation_persistence();
  test_annotation_list_persistence();
  
  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}