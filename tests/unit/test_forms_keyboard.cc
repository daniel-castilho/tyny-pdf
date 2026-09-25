// R51.1 R51.2 R52.1 R52.2 (story 7.3) - viewer keyboard wiring for forms, headless:
// Tab/Shift+Tab cycle focus, WM_CHAR typing fills the buffer, Tab commits through the
// transaction log, Space toggles a checkbox, and the announcement (UTF-16) carries the
// exact strings docs/a11y/forms.md quotes. The viewer path is the one the Windows
// message loop drives; this test runs it on Linux (ADR-0011 R-M10).

#include <cstdio>
#include <cstring>
#include <cwchar>

#include "pdfcore.h"
#include "viewer/viewer.h"

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

static const char* kFixture = TEST_FIXTURE_DIR "/forms/field.pdf";

int main() {
  const pc_backend_api* api = pc_mupdf_backend_get_api();
  CHECK(api != nullptr, "mupdf_api_available");

  tynypdf::viewer::viewer_state st = {};
  pc_status s = tynypdf::viewer::viewer_open(&st, api, kFixture, 72, 4, 0);
  CHECK(s.code == PC_ERR_NONE, "viewer_open");

  // The fixture's four fields are in the IR (Name, Email, Subscribe, AgreeTerms).
  char value[64] = {};
  s = pc_form_get_value(st.ir, "Name", value, sizeof(value));
  CHECK(s.code == PC_ERR_NONE, "ir_has_fields");

  // R51.1: Tab lands on Name; typing buffers; Tab commits and moves to Email.
  s = tynypdf::viewer::viewer_on_key(&st, 0, 0x09 /*VK_TAB*/, 1, 0);
  CHECK(s.code == PC_ERR_NONE, "tab_first_field");
  char name[64] = {};
  pc_form_focus_field(st.ir, &st.form_focus, name, sizeof(name));
  CHECK(strcmp(name, "Name") == 0, "tab_lands_on_name");

  s = tynypdf::viewer::viewer_on_char(&st, 'F');
  s = tynypdf::viewer::viewer_on_char(&st, 'o');
  s = tynypdf::viewer::viewer_on_char(&st, 'o');
  CHECK(s.code == PC_ERR_NONE, "char_types_ok");
  CHECK(strcmp(st.form_focus.text, "Foo") == 0, "char_buffer_holds_foo");

  // R51.2: while typing, the announcement carries the buffer, not the stored value.
  wchar_t announce[PC_UIA_ANNOUNCE_MAX] = {};
  s = tynypdf::viewer::viewer_forms_announcement(&st, announce, PC_UIA_ANNOUNCE_MAX);
  CHECK(s.code == PC_ERR_NONE && wcscmp(announce, L"Name, edit, value Foo") == 0,
        "announce_while_typing");

  s = tynypdf::viewer::viewer_on_key(&st, 0, 0x09, 1, 0);  // Tab: commit + move
  pc_form_get_value(st.ir, "Name", value, sizeof(value));
  CHECK(strcmp(value, "Foo") == 0, "tab_commits_buffer");
  pc_form_focus_field(st.ir, &st.form_focus, name, sizeof(name));
  CHECK(strcmp(name, "Email") == 0, "tab_moves_to_email");

  // Shift+Tab moves backwards without losing the focus position.
  s = tynypdf::viewer::viewer_on_key(&st, 0, 0x09, 1, 1 /*Shift*/);
  pc_form_focus_field(st.ir, &st.form_focus, name, sizeof(name));
  CHECK(strcmp(name, "Name") == 0, "shift_tab_back_to_name");
  s = tynypdf::viewer::viewer_on_key(&st, 0, 0x09, 1, 0);  // Email
  s = tynypdf::viewer::viewer_on_key(&st, 0, 0x09, 1, 0);  // Subscribe
  pc_form_focus_field(st.ir, &st.form_focus, name, sizeof(name));
  CHECK(strcmp(name, "Subscribe") == 0, "tab_advances_to_subscribe");

  // R51.1: Space toggles the focused checkbox.
  s = tynypdf::viewer::viewer_on_key(&st, 0, 0x20 /*VK_SPACE*/, 1, 0);
  CHECK(s.code == PC_ERR_NONE, "space_toggles");
  pc_form_get_value(st.ir, "Subscribe", value, sizeof(value));
  CHECK(strcmp(value, "Yes") == 0, "checkbox_toggled_yes");
  s = tynypdf::viewer::viewer_forms_announcement(&st, announce, PC_UIA_ANNOUNCE_MAX);
  CHECK(s.code == PC_ERR_NONE && wcscmp(announce, L"Subscribe, checkbox, checked") == 0,
        "announce_checkbox_checked");

  // R52.2: the UIA state switches to the forms announcement and reverts on blur.
  pc_uia_state* uia = nullptr;
  s = pc_uia_state_create(&uia);
  CHECK(s.code == PC_ERR_NONE, "uia_state_create");
  pc_uia_state_set_forms_focus(uia, announce);
  wchar_t out[PC_UIA_ANNOUNCE_MAX] = {};
  s = pc_uia_state_announcement(uia, out, PC_UIA_ANNOUNCE_MAX);
  CHECK(s.code == PC_ERR_NONE && wcscmp(out, L"Subscribe, checkbox, checked") == 0,
        "uia_announces_forms_focus");
  pc_uia_state_set_forms_focus(uia, nullptr);
  s = pc_uia_state_announcement(uia, out, PC_UIA_ANNOUNCE_MAX);
  CHECK(s.code == PC_ERR_NONE && wcscmp(out, L"Page 1 of 1, zoom 100%") == 0,
        "uia_reverts_to_page_state");
  pc_uia_state_destroy(uia);

  // R51.1: Escape discards the buffer without committing.
  tynypdf::viewer::viewer_on_key(&st, 0, 0x09, 1, 0);  // to AgreeTerms
  tynypdf::viewer::viewer_on_key(&st, 0, 0x09, 1, 0);  // wrap to Name
  pc_form_focus_field(st.ir, &st.form_focus, name, sizeof(name));
  CHECK(strcmp(name, "Name") == 0, "tab_wraps_to_name");
  tynypdf::viewer::viewer_on_char(&st, 'X');
  tynypdf::viewer::viewer_on_char(&st, 'y');
  s = tynypdf::viewer::viewer_on_key(&st, 0, 0x1B /*VK_ESCAPE*/, 1, 0);
  CHECK(s.code == PC_ERR_NONE, "escape_ok");
  pc_form_get_value(st.ir, "Name", value, sizeof(value));
  CHECK(strcmp(value, "Foo") == 0, "escape_keeps_committed_value");
  CHECK(strcmp(st.form_focus.text, "Foo") == 0, "escape_resets_buffer_to_value");

  // Undo twice: the two checkbox toggles in this session (Yes then Off... only one
  // toggle here) - undo restores the pre-toggle value.
  s = pc_txn_undo(st.txn);
  CHECK(s.code == PC_ERR_NONE, "viewer_undo_ok");
  pc_form_get_value(st.ir, "Subscribe", value, sizeof(value));
  CHECK(strcmp(value, "Off") == 0, "viewer_undo_restores_checkbox");

  tynypdf::viewer::viewer_close(&st);

  if (failures) {
    printf("FAIL: %d test(s) failed\n", failures);
    return 1;
  }
  printf("All R51.x/R52.x viewer keyboard tests passed\n");
  return 0;
}
