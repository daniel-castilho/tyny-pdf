// R50.1 R50.2 R50.3 R51.1 R51.2 (story 7.3) - core focus model, keyboard actions and
// the diffable a11y announcements. Synthetic IR, null backend only (AGENTS.md testing
// strategy: core semantics never need MuPDF to make their point).

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

static pc_status add_text_field(pc_doc* doc, const char* name, uint32_t max_len) {
  pc_form_field f = {};
  f.type = PC_FORM_FIELD_TEXT;
  f.name = const_cast<char*>(name);
  f.value = const_cast<char*>("");
  f.max_len = max_len;
  return pc_form_ir_add_field(doc, &f);
}

static pc_status add_checkbox_field(pc_doc* doc, const char* name) {
  pc_form_field f = {};
  f.type = PC_FORM_FIELD_CHECKBOX;
  f.name = const_cast<char*>(name);
  f.value = const_cast<char*>("Off");
  return pc_form_ir_add_field(doc, &f);
}

int main() {
  pc_doc* doc = nullptr;
  pc_status s = pc_doc_open("synthetic.pdf", nullptr, &doc);
  CHECK(s.code == PC_ERR_NONE, "doc_open");
  add_text_field(doc, "Name", 10);
  add_text_field(doc, "Email", 0);
  add_checkbox_field(doc, "Subscribe");

  pc_budget b = {sizeof(pc_budget), 0, 0};
  pc_txn* txn = nullptr;
  s = pc_txn_create(doc, &b, &txn);
  CHECK(s.code == PC_ERR_NONE, "txn_create");

  // R50.1: focus starts nowhere; announcements and commit answer PC_ERR_STATE.
  pc_form_focus focus;
  s = pc_form_focus_init(&focus);
  CHECK(s.code == PC_ERR_NONE && focus.index == -1, "focus_init_none");
  char name[64] = {};
  s = pc_form_focus_field(doc, &focus, name, sizeof(name));
  CHECK(s.code == PC_ERR_STATE, "no_focus_answers_state");
  s = pc_form_focus_announce(doc, &focus, name, sizeof(name));
  CHECK(s.code == PC_ERR_STATE, "no_focus_announce_state");
  s = pc_form_focus_commit(doc, txn, &focus);
  CHECK(s.code == PC_ERR_STATE, "no_focus_commit_state");

  // R50.1: Tab lands on the first field, wraps at the end; Shift+Tab wraps backwards.
  s = pc_form_focus_next(doc, &focus);
  CHECK(s.code == PC_ERR_NONE, "focus_next_ok");
  pc_form_focus_field(doc, &focus, name, sizeof(name));
  CHECK(strcmp(name, "Name") == 0, "focus_starts_first_field");
  pc_form_focus_next(doc, &focus);
  pc_form_focus_next(doc, &focus);
  pc_form_focus_next(doc, &focus);
  pc_form_focus_field(doc, &focus, name, sizeof(name));
  CHECK(strcmp(name, "Name") == 0, "focus_wraps_forward");
  pc_form_focus_prev(doc, &focus);
  pc_form_focus_field(doc, &focus, name, sizeof(name));
  CHECK(strcmp(name, "Subscribe") == 0, "focus_prev_wraps_backward");

  // R50.2: typing appends to the buffer; max_len rejects with PC_ERR_LIMIT and keeps
  // the previous content.
  pc_form_focus_init(&focus);
  pc_form_focus_next(doc, &focus);  // Name, max_len 10
  s = pc_form_focus_type(doc, &focus, "Foo", 3);
  CHECK(s.code == PC_ERR_NONE, "type_ok");
  s = pc_form_focus_type(doc, &focus, "BarExtra!", 10);  // 3 + 10 > 10
  CHECK(s.code == PC_ERR_LIMIT, "type_over_max_len_limit");
  CHECK(strcmp(focus.text, "Foo") == 0, "rejected_type_keeps_buffer");
  s = pc_form_focus_type(doc, &focus, "Bar", 3);
  CHECK(s.code == PC_ERR_NONE && strcmp(focus.text, "FooBar") == 0, "type_appends");

  // R50.2: commit applies the buffer as ONE PC_CMD_FORM_SET - undo (field-granular)
  // restores the previous value.
  s = pc_form_focus_commit(doc, txn, &focus);
  CHECK(s.code == PC_ERR_NONE, "commit_ok");
  char value[64] = {};
  pc_form_get_value(doc, "Name", value, sizeof(value));
  CHECK(strcmp(value, "FooBar") == 0, "commit_stores_value");
  s = pc_txn_undo(txn);
  CHECK(s.code == PC_ERR_NONE, "commit_undo_ok");
  pc_form_get_value(doc, "Name", value, sizeof(value));
  CHECK(value[0] == '\0', "commit_undo_restores_empty");
  pc_form_focus_load(doc, &focus);
  CHECK(focus.text[0] == '\0', "load_resyncs_buffer");

  // R50.2: SPACE toggles a checkbox both ways as one command; on a text field it
  // types one space.
  pc_form_focus_init(&focus);
  pc_form_focus_next(doc, &focus);
  pc_form_focus_next(doc, &focus);
  pc_form_focus_next(doc, &focus);  // Subscribe
  s = pc_form_toggle(doc, txn, &focus);
  CHECK(s.code == PC_ERR_NONE, "toggle_on_ok");
  pc_form_get_value(doc, "Subscribe", value, sizeof(value));
  CHECK(strcmp(value, "Yes") == 0, "toggle_off_to_yes");
  s = pc_form_toggle(doc, txn, &focus);
  CHECK(s.code == PC_ERR_NONE, "toggle_off_ok");
  pc_form_get_value(doc, "Subscribe", value, sizeof(value));
  CHECK(strcmp(value, "Off") == 0, "toggle_yes_to_off");
  s = pc_txn_undo(txn);  // undo the Off toggle
  pc_form_get_value(doc, "Subscribe", value, sizeof(value));
  CHECK(strcmp(value, "Yes") == 0, "toggle_undo_restores");

  pc_form_focus_init(&focus);
  pc_form_focus_next(doc, &focus);  // Name (text)
  s = pc_form_toggle(doc, txn, &focus);
  CHECK(s.code == PC_ERR_NONE, "space_on_text_types");
  CHECK(strcmp(focus.text, " ") == 0, "space_typed_into_buffer");

  // R50.3: the four announcement shapes, exact text - docs/a11y/forms.md quotes these
  // strings verbatim; changing one is a script change, not a refactor.
  // Subscribe is "Yes" here: the toggle block above ended with pc_txn_undo, which
  // restored the pre-toggle value.
  char out[128] = {};
  pc_form_focus_init(&focus);
  pc_form_focus_next(doc, &focus);  // Name, empty buffer
  s = pc_form_focus_announce(doc, &focus, out, sizeof(out));
  CHECK(s.code == PC_ERR_NONE && strcmp(out, "Name, edit, empty") == 0, "announce_text_empty");
  pc_form_focus_type(doc, &focus, "Foo", 3);
  pc_form_focus_announce(doc, &focus, out, sizeof(out));
  CHECK(strcmp(out, "Name, edit, value Foo") == 0, "announce_text_value");
  pc_form_focus_next(doc, &focus);
  pc_form_focus_next(doc, &focus);  // Subscribe = Yes (restored by undo above)
  pc_form_focus_announce(doc, &focus, out, sizeof(out));
  CHECK(strcmp(out, "Subscribe, checkbox, checked") == 0, "announce_checkbox_on");
  pc_form_toggle(doc, txn, &focus);  // Yes -> Off
  pc_form_focus_announce(doc, &focus, out, sizeof(out));
  CHECK(strcmp(out, "Subscribe, checkbox, not checked") == 0, "announce_checkbox_off");

  // R50.1: a document with no fields answers PC_ERR_STATE on Tab, not a crash.
  {
    pc_doc* empty = nullptr;
    pc_doc_open("empty.pdf", nullptr, &empty);
    pc_form_focus ef;
    pc_form_focus_init(&ef);
    s = pc_form_focus_next(empty, &ef);
    CHECK(s.code == PC_ERR_STATE, "no_fields_tab_state");
    pc_doc_close(empty);
  }

  pc_txn_free(txn);
  pc_doc_close(doc);

  if (failures) {
    printf("FAIL: %d test(s) failed\n", failures);
    return 1;
  }
  printf("All R50.x focus tests passed\n");
  return 0;
}
