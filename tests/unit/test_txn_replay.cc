#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/doc.h"
#include "pdfcore/sha256.h"
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

#if defined(_WIN32)
static inline int get_exit_code(int status) {
  return status;
}
#else
#include <sys/wait.h>
static inline int get_exit_code(int status) {
  return WEXITSTATUS(status);
}
#endif

static pc_status open_synthetic_doc(pc_doc** out) {
  return pc_doc_open("synthetic.pdf", nullptr, out);
}

static void sha256_hex(const char* s, char out[65]) {
  pc_sha256 ctx;
  pc_sha256_init(&ctx);
  pc_sha256_update(&ctx, reinterpret_cast<const unsigned char*>(s), strlen(s));
  unsigned char digest[32];
  pc_sha256_final(&ctx, digest);
  for (int i = 0; i < 32; ++i) {
    sprintf(out + i * 2, "%02x", digest[i]);
  }
  out[64] = '\0';
}

static void make_id(char id[11], int iter, int i) {
  static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz234567";
  for (int k = 0; k < 10; ++k) {
    id[k] = alphabet[(iter * 11 + i * 5 + k * 3) % 32];
  }
  id[10] = '\0';
}

// R22.1 / R22.2 / R22.3: canonical serialize + parse round-trip of a 5-command log.
// R12.3: CLI `txn replay` is asserted in test_cli_replay_bytes.
static void test_round_trip(void) {
  pc_doc* doc = nullptr;
  pc_status s = open_synthetic_doc(&doc);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  pc_budget b = {sizeof(pc_budget), 100, 0};
  pc_txn* txn = nullptr;
  s = pc_txn_create(doc, &b, &txn);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  pc_command cmds[5] = {
      {sizeof(pc_command), PC_CMD_ADD_ANNOT, "abc2d3e4f5", {}, {10, 10, 20, 20}},
      {sizeof(pc_command), PC_CMD_MOVE, "abc2d3e4f5", {}, {30, 30, 40, 40}},
      {sizeof(pc_command), PC_CMD_ADD_ANNOT, "m3n4p5q6r7", {}, {5, 5, 15, 15}},
      {sizeof(pc_command), PC_CMD_DELETE, "m3n4p5q6r7", {}, {}},
      {sizeof(pc_command), PC_CMD_MOVE, "abc2d3e4f5", {}, {50, 50, 60, 60}},
  };

  for (int i = 0; i < 5; ++i) {
    s = pc_txn_apply(txn, &cmds[i]);
    ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  }

  char* json1 = nullptr;
  s = pc_txn_to_json(txn, &json1);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  pc_doc* doc2 = nullptr;
  s = open_synthetic_doc(&doc2);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  pc_txn* txn2 = nullptr;
  s = pc_txn_from_json(json1, doc2, &txn2);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);

  char* json2 = nullptr;
  s = pc_txn_to_json(txn2, &json2);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  ASSERT_STR_EQ(json1, json2);

  char h1[65] = {0}, h2[65] = {0};
  pc_doc_hash(doc, h1);
  pc_doc_hash(doc2, h2);
  ASSERT_STR_EQ(h1, h2);

  char d1[65] = {0}, d2[65] = {0};
  sha256_hex(json1, d1);
  sha256_hex(json2, d2);
  ASSERT_STR_EQ(d1, d2);

  // Redo 5 after undoing 5 on the replayed log must restore the same IR.
  for (int i = 0; i < 5; ++i) {
    s = pc_txn_undo(txn2);
    ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  }
  for (int i = 0; i < 5; ++i) {
    s = pc_txn_redo(txn2);
    ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  }
  char h3[65] = {0};
  pc_doc_hash(doc2, h3);
  ASSERT_STR_EQ(h1, h3);

  free(json1);
  free(json2);
  pc_txn_free(txn);
  pc_txn_free(txn2);
  pc_doc_close(doc);
  pc_doc_close(doc2);
}

static void test_unknown_keys(void) {
  const char* json =
      "{\n"
      "  \"budget\": {\n"
      "    \"max_bytes\": 0,\n"
      "    \"max_tiles\": 100\n"
      "  },\n"
      "  \"future_flag\": true,\n"
      "  \"redo\": [],\n"
      "  \"undo\": [\n"
      "    {\n"
      "      \"after\": {\n"
      "        \"x0\": 1.000,\n"
      "        \"x1\": 3.000,\n"
      "        \"y0\": 2.000,\n"
      "        \"y1\": 4.000\n"
      "      },\n"
      "      \"annotation_id\": \"abc2d3e4f5\",\n"
      "      \"before\": {\n"
      "        \"x0\": 0.000,\n"
      "        \"x1\": 0.000,\n"
      "        \"y0\": 0.000,\n"
      "        \"y1\": 0.000\n"
      "      },\n"
      "      \"note\": \"keep-me\",\n"
      "      \"type\": \"ADD_ANNOT\"\n"
      "    }\n"
      "  ]\n"
      "}\n";

  pc_doc* doc = nullptr;
  open_synthetic_doc(&doc);
  pc_txn* txn = nullptr;
  pc_status s = pc_txn_from_json(json, doc, &txn);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  char* out = nullptr;
  s = pc_txn_to_json(txn, &out);
  ASSERT_INT_EQ(s.code, PC_ERR_NONE);
  if (!strstr(out, "future_flag") || !strstr(out, "keep-me")) {
    fprintf(stderr, "FAIL: unknown keys not preserved:\n%s\n", out);
    tests_failed++;
  } else {
    tests_passed++;
  }
  free(out);
  pc_txn_free(txn);
  pc_doc_close(doc);
}

static void test_property_round_trip(void) {
  for (int iter = 0; iter < 20; ++iter) {
    pc_doc* doc = nullptr;
    pc_doc_open("synthetic.pdf", nullptr, &doc);

    pc_budget b = {sizeof(pc_budget), 100, 0};
    pc_txn* txn = nullptr;
    pc_txn_create(doc, &b, &txn);

    int n_cmds = 3 + (iter % 5);
    for (int i = 0; i < n_cmds; ++i) {
      char id[11];
      make_id(id, iter, i);
      pc_command c = {sizeof(pc_command),
                      PC_CMD_ADD_ANNOT,
                      "",
                      {},
                      {i * 10.0, i * 10.0, i * 10.0 + 5, i * 10.0 + 5}};
      memcpy(c.annotation_id, id, 11);
      pc_txn_apply(txn, &c);
      if (i % 2 == 0) {
        pc_command mv = {sizeof(pc_command),
                         PC_CMD_MOVE,
                         "",
                         {},
                         {i * 10.0 + 10, i * 10.0 + 10, i * 10.0 + 15, i * 10.0 + 15}};
        memcpy(mv.annotation_id, id, 11);
        pc_txn_apply(txn, &mv);
      }
    }

    // Leave some commands on redo so the log is not undo-only.
    if (iter % 3 == 0) {
      pc_txn_undo(txn);
    }

    char* json1 = nullptr;
    pc_txn_to_json(txn, &json1);
    pc_txn_free(txn);

    pc_doc* doc2 = nullptr;
    pc_doc_open("synthetic.pdf", nullptr, &doc2);
    pc_txn* txn2 = nullptr;
    pc_txn_from_json(json1, doc2, &txn2);
    char* json2 = nullptr;
    pc_txn_to_json(txn2, &json2);

    ASSERT_STR_EQ(json1, json2);

    free(json1);
    free(json2);
    pc_txn_free(txn2);
    pc_doc_close(doc);
    pc_doc_close(doc2);
  }
}

static void test_cli_replay_bytes(void) {
  const char* cli = getenv("CLI_BINARY");
  if (!cli) {
    fprintf(stderr, "FAIL: CLI_BINARY not set\n");
    tests_failed++;
    return;
  }

  pc_doc* doc = nullptr;
  pc_doc_open("synthetic.pdf", nullptr, &doc);
  pc_budget b = {sizeof(pc_budget), 100, 0};
  pc_txn* txn = nullptr;
  pc_txn_create(doc, &b, &txn);
  pc_command c = {sizeof(pc_command), PC_CMD_ADD_ANNOT, "abc2d3e4f5", {}, {1, 2, 3, 4}};
  pc_txn_apply(txn, &c);
  char* json_core = nullptr;
  pc_txn_to_json(txn, &json_core);
  pc_txn_free(txn);
  pc_doc_close(doc);

  const char* in_path = "/tmp/test_txn_replay_in.json";
  const char* out_path = "/tmp/test_txn_replay_out.json";
  FILE* in = fopen(in_path, "wb");
  fwrite(json_core, 1, strlen(json_core), in);
  fclose(in);

  char cmd[1024];
  snprintf(cmd, sizeof(cmd), "\"%s\" txn replay %s --out %s", cli, in_path, out_path);
  int rc = system(cmd);
  ASSERT_INT_EQ(get_exit_code(rc), 0);

  FILE* out = fopen(out_path, "rb");
  if (!out) {
    fprintf(stderr, "FAIL: CLI did not write output\n");
    tests_failed++;
    free(json_core);
    return;
  }
  fseek(out, 0, SEEK_END);
  long sz = ftell(out);
  fseek(out, 0, SEEK_SET);
  char* json_cli = static_cast<char*>(malloc(static_cast<size_t>(sz) + 1));
  fread(json_cli, 1, static_cast<size_t>(sz), out);
  json_cli[sz] = '\0';
  fclose(out);

  ASSERT_STR_EQ(json_core, json_cli);

  snprintf(cmd, sizeof(cmd), "\"%s\" txn replay", cli);
  rc = system(cmd);
  ASSERT_INT_EQ(get_exit_code(rc), 2);

  snprintf(cmd, sizeof(cmd), "\"%s\" txn replay /tmp/does-not-exist-txn.json --out %s", cli,
           out_path);
  rc = system(cmd);
  ASSERT_INT_EQ(get_exit_code(rc), 1);

  FILE* bad = fopen(in_path, "wb");
  fputs("{not json", bad);
  fclose(bad);
  snprintf(cmd, sizeof(cmd), "\"%s\" txn replay %s --out %s", cli, in_path, out_path);
  rc = system(cmd);
  ASSERT_INT_EQ(get_exit_code(rc), 1);

  free(json_core);
  free(json_cli);
  remove(in_path);
  remove(out_path);
}

int main(void) {
  test_round_trip();
  test_unknown_keys();
  test_property_round_trip();
  test_cli_replay_bytes();

  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}
