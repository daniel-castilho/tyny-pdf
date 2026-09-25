// R48.1 R48.2 R48.3 R48.4 R49.1 R49.2 (story 7.2) - fill + validate + undo through the
// transaction log, on a synthetic IR. Runs against the null backend only: core semantics
// never need MuPDF to make their point (AGENTS.md testing strategy).

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore.h"

static int failures = 0;

#define CHECK(cond, name)                            \
  do {                                               \
    if (cond) {                                      \
      printf("PASS %s\n", name);                     \
    } else {                                         \
      printf("FAIL %s (line %d)\n", name, __LINE__); \
      failures++;                                    \
    }                                                \
  } while (0)

static pc_status add_text_field(pc_doc* doc, const char* name, uint32_t max_len, uint32_t flags) {
  pc_form_field f = {};
  f.type = PC_FORM_FIELD_TEXT;
  f.name = const_cast<char*>(name);
  f.value = const_cast<char*>("");
  f.max_len = max_len;
  f.flags = flags;
  return pc_form_ir_add_field(doc, &f);
}

static pc_status add_checkbox_field(pc_doc* doc, const char* name, uint32_t flags) {
  pc_form_field f = {};
  f.type = PC_FORM_FIELD_CHECKBOX;
  f.name = const_cast<char*>(name);
  f.value = const_cast<char*>("Off");
  f.flags = flags;
  return pc_form_ir_add_field(doc, &f);
}

int main() {
  // R48.1: fill stores the value in the IR and pc_form_get_value reads it back.
  {
    pc_doc* doc = nullptr;
    pc_status s = pc_doc_open("synthetic.pdf", nullptr, &doc);
    CHECK(s.code == PC_ERR_NONE, "doc_open");
    s = add_text_field(doc, "Name", 50, 0);
    CHECK(s.code == PC_ERR_NONE, "ir_add_field");

    pc_budget b = {sizeof(pc_budget), 0, 0};
    pc_txn* txn = nullptr;
    s = pc_txn_create(doc, &b, &txn);
    CHECK(s.code == PC_ERR_NONE, "txn_create");

    s = pc_form_fill_field(txn, "Name", "Foo");
    CHECK(s.code == PC_ERR_NONE, "fill_ok");

    char value[64] = {};
    s = pc_form_get_value(doc, "Name", value, sizeof(value));
    CHECK(s.code == PC_ERR_NONE && strcmp(value, "Foo") == 0, "fill_stores_value");

    pc_txn_free(txn);
    pc_doc_close(doc);
  }

  // R48.3: undo restores the exact prior state (hash equality proves it byte-wise), redo
  // re-applies. This is the same log annotations use - one mechanism, not a parallel one.
  {
    pc_doc* doc = nullptr;
    pc_doc_open("synthetic.pdf", nullptr, &doc);
    add_text_field(doc, "Name", 50, 0);
    pc_budget b = {sizeof(pc_budget), 0, 0};
    pc_txn* txn = nullptr;
    pc_txn_create(doc, &b, &txn);

    char before[65] = {};
    pc_status s = pc_doc_hash(doc, before);
    CHECK(s.code == PC_ERR_NONE, "hash_before");

    pc_form_fill_field(txn, "Name", "Foo");
    char filled[65] = {};
    pc_doc_hash(doc, filled);
    CHECK(strcmp(before, filled) != 0, "fill_changes_hash");

    s = pc_txn_undo(txn);
    CHECK(s.code == PC_ERR_NONE, "undo_ok");
    char undone[65] = {};
    pc_doc_hash(doc, undone);
    CHECK(strcmp(before, undone) == 0, "undo_restores_exact_state");

    char value[64] = {};
    pc_form_get_value(doc, "Name", value, sizeof(value));
    CHECK(value[0] == '\0', "undo_restores_value");

    s = pc_txn_redo(txn);
    CHECK(s.code == PC_ERR_NONE, "redo_ok");
    pc_form_get_value(doc, "Name", value, sizeof(value));
    CHECK(strcmp(value, "Foo") == 0, "redo_reapplies_value");

    pc_txn_free(txn);
    pc_doc_close(doc);
  }

  // R48.3: validation - max_len over the limit answers PC_ERR_RANGE and leaves the IR
  // untouched.
  {
    pc_doc* doc = nullptr;
    pc_doc_open("synthetic.pdf", nullptr, &doc);
    add_text_field(doc, "Zip", 4, 0);
    pc_budget b = {sizeof(pc_budget), 0, 0};
    pc_txn* txn = nullptr;
    pc_txn_create(doc, &b, &txn);

    pc_status s = pc_form_fill_field(txn, "Zip", "Toolong");
    CHECK(s.code == PC_ERR_RANGE, "max_len_answers_range");

    char value[64] = {};
    pc_form_get_value(doc, "Zip", value, sizeof(value));
    CHECK(value[0] == '\0', "rejected_fill_leaves_ir_untouched");

    pc_txn_free(txn);
    pc_doc_close(doc);
  }

  // R48.3: checkbox accepts Yes/Off case-insensitively and rejects anything else with
  // PC_ERR_ARGUMENT.
  {
    pc_doc* doc = nullptr;
    pc_doc_open("synthetic.pdf", nullptr, &doc);
    add_checkbox_field(doc, "Subscribe", 0);
    pc_budget b = {sizeof(pc_budget), 0, 0};
    pc_txn* txn = nullptr;
    pc_txn_create(doc, &b, &txn);

    pc_status s = pc_form_fill_field(txn, "Subscribe", "yes");
    CHECK(s.code == PC_ERR_NONE, "checkbox_yes_ok");
    s = pc_form_fill_field(txn, "Subscribe", "OFF");
    CHECK(s.code == PC_ERR_NONE, "checkbox_off_ok");
    s = pc_form_fill_field(txn, "Subscribe", "Maybe");
    CHECK(s.code == PC_ERR_ARGUMENT, "checkbox_invalid_answers_argument");

    char value[64] = {};
    pc_form_get_value(doc, "Subscribe", value, sizeof(value));
    CHECK(strcmp(value, "OFF") == 0, "checkbox_reject_leaves_last_good_value");

    pc_txn_free(txn);
    pc_doc_close(doc);
  }

  // R48.3: readonly answers PC_ERR_STATE; an unknown field answers PC_ERR_ARGUMENT.
  {
    pc_doc* doc = nullptr;
    pc_doc_open("synthetic.pdf", nullptr, &doc);
    add_text_field(doc, "Locked", 0, PC_FFLAG_READONLY);
    pc_budget b = {sizeof(pc_budget), 0, 0};
    pc_txn* txn = nullptr;
    pc_txn_create(doc, &b, &txn);

    pc_status s = pc_form_fill_field(txn, "Locked", "Nope");
    CHECK(s.code == PC_ERR_STATE, "readonly_answers_state");
    s = pc_form_fill_field(txn, "Nope", "Foo");
    CHECK(s.code == PC_ERR_ARGUMENT, "unknown_field_answers_argument");

    pc_txn_free(txn);
    pc_doc_close(doc);
  }

  // R48.2: IR loading propagates the backend's status unchanged - the null backend does
  // not declare PC_CAP_FORMS, so PC_ERR_CAPABILITY leaves the IR untouched.
  {
    pc_doc* doc = nullptr;
    pc_doc_open("synthetic.pdf", nullptr, &doc);
    const pc_backend_api* api = pc_null_backend_get_api();
    void* backend_doc = nullptr;
    api->doc_open("synthetic.pdf", nullptr, &backend_doc);

    pc_status s = pc_form_ir_load_from_backend(doc, api, backend_doc);
    CHECK(s.code == PC_ERR_CAPABILITY, "null_backend_load_answers_capability");

    api->doc_close(backend_doc);
    pc_doc_close(doc);
  }

  // R48.4: FDF export from the IR is byte-stable for a given state and carries the
  // filled value; the header is the full FDF signature, not just the prefix.
  {
    pc_doc* doc = nullptr;
    pc_doc_open("synthetic.pdf", nullptr, &doc);
    add_text_field(doc, "Name", 50, 0);
    add_checkbox_field(doc, "Subscribe", 0);
    pc_budget b = {sizeof(pc_budget), 0, 0};
    pc_txn* txn = nullptr;
    pc_txn_create(doc, &b, &txn);
    pc_form_fill_field(txn, "Name", "Foo");

    pc_fdf fdf = {};
    pc_status s = pc_form_fdf_export_ir(doc, &fdf);
    CHECK(s.code == PC_ERR_NONE && fdf.size > 0, "ir_fdf_export_ok");
    CHECK(fdf.size >= 15 && memcmp(fdf.data, "%FDF-1.2\n%\xe2\xe3\xcf\xd3\n", 15) == 0,
          "ir_fdf_full_header");
    CHECK(strstr(fdf.data, "/T (Name)") && strstr(fdf.data, "/V (Foo)"),
          "ir_fdf_carries_filled_value");
    CHECK(strstr(fdf.data, "/T (Subscribe)") && strstr(fdf.data, "/V (Off)"),
          "ir_fdf_carries_unfilled_value");

    pc_fdf again = {};
    pc_form_fdf_export_ir(doc, &again);
    CHECK(again.size == fdf.size && memcmp(again.data, fdf.data, fdf.size) == 0,
          "ir_fdf_byte_stable");
    pc_fdf_free(&fdf);
    pc_fdf_free(&again);
    pc_txn_free(txn);
    pc_doc_close(doc);
  }

  // R48.3 + R29.2: a fill past the undo budget answers PC_ERR_LIMIT and the IR stays
  // untouched - the same ceiling annotations already honour.
  {
    pc_doc* doc = nullptr;
    pc_doc_open("synthetic.pdf", nullptr, &doc);
    add_text_field(doc, "Name", 50, 0);
    pc_budget b = {sizeof(pc_budget), 1, 0};
    pc_txn* txn = nullptr;
    pc_txn_create(doc, &b, &txn);

    pc_status s = pc_form_fill_field(txn, "Name", "One");
    CHECK(s.code == PC_ERR_NONE, "budget_first_fill_ok");
    s = pc_form_fill_field(txn, "Name", "Two");
    CHECK(s.code == PC_ERR_LIMIT, "budget_second_fill_answers_limit");

    char value[64] = {};
    pc_form_get_value(doc, "Name", value, sizeof(value));
    CHECK(strcmp(value, "One") == 0, "budget_reject_leaves_first_value");

    pc_txn_free(txn);
    pc_doc_close(doc);
  }

  if (failures) {
    printf("FAIL: %d test(s) failed\n", failures);
    return 1;
  }
  printf("All R48.x fill/undo tests passed\n");
  return 0;
}
