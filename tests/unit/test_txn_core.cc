#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/doc.h"
#include "pdfcore/status.h"
#include "pdfcore/transaction.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_INT_EQ(a, b)                                                                        \
  do {                                                                                             \
    if ((a) != (b)) {                                                                              \
      fprintf(stderr, "FAIL: %s:%d: %s == %s (%d != %d)\n", __FILE__, __LINE__, #a, #b, (a), (b)); \
      tests_failed++;                                                                              \
    } else {                                                                                       \
      tests_passed++;                                                                              \
    }                                                                                              \
  } while (0)

#define ASSERT_STR_EQ(a, b)                                                                     \
  do {                                                                                          \
    if (strcmp((a), (b)) != 0) {                                                                \
      fprintf(stderr, "FAIL: %s:%d: %s == %s (\"%s\" != \"%s\")\n", __FILE__, __LINE__, #a, #b, \
              (a), (b));                                                                        \
      tests_failed++;                                                                           \
    } else {                                                                                    \
      tests_passed++;                                                                           \
    }                                                                                           \
  } while (0)

#define ASSERT_STR_NE(a, b)                                                                     \
  do {                                                                                          \
    if (strcmp((a), (b)) == 0) {                                                                \
      fprintf(stderr, "FAIL: %s:%d: %s != %s (\"%s\" == \"%s\")\n", __FILE__, __LINE__, #a, #b, \
              (a), (b));                                                                        \
      tests_failed++;                                                                           \
    } else {                                                                                    \
      tests_passed++;                                                                           \
    }                                                                                           \
  } while (0)

static pc_status open_synthetic_doc(pc_doc** out) {
  // The null backend ignores the path and answers a 5-page document (see null_doc_open).
  return pc_doc_open("synthetic.pdf", nullptr, out);
}

// R18.3: "apply; undo; redo" leaves the IR byte-identical to "apply" alone.
// R18.1: commands and the hash are value-types only, no engine reach (R-M4/R-M8).
static void test_apply_undo_redo_hash_equality(void) {
  pc_doc* doc = nullptr;
  pc_status s = open_synthetic_doc(&doc);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  char h0[65] = {0};
  s = pc_doc_hash(doc, h0);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  pc_budget b = {sizeof(pc_budget), 100, 0};
  pc_txn* txn = nullptr;
  s = pc_txn_create(doc, &b, &txn);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  pc_command add = {sizeof(pc_command), PC_CMD_ADD_ANNOT, "abc2d3e4f5", {}, {10, 10, 20, 20}};
  s = pc_txn_apply(txn, &add);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  char h_apply[65] = {0};
  s = pc_doc_hash(doc, h_apply);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  ASSERT_STR_NE(h0, h_apply);

  s = pc_txn_undo(txn);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  char h_undo[65] = {0};
  s = pc_doc_hash(doc, h_undo);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  ASSERT_STR_EQ(h0, h_undo);

  s = pc_txn_redo(txn);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  char h_redo[65] = {0};
  s = pc_doc_hash(doc, h_redo);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  ASSERT_STR_EQ(h_apply, h_redo);

  pc_command mv = {sizeof(pc_command), PC_CMD_MOVE, "abc2d3e4f5", {}, {30, 30, 40, 40}};
  s = pc_txn_apply(txn, &mv);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  char h_move[65] = {0};
  s = pc_doc_hash(doc, h_move);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  ASSERT_STR_NE(h_apply, h_move);

  pc_command del = {sizeof(pc_command), PC_CMD_DELETE, "abc2d3e4f5", {}, {}};
  s = pc_txn_apply(txn, &del);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  char h_delete[65] = {0};
  s = pc_doc_hash(doc, h_delete);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  ASSERT_STR_NE(h_move, h_delete);

  // unwind all three commands, then replay them: state returns to anchor points.
  s = pc_txn_undo(txn);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  char h_rev1[65] = {0};
  pc_doc_hash(doc, h_rev1);
  ASSERT_STR_EQ(h_move, h_rev1);

  s = pc_txn_undo(txn);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  char h_rev2[65] = {0};
  pc_doc_hash(doc, h_rev2);
  ASSERT_STR_EQ(h_apply, h_rev2);

  s = pc_txn_undo(txn);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  char h_rev3[65] = {0};
  pc_doc_hash(doc, h_rev3);
  ASSERT_STR_EQ(h0, h_rev3);

  s = pc_txn_redo(txn);
  s = pc_txn_redo(txn);
  s = pc_txn_redo(txn);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  char h_replay[65] = {0};
  pc_doc_hash(doc, h_replay);
  ASSERT_STR_EQ(h_delete, h_replay);

  pc_txn_free(txn);
  pc_doc_close(doc);
}

// R18.4: budget gates undo. Tightening the ceiling fails an apply with PC_ERR_LIMIT and leaves
// the IR untouched; raising the ceiling lets the same commands through.
static void test_budget_ceiling(void) {
  pc_doc* doc = nullptr;
  pc_status s = open_synthetic_doc(&doc);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  pc_budget tight = {sizeof(pc_budget), 1, 0};
  pc_txn* txn = nullptr;
  s = pc_txn_create(doc, &tight, &txn);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  pc_command add = {sizeof(pc_command), PC_CMD_ADD_ANNOT, "abc2d3e4f5", {}, {10, 10, 20, 20}};
  s = pc_txn_apply(txn, &add);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  char h_pre[65] = {0};
  pc_doc_hash(doc, h_pre);

  pc_command add2 = {sizeof(pc_command), PC_CMD_ADD_ANNOT, "m3n4p5q6r7", {}, {5, 5, 15, 15}};
  s = pc_txn_apply(txn, &add2);
  ASSERT_INT_EQ(s.code, PC_ERR_LIMIT);
  ASSERT_STR_EQ(s.detail, "undo budget exceeded");

  char h_post[65] = {0};
  pc_doc_hash(doc, h_post);
  ASSERT_STR_EQ(h_pre, h_post);

  pc_txn_free(txn);

  // byte ceiling: 1 byte never fits a command; 1 MB and two commands fit.
  pc_budget tiny = {sizeof(pc_budget), 0, 1};
  pc_txn* txnb = nullptr;
  s = pc_txn_create(doc, &tiny, &txnb);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  pc_command addb = {sizeof(pc_command), PC_CMD_ADD_ANNOT, "aaaa2222bb", {}, {8, 8, 18, 18}};
  s = pc_txn_apply(txnb, &addb);
  ASSERT_INT_EQ(s.code, PC_ERR_LIMIT);
  pc_txn_free(txnb);

  pc_budget roomy = {sizeof(pc_budget), 0, 1024 * 1024};
  pc_txn* txn2 = nullptr;
  s = pc_txn_create(doc, &roomy, &txn2);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  pc_command addc = {sizeof(pc_command), PC_CMD_ADD_ANNOT, "aaaa2222bb", {}, {8, 8, 18, 18}};
  s = pc_txn_apply(txn2, &addc);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  pc_command addd = {sizeof(pc_command), PC_CMD_ADD_ANNOT, "m3n4p5q6r7", {}, {5, 5, 15, 15}};
  s = pc_txn_apply(txn2, &addd);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  pc_txn_free(txn2);

  pc_doc_close(doc);
}

// R18.2: null arguments are PC_ERR_ARGUMENT; empty stacks are PC_ERR_STATE; a duplicate ADD,
// an unknown-id MOVE/DELETE and a malformed id are PC_ERR_ARGUMENT.
static void test_null_and_internal_errors(void) {
  pc_doc* doc = nullptr;
  pc_status s = open_synthetic_doc(&doc);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  char h[65] = {0};
  ASSERT_INT_EQ(pc_txn_create(nullptr, nullptr, nullptr).code, PC_ERR_ARGUMENT);
  ASSERT_INT_EQ(pc_txn_apply(nullptr, nullptr).code, PC_ERR_ARGUMENT);
  ASSERT_INT_EQ(pc_txn_undo(nullptr).code, PC_ERR_ARGUMENT);
  ASSERT_INT_EQ(pc_txn_redo(nullptr).code, PC_ERR_ARGUMENT);
  ASSERT_INT_EQ(pc_doc_hash(nullptr, h).code, PC_ERR_ARGUMENT);

  pc_budget b = {sizeof(pc_budget), 10, 0};
  pc_txn* txn = nullptr;
  s = pc_txn_create(doc, &b, &txn);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  pc_command bad = {sizeof(pc_command), PC_CMD_ADD_ANNOT, "NOTVALID!", {}, {1, 1, 2, 2}};
  ASSERT_INT_EQ(pc_txn_apply(txn, &bad).code, PC_ERR_ARGUMENT);
  ASSERT_INT_EQ(pc_txn_apply(txn, nullptr).code, PC_ERR_ARGUMENT);

  pc_command mv = {sizeof(pc_command), PC_CMD_MOVE, "abc2d3e4f5", {}, {5, 5, 6, 6}};
  ASSERT_INT_EQ(pc_txn_apply(txn, &mv).code, PC_ERR_ARGUMENT);
  pc_command del = {sizeof(pc_command), PC_CMD_DELETE, "abc2d3e4f5", {}, {}};
  ASSERT_INT_EQ(pc_txn_apply(txn, &del).code, PC_ERR_ARGUMENT);

  ASSERT_INT_EQ(pc_txn_undo(txn).code, PC_ERR_STATE);
  ASSERT_INT_EQ(pc_txn_redo(txn).code, PC_ERR_STATE);

  pc_command add = {sizeof(pc_command), PC_CMD_ADD_ANNOT, "abc2d3e4f5", {}, {1, 1, 2, 2}};
  ASSERT_INT_EQ(pc_txn_apply(txn, &add).code, PC_ERR_NONE);
  pc_command dup = {sizeof(pc_command), PC_CMD_ADD_ANNOT, "abc2d3e4f5", {}, {9, 9, 10, 10}};
  ASSERT_INT_EQ(pc_txn_apply(txn, &dup).code, PC_ERR_ARGUMENT);

  pc_txn_free(txn);
  pc_txn_free(nullptr);
  pc_doc_close(doc);
}

int main(void) {
  test_apply_undo_redo_hash_equality();
  test_budget_ceiling();
  test_null_and_internal_errors();

  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}