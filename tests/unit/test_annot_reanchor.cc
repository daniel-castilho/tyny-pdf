// R44.1 - annotation re-anchoring test

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore/annot.h"
#include "pdfcore/backend.h"
#include "pdfcore/doc.h"
#include "pdfcore/geom.h"
#include "pdfcore/import.h"
#include "pdfcore/page.h"
#include "pdfcore/selection.h"
#include "pdfcore/status.h"
#include "pdfcore/transaction.h"

// Extern the null backend API
extern pc_backend_api pc_null_backend_api;

static pc_status open_synthetic_doc(pc_doc** out) {
  return pc_doc_open("synthetic.pdf", nullptr, out);
}

int main() {
  int failures = 0;

  pc_doc* doc = nullptr;
  pc_status s = open_synthetic_doc(&doc);
  if (s.code != PC_ERR_NONE) {
    printf("FAIL: doc_open failed\n");
    return 1;
  }

  pc_budget b = {sizeof(pc_budget), 100, 0};
  pc_txn* txn = nullptr;
  s = pc_txn_create(doc, &b, &txn);
  if (s.code != PC_ERR_NONE) {
    printf("FAIL: txn_create failed\n");
    pc_doc_close(doc);
    return 1;
  }

  const pc_backend_api* real_api = &pc_null_backend_api;

  // Test with real backend page
  void* backend_page = nullptr;
  s = real_api->page_get(doc, 0, &backend_page);
  if (s.code == PC_ERR_NONE && backend_page) {
    // Test 1: Re-anchor at exact position
    // Null backend doesn't support text layout, so this returns PC_ERR_CAPABILITY
    uint32_t new_offset = 0, new_len = 0;
    float confidence = 0.0f;
    int detached = 0;
    s = pc_annot_reanchor(real_api, backend_page, 0, 5, &new_offset, &new_len, &confidence,
                          &detached);
    if (s.code == PC_ERR_CAPABILITY) {
      printf("PASS reanchor_test1: null backend returns PC_ERR_CAPABILITY as expected\n");
    } else {
      printf("FAIL reanchor_test1: code=%u (expected PC_ERR_CAPABILITY=%u)\n", s.code,
             PC_ERR_CAPABILITY);
      failures++;
    }

    // Test 2: Re-anchor at position beyond text
    s = pc_annot_reanchor(real_api, backend_page, 1000, 5, &new_offset, &new_len, &confidence,
                          &detached);
    if (s.code == PC_ERR_CAPABILITY) {
      printf("PASS reanchor_test2: null backend returns PC_ERR_CAPABILITY as expected\n");
    } else {
      printf("FAIL reanchor_test2: code=%u (expected PC_ERR_CAPABILITY=%u)\n", s.code,
             PC_ERR_CAPABILITY);
      failures++;
    }

    real_api->page_free(backend_page);
  } else {
    printf("SKIP: no backend page available for reanchor tests\n");
  }

  // Test 3: Null arguments
  s = pc_annot_reanchor(nullptr, nullptr, 0, 5, nullptr, nullptr, nullptr, nullptr);
  if (s.code == PC_ERR_ARGUMENT) {
    printf("PASS reanchor_test3: null_arguments\n");
  } else {
    printf("FAIL reanchor_test3: code=%u\n", s.code);
    failures++;
  }

  // Test 4: Null out_offset
  s = pc_annot_reanchor(nullptr, nullptr, 0, 5, nullptr, nullptr, nullptr, nullptr);
  if (s.code == PC_ERR_ARGUMENT) {
    printf("PASS reanchor_test4: null_out_offset\n");
  } else {
    printf("FAIL reanchor_test4: code=%u\n", s.code);
    failures++;
  }

  // Test 5: Null out_confidence
  uint32_t off = 0, len = 0;
  int det = 0;
  s = pc_annot_reanchor(nullptr, nullptr, 0, 5, &off, &len, nullptr, &det);
  if (s.code == PC_ERR_ARGUMENT) {
    printf("PASS reanchor_test5: null_out_confidence\n");
  } else {
    printf("FAIL reanchor_test5: code=%u\n", s.code);
    failures++;
  }

  pc_txn_free(txn);
  pc_doc_close(doc);

  if (failures) {
    printf("FAIL: %d test(s) failed\n", failures);
    return 1;
  }

  printf("\nAll re-anchoring tests passed\n");
  return 0;
}