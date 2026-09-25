// R53.1 R53.2 R53.3 (story 7.4) - flatten: bake widgets into static content, remove the
// interactive form, save a NEW file, log the IR clear as one PC_CMD_FORM_FLATTEN whose
// undo restores editability. The bake-equality claim is pixel-level: rendering the page
// with widgets before flatten equals rendering the baked page after (R53.2).

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore.h"
#include "pdfcore/sha256.h"

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
static const char* kAfter = "/tmp/tynypdf-flatten-test.pdf";

// SHA-256 of the rendered page's pixel bytes (rows via stride, no padding assumption).
static void pixmap_sha(const pc_pixmap* pix, char out[65]) {
  pc_sha256 ctx;
  pc_sha256_init(&ctx);
  for (uint32_t y = 0; y < pix->height; ++y) {
    pc_sha256_update(&ctx, pix->data + (size_t)y * pix->stride, (size_t)pix->width * 4);
  }
  unsigned char d[32];
  pc_sha256_final(&ctx, d);
  for (int i = 0; i < 32; ++i) {
    sprintf(out + i * 2, "%02x", d[i]);
  }
  out[64] = '\0';
}

// Render page 0 full-page at 72 dpi through the backend; widgets per the flag.
static pc_status render_page0(const pc_backend_api* api, void* backend_doc, int widgets,
                              pc_pixmap* pix, char sha_out[65]) {
  void* page = nullptr;
  pc_status s = api->page_get(backend_doc, 0, &page);
  if (s.code != PC_ERR_NONE) {
    return s;
  }
  pc_page_box box = {};
  s = api->page_get_box(backend_doc, 0, &box);
  if (s.code != PC_ERR_NONE) {
    api->page_free(page);
    return s;
  }
  pc_render_params params = {};
  params.dpi = 72;
  // Zero clip renders the full mediabox (the backend's documented default).
  params.clip = {0, 0, 0, 0};
  params.render_widgets = widgets;
  s = api->page_render(page, &params, pix);
  api->page_free(page);
  if (s.code == PC_ERR_NONE && sha_out) {
    pixmap_sha(pix, sha_out);
  }
  return s;
}

int main() {
  const pc_backend_api* mupdf = pc_mupdf_backend_get_api();
  const pc_backend_api* null_api = pc_null_backend_get_api();
  CHECK(mupdf && null_api, "backends_available");

  // R53.1: the null backend does not declare PC_CAP_FORMS - flatten answers
  // PC_ERR_CAPABILITY and nothing is logged or cleared.
  {
    pc_doc* doc = nullptr;
    pc_doc_open("synthetic.pdf", nullptr, &doc);
    pc_form_field f = {};
    f.type = PC_FORM_FIELD_TEXT;
    f.name = const_cast<char*>("Name");
    f.value = const_cast<char*>("");
    pc_form_ir_add_field(doc, &f);
    void* bdoc = nullptr;
    null_api->doc_open("synthetic.pdf", nullptr, &bdoc);
    pc_budget b = {sizeof(pc_budget), 0, 0};
    pc_txn* txn = nullptr;
    pc_txn_create(doc, &b, &txn);

    pc_status s = pc_form_flatten(null_api, bdoc, doc, txn, "/tmp/never.pdf");
    CHECK(s.code == PC_ERR_CAPABILITY, "null_flatten_capability");
    char value[64] = {};
    s = pc_form_get_value(doc, "Name", value, sizeof(value));
    CHECK(s.code == PC_ERR_NONE, "null_flatten_leaves_ir");

    null_api->doc_close(bdoc);
    pc_txn_free(txn);
    pc_doc_close(doc);
  }

  // R53.1 + R53.2: the real bake against the fixture.
  void* before_doc = nullptr;
  pc_status s = mupdf->doc_open(kFixture, nullptr, &before_doc);
  CHECK(s.code == PC_ERR_NONE, "before_open");

  // Render BEFORE: widgets drawn as interactive widgets (the pre-flatten half).
  pc_pixmap before_pix = {};
  char before_sha[65] = {};
  s = render_page0(mupdf, before_doc, 1, &before_pix, before_sha);
  CHECK(s.code == PC_ERR_NONE && before_pix.data, "before_render_widgets");
  mupdf->pixmap_free(&before_pix);

  // IR + txn for the command path.
  pc_doc* ir = nullptr;
  s = pc_doc_open(kFixture, nullptr, &ir);
  CHECK(s.code == PC_ERR_NONE, "ir_open");
  s = pc_form_ir_load_from_backend(ir, mupdf, before_doc);
  CHECK(s.code == PC_ERR_NONE, "ir_load_4_fields");
  pc_budget b = {sizeof(pc_budget), 0, 0};
  pc_txn* txn = nullptr;
  s = pc_txn_create(ir, &b, &txn);
  CHECK(s.code == PC_ERR_NONE, "txn_create");

  // Flatten: backend bakes + saves, then the IR clears as one command.
  s = pc_form_flatten(mupdf, before_doc, ir, txn, kAfter);
  CHECK(s.code == PC_ERR_NONE, "flatten_ok");

  // Every form entry point now answers "no fields".
  char value[64] = {};
  s = pc_form_get_value(ir, "Name", value, sizeof(value));
  CHECK(s.code == PC_ERR_ARGUMENT, "flattened_ir_has_no_fields");
  pc_form_focus focus;
  pc_form_focus_init(&focus);
  s = pc_form_focus_next(ir, &focus);
  CHECK(s.code == PC_ERR_STATE, "flattened_tab_answers_state");

  // Undo restores editability (R53.1) - the four fields and their values.
  s = pc_txn_undo(txn);
  CHECK(s.code == PC_ERR_NONE, "flatten_undo_ok");
  s = pc_form_get_value(ir, "Name", value, sizeof(value));
  CHECK(s.code == PC_ERR_NONE && value[0] == '\0', "undo_restores_name");
  s = pc_form_get_value(ir, "Subscribe", value, sizeof(value));
  CHECK(s.code == PC_ERR_NONE && strcmp(value, "Off") == 0, "undo_restores_subscribe");

  // Redo flattens the IR again.
  s = pc_txn_redo(txn);
  CHECK(s.code == PC_ERR_NONE, "flatten_redo_ok");
  s = pc_form_get_value(ir, "Name", value, sizeof(value));
  CHECK(s.code == PC_ERR_ARGUMENT, "redo_clears_fields");
  pc_txn_free(txn);

  // The saved file is really baked: reopening it yields no fields (honest bake), and
  // rendering it WITHOUT widgets equals the before-render WITH widgets (R53.2).
  void* after_doc = nullptr;
  s = mupdf->doc_open(kAfter, nullptr, &after_doc);
  CHECK(s.code == PC_ERR_NONE, "after_reopen");

  pc_form_list list = {};
  s = mupdf->form_list_fields(mupdf, after_doc, &list);
  CHECK(s.code == PC_ERR_NONE && list.count == 0, "after_has_no_fields");
  mupdf->form_list_free(&list);

  pc_pixmap after_pix = {};
  char after_sha[65] = {};
  s = render_page0(mupdf, after_doc, 0, &after_pix, after_sha);
  CHECK(s.code == PC_ERR_NONE && after_pix.data, "after_render_content");
  printf("  before sha: %s\n  after  sha: %s\n", before_sha, after_sha);
  CHECK(strcmp(before_sha, after_sha) == 0, "bake_pixel_identity");
  mupdf->pixmap_free(&after_pix);

  mupdf->doc_close(after_doc);
  mupdf->doc_close(before_doc);
  pc_doc_close(ir);

  // R53.1: the log round-trips - a replayed flatten carries the snapshot, so undo on
  // the replayed log restores the fields too.
  {
    pc_doc* doc = nullptr;
    pc_doc_open(kFixture, nullptr, &doc);
    void* bdoc = nullptr;
    mupdf->doc_open(kFixture, nullptr, &bdoc);
    pc_form_ir_load_from_backend(doc, mupdf, bdoc);
    pc_budget b2 = {sizeof(pc_budget), 0, 0};
    pc_txn* t = nullptr;
    pc_txn_create(doc, &b2, &t);
    pc_form_flatten(mupdf, bdoc, doc, t, "/tmp/tynypdf-flatten-test2.pdf");

    char* json = nullptr;
    pc_status s2 = pc_txn_to_json(t, &json);
    CHECK(s2.code == PC_ERR_NONE && strstr(json, "FORM_FLATTEN"), "flatten_in_json");
    CHECK(strstr(json, "flattened_fields") && strstr(json, "\"Subscribe\""), "snapshot_in_json");
    pc_txn_free(t);

    pc_txn* replayed = nullptr;
    s2 = pc_txn_from_json(json, doc, &replayed);
    CHECK(s2.code == PC_ERR_NONE, "replay_ok");
    free(json);
    s2 = pc_txn_undo(replayed);
    CHECK(s2.code == PC_ERR_NONE, "replay_undo_ok");
    char v[64] = {};
    s2 = pc_form_get_value(doc, "Subscribe", v, sizeof(v));
    CHECK(s2.code == PC_ERR_NONE && strcmp(v, "Off") == 0, "replay_undo_restores_fields");
    pc_txn_free(replayed);
    mupdf->doc_close(bdoc);
    pc_doc_close(doc);
  }

  if (failures) {
    printf("FAIL: %d test(s) failed\n", failures);
    return 1;
  }
  printf("All R53.x flatten tests passed\n");
  return 0;
}
