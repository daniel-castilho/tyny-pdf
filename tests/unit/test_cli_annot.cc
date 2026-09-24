// R42.1 - CLI annot subcommand test

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore/annot.h"
#include "pdfcore/backend.h"
#include "pdfcore/doc.h"
#include "pdfcore/geom.h"
#include "pdfcore/page.h"
#include "pdfcore/selection.h"
#include "pdfcore/status.h"
#include "pdfcore/transaction.h"

static pc_status open_synthetic_doc(pc_doc** out) {
  return pc_doc_open("synthetic.pdf", nullptr, out);
}

int main() {
  // Test that the annot.h header is usable and API exists
  pc_doc* doc = nullptr;
  pc_status s = open_synthetic_doc(&doc);
  if (s.code != PC_ERR_NONE) {
    printf("FAIL cli_annot: doc_open failed\n");
    return 1;
  }

  pc_budget b = {sizeof(pc_budget), 100, 0};
  pc_txn* txn = nullptr;
  s = pc_txn_create(doc, &b, &txn);
  if (s.code != PC_ERR_NONE) {
    printf("FAIL cli_annot: txn_create failed\n");
    pc_doc_close(doc);
    return 1;
  }

  pc_selection_result selection = {};
  selection.page_index = 0;
  selection.quad = {100, 100, 150, 100, 100, 120, 150, 120};
  selection.byte_offset = 0;
  selection.byte_len = 5;

  uint8_t color[3] = {255, 255, 0};
  pc_command cmd = {};

  s = pc_annot_cmd_add_from_selection(&selection, color, PC_ANNOT_HIGHLIGHT, &cmd);
  if (s.code == PC_ERR_NONE) {
    printf("PASS cli_annot_cmd_add_from_selection\n");
  } else {
    printf("FAIL cli_annot_cmd_add_from_selection: code=%u\n", s.code);
    pc_txn_free(txn);
    pc_doc_close(doc);
    return 1;
  }

  // Test annot_cmd_delete
  pc_command del_cmd = {};
  s = pc_annot_cmd_delete(cmd.annotation_id, &del_cmd);
  if (s.code == PC_ERR_NONE) {
    printf("PASS cli_annot_cmd_delete\n");
  } else {
    printf("FAIL cli_annot_cmd_delete: code=%u\n", s.code);
    pc_txn_free(txn);
    pc_doc_close(doc);
    return 1;
  }

  pc_txn_free(txn);
  pc_doc_close(doc);
  printf("All CLI annot tests passed\n");
  return 0;
}