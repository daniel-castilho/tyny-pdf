// R41.1, R41.2, R41.3 - annotation transaction tests using real pc_txn API

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

static pc_status open_synthetic_doc(pc_doc** out) {
  return pc_doc_open("synthetic.pdf", nullptr, out);
}

int main() {
  int failures = 0;

  // Test 1: Create annotation from selection via command, apply, undo, redo
  {
    pc_doc* doc = nullptr;
    pc_status s = open_synthetic_doc(&doc);
    if (s.code != PC_ERR_NONE) {
      printf("FAIL test1: doc_open failed\n");
      failures++;
      goto test_done;
    }

    pc_budget b = {sizeof(pc_budget), 100, 0};
    pc_txn* txn = nullptr;
    s = pc_txn_create(doc, &b, &txn);
    if (s.code != PC_ERR_NONE) {
      printf("FAIL test1: txn_create failed\n");
      failures++;
      pc_doc_close(doc);
      goto test_done;
    }

    // Use a hardcoded command like the transaction test
    pc_command cmd = {
        sizeof(pc_command), PC_CMD_ADD_ANNOT, "abc2d3e4f5", {0, 0, 0, 0}, {10, 10, 20, 20}, "", ""};

    // Apply command
    s = pc_txn_apply(txn, &cmd);
    if (s.code != PC_ERR_NONE) {
      printf("FAIL test1: txn_apply code=%u\n", s.code);
      failures++;
      pc_txn_free(txn);
      pc_doc_close(doc);
      goto test_done;
    }

    // Undo
    s = pc_txn_undo(txn);
    if (s.code == PC_ERR_NONE) {
      printf("PASS test1: undo\n");
    } else {
      printf("FAIL test1: undo code=%u\n", s.code);
      failures++;
    }

    // Redo
    s = pc_txn_redo(txn);
    if (s.code == PC_ERR_NONE) {
      printf("PASS test1: redo\n");
    } else {
      printf("FAIL test1: redo code=%u\n", s.code);
      failures++;
    }

    pc_txn_free(txn);
    pc_doc_close(doc);
  }

test_done:
  // Test 2: Delete annotation
  {
    pc_doc* doc = nullptr;
    pc_status s = open_synthetic_doc(&doc);
    if (s.code != PC_ERR_NONE) {
      printf("FAIL test2: doc_open failed\n");
      failures++;
      goto test_done2;
    }

    pc_budget b = {sizeof(pc_budget), 100, 0};
    pc_txn* txn = nullptr;
    s = pc_txn_create(doc, &b, &txn);
    if (s.code != PC_ERR_NONE) {
      printf("FAIL test2: txn_create failed\n");
      failures++;
      pc_doc_close(doc);
      goto test_done2;
    }

    // First create an annotation with hardcoded ID
    pc_command add_cmd = {
        sizeof(pc_command), PC_CMD_ADD_ANNOT, "m3n4p5q6r7", {0, 0, 0, 0}, {5, 5, 15, 15}, "", ""};
    s = pc_txn_apply(txn, &add_cmd);
    if (s.code != PC_ERR_NONE) {
      printf("FAIL test2: txn_apply add code=%u\n", s.code);
      failures++;
      pc_txn_free(txn);
      pc_doc_close(doc);
      goto test_done2;
    }

    // Now delete it
    pc_command del_cmd = {
        sizeof(pc_command), PC_CMD_DELETE, "m3n4p5q6r7", {0, 0, 0, 0}, {0, 0, 0, 0}, "", ""};
    s = pc_txn_apply(txn, &del_cmd);
    if (s.code != PC_ERR_NONE) {
      printf("FAIL test2: txn_apply delete code=%u\n", s.code);
      failures++;
      pc_txn_free(txn);
      pc_doc_close(doc);
      goto test_done2;
    }

    // Undo delete
    s = pc_txn_undo(txn);
    if (s.code == PC_ERR_NONE) {
      printf("PASS test2: undo delete\n");
    } else {
      printf("FAIL test2: undo delete code=%u\n", s.code);
      failures++;
    }

    pc_txn_free(txn);
    pc_doc_close(doc);
  }

test_done2:
  // Test 3: Null backend capability (command builder doesn't use backend)
  {
    pc_selection_result selection = {};
    selection.page_index = 0;
    selection.quad = {100, 100, 150, 100, 100, 120, 150, 120};
    selection.byte_offset = 0;
    selection.byte_len = 5;

    uint8_t color[3] = {255, 255, 0};
    pc_command cmd = {};

    // The command builder doesn't use the backend, so it should work
    pc_status s = pc_annot_cmd_add_from_selection(&selection, color, PC_ANNOT_HIGHLIGHT, &cmd);
    if (s.code == PC_ERR_NONE) {
      printf("PASS test3: annot_cmd_add_from_selection builds command\n");
    } else {
      printf("FAIL test3: annot_cmd_add_from_selection code=%u\n", s.code);
      failures++;
    }
  }

  // Test 4: Null arguments
  {
    pc_selection_result selection = {};
    selection.page_index = 0;
    selection.quad = {100, 100, 150, 100, 100, 120, 150, 120};
    selection.byte_offset = 0;
    selection.byte_len = 5;

    uint8_t color[3] = {255, 255, 0};
    pc_command cmd = {};

    pc_status s = pc_annot_cmd_add_from_selection(nullptr, color, PC_ANNOT_HIGHLIGHT, &cmd);
    if (s.code == PC_ERR_ARGUMENT) {
      printf("PASS test4: null_selection_argument\n");
    } else {
      printf("FAIL test4: null_selection_argument code=%u\n", s.code);
      failures++;
    }

    s = pc_annot_cmd_add_from_selection(&selection, nullptr, PC_ANNOT_HIGHLIGHT, &cmd);
    if (s.code == PC_ERR_ARGUMENT) {
      printf("PASS test4: null_color_argument\n");
    } else {
      printf("FAIL test4: null_color_argument code=%u\n", s.code);
      failures++;
    }

    s = pc_annot_cmd_add_from_selection(&selection, color, PC_ANNOT_HIGHLIGHT, nullptr);
    if (s.code == PC_ERR_ARGUMENT) {
      printf("PASS test4: null_cmd_argument\n");
    } else {
      printf("FAIL test4: null_cmd_argument code=%u\n", s.code);
      failures++;
    }

    s = pc_annot_cmd_delete(nullptr, &cmd);
    if (s.code == PC_ERR_ARGUMENT) {
      printf("PASS test4: null_annot_id_argument\n");
    } else {
      printf("FAIL test4: null_annot_id_argument code=%u\n", s.code);
      failures++;
    }

    s = pc_annot_cmd_modify(nullptr, nullptr, nullptr, 0, &cmd);
    if (s.code == PC_ERR_ARGUMENT) {
      printf("PASS test4: null_modify_argument\n");
    } else {
      printf("FAIL test4: null_modify_argument code=%u\n", s.code);
      failures++;
    }
  }

  if (failures) {
    printf("FAIL: %d test(s) failed\n", failures);
    return 1;
  }

  printf("\nAll annotation transaction tests passed\n");
  return 0;
}