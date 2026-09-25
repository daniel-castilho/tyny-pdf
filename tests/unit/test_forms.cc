// R46.1 R46.2 R46.3 R47.1 R48.1 R48.2 R48.4 R49.1 (stories 7.1/7.2) - AcroForm enumeration
// via the dict walk, FDF export, and the honest capability answers that remain. Links BOTH
// backends: mupdf enumerates the real fixture; the null backend is the R-M5 contract twin.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore.h"

// Get MuPDF backend API directly
extern "C" const pc_backend_api* pc_mupdf_backend_get_api(void);

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

  // R46.2 (abi 1.3): the dict walk enumerates the fixture's flat AcroForm - 2 text fields
  // (max_len 50/100) and 2 checkboxes, with their current values.
  {
    void* doc = nullptr;
    pc_status s = api->doc_open(kFixture, nullptr, &doc);
    CHECK(s.code == PC_ERR_NONE, "doc_open_fixture");

    pc_form_list list = {};
    s = api->form_list_fields(api, doc, &list);
    CHECK(s.code == PC_ERR_NONE, "list_fields_ok");
    CHECK(list.count == 4, "list_fields_counts_4");

    int text_count = 0;
    int checkbox_count = 0;
    bool has_name = false;
    bool has_email = false;
    bool has_subscribe = false;
    bool has_agree = false;
    uint32_t name_max_len = 0;
    for (uint32_t i = 0; i < list.count; ++i) {
      const pc_form_field* f = &list.items[i];
      if (f->type == PC_FORM_FIELD_TEXT) {
        text_count++;
      }
      if (f->type == PC_FORM_FIELD_CHECKBOX) {
        checkbox_count++;
      }
      if (f->name && strcmp(f->name, "Name") == 0) {
        has_name = true;
        name_max_len = f->max_len;
      }
      if (f->name && strcmp(f->name, "Email") == 0) {
        has_email = true;
      }
      if (f->name && strcmp(f->name, "Subscribe") == 0) {
        has_subscribe = true;
      }
      if (f->name && strcmp(f->name, "AgreeTerms") == 0) {
        has_agree = true;
      }
    }
    CHECK(text_count == 2 && checkbox_count == 2, "list_fields_2_text_2_checkbox");
    CHECK(has_name && has_email && has_subscribe && has_agree, "list_fields_names");
    CHECK(name_max_len == 50, "list_fields_name_max_len");

    api->form_list_free(&list);
    api->doc_close(doc);
  }

  // R48.2: the enumerated fields load into the IR through the vtable.
  {
    void* bdoc = nullptr;
    pc_status s = api->doc_open(kFixture, nullptr, &bdoc);
    CHECK(s.code == PC_ERR_NONE, "ir_load_doc_open");

    pc_doc* doc = nullptr;
    s = pc_doc_open(kFixture, nullptr, &doc);
    CHECK(s.code == PC_ERR_NONE, "ir_load_pc_doc_open");

    s = pc_form_ir_load_from_backend(doc, api, bdoc);
    CHECK(s.code == PC_ERR_NONE, "ir_load_from_backend_ok");

    char value[64] = {};
    s = pc_form_get_value(doc, "Name", value, sizeof(value));
    CHECK(s.code == PC_ERR_NONE && value[0] == '\0', "ir_load_name_empty");
    s = pc_form_get_value(doc, "Subscribe", value, sizeof(value));
    CHECK(s.code == PC_ERR_NONE && strcmp(value, "Off") == 0, "ir_load_subscribe_off");

    pc_fdf fdf = {};
    s = pc_form_fdf_export_ir(doc, &fdf);
    CHECK(s.code == PC_ERR_NONE && strstr(fdf.data, "/T (Name)"), "ir_fdf_has_name");
    pc_fdf_free(&fdf);

    pc_doc_close(doc);
    api->doc_close(bdoc);
  }

  // R46.1: backend FDF export carries the enumerated fields with the full FDF signature
  // header (the truncated 13-byte header this test replaces passed a prefix-only check).
  {
    void* doc = nullptr;
    api->doc_open(kFixture, nullptr, &doc);

    pc_fdf fdf = {};
    pc_status s = api->form_fdf_export(api, doc, &fdf);
    CHECK(s.code == PC_ERR_NONE && fdf.size >= 15, "fdf_export_ok");
    CHECK(memcmp(fdf.data, "%FDF-1.2\n%\xe2\xe3\xcf\xd3\n", 15) == 0, "fdf_export_full_header");
    CHECK(strstr(fdf.data, "/T (Name)") && strstr(fdf.data, "/T (Subscribe)"),
          "fdf_export_lists_fields");
    CHECK(strstr(fdf.data, "/V (Off)"), "fdf_export_carries_checkbox_value");
    pc_fdf_free(&fdf);
    api->doc_close(doc);
  }

  // R-M5: the null backend does not declare PC_CAP_FORMS - every forms entry point answers
  // PC_ERR_CAPABILITY there, never an empty list that a caller could mistake for a
  // formless document.
  {
    const pc_backend_api* null_api = pc_null_backend_get_api();
    void* doc = nullptr;
    pc_status s = null_api->doc_open(kFixture, nullptr, &doc);
    CHECK(s.code == PC_ERR_NONE, "null_doc_open");

    pc_form_list list = {};
    s = null_api->form_list_fields(null_api, doc, &list);
    CHECK(s.code == PC_ERR_CAPABILITY, "null_list_fields_capability");

    pc_fdf fdf = {};
    s = pc_form_fdf_export(null_api, doc, &fdf);
    CHECK(s.code == PC_ERR_CAPABILITY, "null_fdf_export_capability");
    s = null_api->form_fdf_import(null_api, doc, "x", 1);
    CHECK(s.code == PC_ERR_CAPABILITY, "null_fdf_import_capability");

    null_api->doc_close(doc);
  }

  // Deferred, tracked: FDF import writes /V back into the AcroForm tree and needs an
  // incremental-save story first - UNSUPPORTED, never a silent no-op.
  {
    void* doc = nullptr;
    api->doc_open(kFixture, nullptr, &doc);
    pc_status s = api->form_fdf_import(api, doc, "%FDF-1.2", 8);
    CHECK(s.code == PC_ERR_UNSUPPORTED, "fdf_import_unsupported");
    api->doc_close(doc);
  }

  if (failures) {
    printf("FAIL: %d test(s) failed\n", failures);
    return 1;
  }
  printf("All forms tests passed\n");
  return 0;
}
