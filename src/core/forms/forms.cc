// R46.1 R46.2 R46.3 (story 7.1) - AcroForm field model and FDF round-trip.
// R48.1-R48.4 (story 7.2) - fill with validation + undo through the transaction log.

#include "pdfcore/forms.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../doc/transaction.h"
#include "pdfcore/backend.h"
#include "pdfcore/status.h"

static pc_status ok_status() {
  pc_status s = {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
  return s;
}

static pc_status arg_err(const char* detail) {
  pc_status s = {sizeof(pc_status), PC_ERR_ARGUMENT, 0, detail};
  return s;
}

static pc_status cap_err(const char* detail) {
  pc_status s = {sizeof(pc_status), PC_ERR_CAPABILITY, 0, detail};
  return s;
}

static pc_status mem_err(const char* detail) {
  pc_status s = {sizeof(pc_status), PC_ERR_MEMORY, 0, detail};
  return s;
}

// Helper: append to a growable string buffer. The growth loop must account for the
// incoming length, not the current one - the previous `strlen(*buf)` variant could
// realloc short and let the memcpy below run past the allocation.
static int append_str(char** buf, size_t* capacity, size_t* len, const char* s) {
  size_t slen = strlen(s);
  if (*len + slen + 1 > *capacity) {
    size_t new_cap = (*capacity == 0) ? 256 : *capacity;
    while (*len + slen + 1 > new_cap) {
      new_cap *= 2;
    }
    char* new_buf = (char*)realloc(*buf, new_cap);
    if (!new_buf) {
      return 0;
    }
    *buf = new_buf;
    *capacity = new_cap;
  }
  memcpy(*buf + *len, s, slen);
  *len += slen;
  (*buf)[*len] = '\0';
  return 1;
}

pc_status pc_form_list_fields(const pc_backend_api* api, void* backend_doc,
                              pc_form_list* out_list) {
  if (!api || !backend_doc || !out_list) {
    return arg_err("null argument");
  }
  // Not implemented in core - backend must provide
  return cap_err("form listing not implemented in backend");
}

void pc_form_list_free(pc_form_list* list) {
  if (!list || !list->items)
    return;
  for (uint32_t i = 0; i < list->count; ++i) {
    free(list->items[i].name);
    free(list->items[i].value);
    free(list->items[i].default_value);
    for (uint32_t j = 0; j < list->items[i].options_count; ++j) {
      free(list->items[i].options[j]);
    }
    free(list->items[i].options);
    free(list->items[i].format);
  }
  free(list->items);
  list->items = nullptr;
  list->count = 0;
}

pc_status pc_form_fdf_export(const pc_backend_api* api, void* backend_doc, pc_fdf* out_fdf) {
  if (!api || !backend_doc || !out_fdf) {
    return arg_err("null argument");
  }
  // Delegate to backend if it supports it
  if (api->form_fdf_export) {
    return api->form_fdf_export(api, backend_doc, out_fdf);
  }
  return cap_err("FDF export not supported by backend");
}

pc_status pc_form_fdf_import(const pc_backend_api* api, void* backend_doc, const char* fdf_data,
                             size_t fdf_size) {
  if (!api || !backend_doc || !fdf_data) {
    return arg_err("null argument");
  }
  if (api->form_fdf_import) {
    return api->form_fdf_import(api, backend_doc, fdf_data, fdf_size);
  }
  return cap_err("FDF import not supported by backend");
}

void pc_fdf_free(pc_fdf* fdf) {
  if (fdf && fdf->data) {
    free(fdf->data);
    fdf->data = nullptr;
    fdf->size = 0;
  }
}

// Helper: case-insensitive string compare (portable, no POSIX strcasecmp in core)
static int stricmp_ascii(const char* a, const char* b) {
  if (!a || !b)
    return a != b;
  while (*a && *b) {
    int ca = tolower((unsigned char)*a);
    int cb = tolower((unsigned char)*b);
    if (ca != cb)
      return ca - cb;
    a++;
    b++;
  }
  return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

// R48.1 - add one form field to the IR. Grows the array by doubling; copies every string
// into the fixed IR buffers so the IR stays pure value types (R-M4).
pc_status pc_form_ir_add_field(pc_doc* doc, const pc_form_field* field) {
  if (!doc || !field || !field->name) {
    return arg_err("null argument");
  }
  if (doc->form_field_count == doc->form_field_cap) {
    uint32_t new_cap = doc->form_field_cap == 0 ? 4 : doc->form_field_cap * 2;
    IrFormField* grown =
        (IrFormField*)realloc(doc->form_fields, (size_t)new_cap * sizeof(IrFormField));
    if (!grown) {
      return mem_err("OOM growing form field IR");
    }
    doc->form_fields = grown;
    doc->form_field_cap = new_cap;
  }
  IrFormField* dst = &doc->form_fields[doc->form_field_count++];
  memset(dst, 0, sizeof(*dst));
  dst->type = (uint32_t)field->type;
  dst->page_index = field->page_index;
  dst->rect = field->rect;
  dst->max_len = field->max_len;
  dst->flags = field->flags;
  if (field->name) {
    strncpy(dst->name, field->name, sizeof(dst->name) - 1);
  }
  if (field->value) {
    strncpy(dst->value, field->value, sizeof(dst->value) - 1);
  }
  if (field->default_value) {
    strncpy(dst->default_value, field->default_value, sizeof(dst->default_value) - 1);
  }
  if (field->format) {
    strncpy(dst->format, field->format, sizeof(dst->format) - 1);
  }
  return ok_status();
}

// R48.2 - populate the IR through the backend vtable. Propagates the backend's status
// unchanged; a capability answer leaves the IR untouched (R-M5).
pc_status pc_form_ir_load_from_backend(pc_doc* doc, const pc_backend_api* api, void* backend_doc) {
  if (!doc || !api || !backend_doc) {
    return arg_err("null argument");
  }
  if (!api->form_list_fields || !api->form_list_free) {
    return cap_err("backend does not declare PC_CAP_FORMS");
  }
  pc_form_list list = {};
  pc_status s = api->form_list_fields(api, backend_doc, &list);
  if (s.code != PC_ERR_NONE) {
    return s;
  }
  for (uint32_t i = 0; i < list.count; ++i) {
    pc_status add = pc_form_ir_add_field(doc, &list.items[i]);
    if (add.code != PC_ERR_NONE) {
      api->form_list_free(&list);
      return add;
    }
  }
  api->form_list_free(&list);
  return ok_status();
}

// R48.3 - build the PC_CMD_FORM_SET command.
pc_status pc_form_cmd_set(const char* field_name, const char* new_value, pc_command* cmd) {
  if (!field_name || !new_value || !cmd) {
    return arg_err("null argument");
  }
  if (!field_name[0]) {
    return arg_err("empty field name");
  }
  memset(cmd, 0, sizeof(*cmd));
  cmd->size = sizeof(pc_command);
  cmd->type = PC_CMD_FORM_SET;
  strncpy(cmd->form_field_name, field_name, sizeof(cmd->form_field_name) - 1);
  strncpy(cmd->form_new_value, new_value, sizeof(cmd->form_new_value) - 1);
  return ok_status();
}

// Validate a fill against the IR's field constraints. Error codes are part of R48.3:
// RANGE for max_len, ARGUMENT for a bad checkbox value.
static pc_status validate_fill(const IrFormField* field, const char* new_value) {
  if (field->max_len > 0 && strlen(new_value) > field->max_len) {
    pc_status s = {sizeof(pc_status), PC_ERR_RANGE, 0, "value exceeds field max_len"};
    return s;
  }
  if (field->type == (uint32_t)PC_FORM_FIELD_CHECKBOX) {
    if (stricmp_ascii(new_value, "Yes") != 0 && stricmp_ascii(new_value, "Off") != 0) {
      return arg_err("checkbox value must be Yes or Off");
    }
  }
  return ok_status();
}

// R48.3 - fill with validation and undo. The old value is captured by pc_txn_apply at
// apply time, so undo restores it through the same log as annotations.
pc_status pc_form_fill_field(pc_txn* txn, const char* field_name, const char* new_value) {
  if (!txn || !field_name || !new_value) {
    return arg_err("null argument");
  }
  pc_doc* doc = txn->doc;
  IrFormField* field = ir_form_field_find(doc, field_name);
  if (!field) {
    return arg_err("field not found");
  }
  if (field->flags & PC_FFLAG_READONLY) {
    pc_status s = {sizeof(pc_status), PC_ERR_STATE, 0, "field is readonly"};
    return s;
  }
  pc_status s = validate_fill(field, new_value);
  if (s.code != PC_ERR_NONE) {
    return s;
  }
  pc_command cmd;
  s = pc_form_cmd_set(field_name, new_value, &cmd);
  if (s.code != PC_ERR_NONE) {
    return s;
  }
  return pc_txn_apply(txn, &cmd);
}

pc_status pc_form_get_value(const pc_doc* doc, const char* field_name, char* out_value,
                            size_t capacity) {
  if (!doc || !field_name || !out_value || capacity == 0) {
    return arg_err("null argument");
  }
  IrFormField* field = ir_form_field_find((pc_doc*)doc, field_name);
  if (!field) {
    return arg_err("field not found");
  }
  strncpy(out_value, field->value, capacity - 1);
  out_value[capacity - 1] = '\0';
  return ok_status();
}

// R48.4 - export the IR's form fields to FDF. Byte-stable for a given IR state so a
// fill is reproducible headless: same IR in, same FDF bytes out.
pc_status pc_form_fdf_export_ir(const pc_doc* doc, pc_fdf* out_fdf) {
  if (!doc || !out_fdf) {
    return arg_err("null argument");
  }
  char* buf = nullptr;
  size_t capacity = 0;
  size_t len = 0;
  if (!append_str(&buf, &capacity, &len, "%FDF-1.2\n%\xe2\xe3\xcf\xd3\n")) {
    free(buf);
    return mem_err("OOM building FDF");
  }
  if (!append_str(&buf, &capacity, &len, "1 0 obj\n<<\n/FDF\n<<\n/Fields [\n")) {
    free(buf);
    return mem_err("OOM building FDF");
  }
  for (uint32_t i = 0; i < doc->form_field_count; ++i) {
    const IrFormField* f = &doc->form_fields[i];
    if (!append_str(&buf, &capacity, &len, "<<\n/T (") ||
        !append_str(&buf, &capacity, &len, f->name) ||
        !append_str(&buf, &capacity, &len, ")\n/V (") ||
        !append_str(&buf, &capacity, &len, f->value) ||
        !append_str(&buf, &capacity, &len, ")\n>>\n")) {
      free(buf);
      return mem_err("OOM building FDF");
    }
  }
  if (!append_str(&buf, &capacity, &len,
                  "]\n>>\n>>\nendobj\ntrailer\n<<\n/Root 1 0 R\n>>\n%%EOF\n")) {
    free(buf);
    return mem_err("OOM building FDF");
  }
  out_fdf->data = buf;
  out_fdf->size = len;
  return ok_status();
}
// ================ Focus model + keyboard actions (R50.x, story 7.3) ================

// R50.1 - focus starts on no field; Tab lands on the first field.
pc_status pc_form_focus_init(pc_form_focus* focus) {
  if (!focus) {
    return arg_err("null argument");
  }
  focus->index = -1;
  focus->text[0] = '\0';
  return ok_status();
}

// R50.1 - next/prev cycle the IR field list in document order, wrapping around the
// ends. Moving to a text field loads its current value into the typing buffer; a
// checkbox clears it.
static pc_status focus_move(const pc_doc* doc, pc_form_focus* focus, int delta) {
  if (!doc || !focus) {
    return arg_err("null argument");
  }
  if (doc->form_field_count == 0) {
    pc_status s = {sizeof(pc_status), PC_ERR_STATE, 0, "document has no form fields"};
    return s;
  }
  if (focus->index < 0) {
    focus->index = delta > 0 ? 0 : (int32_t)doc->form_field_count - 1;
  } else {
    focus->index =
        (focus->index + delta + (int32_t)doc->form_field_count) % (int32_t)doc->form_field_count;
  }
  const IrFormField* f = &doc->form_fields[focus->index];
  if (f->type == (uint32_t)PC_FORM_FIELD_CHECKBOX) {
    focus->text[0] = '\0';
  } else {
    strncpy(focus->text, f->value, sizeof(focus->text) - 1);
    focus->text[sizeof(focus->text) - 1] = '\0';
  }
  return ok_status();
}

pc_status pc_form_focus_next(const pc_doc* doc, pc_form_focus* focus) {
  return focus_move(doc, focus, 1);
}

pc_status pc_form_focus_prev(const pc_doc* doc, pc_form_focus* focus) {
  return focus_move(doc, focus, -1);
}

// R50.1 - the focused field's name, or PC_ERR_STATE when none.
pc_status pc_form_focus_field(const pc_doc* doc, const pc_form_focus* focus, char* out_name,
                              size_t capacity) {
  if (!doc || !focus || !out_name || capacity == 0) {
    return arg_err("null argument");
  }
  if (focus->index < 0 || (uint32_t)focus->index >= doc->form_field_count) {
    pc_status s = {sizeof(pc_status), PC_ERR_STATE, 0, "no field focused"};
    return s;
  }
  strncpy(out_name, doc->form_fields[focus->index].name, capacity - 1);
  out_name[capacity - 1] = '\0';
  return ok_status();
}

// R50.2 - buffer := current value (focus enter, Escape).
pc_status pc_form_focus_load(const pc_doc* doc, pc_form_focus* focus) {
  if (!doc || !focus) {
    return arg_err("null argument");
  }
  if (focus->index < 0 || (uint32_t)focus->index >= doc->form_field_count) {
    pc_status s = {sizeof(pc_status), PC_ERR_STATE, 0, "no field focused"};
    return s;
  }
  strncpy(focus->text, doc->form_fields[focus->index].value, sizeof(focus->text) - 1);
  focus->text[sizeof(focus->text) - 1] = '\0';
  return ok_status();
}

// R50.2 - type into the buffer, capped by max_len; an over-limit append answers
// PC_ERR_LIMIT and keeps the previous buffer.
pc_status pc_form_focus_type(const pc_doc* doc, pc_form_focus* focus, const char* utf8,
                             size_t len) {
  if (!doc || !focus || (!utf8 && len)) {
    return arg_err("null argument");
  }
  if (focus->index < 0 || (uint32_t)focus->index >= doc->form_field_count) {
    pc_status s = {sizeof(pc_status), PC_ERR_STATE, 0, "no field focused"};
    return s;
  }
  const IrFormField* f = &doc->form_fields[focus->index];
  size_t cur = strlen(focus->text);
  if (f->max_len > 0 && cur + len > f->max_len) {
    pc_status s = {sizeof(pc_status), PC_ERR_LIMIT, 0, "typing exceeds field max_len"};
    return s;
  }
  if (cur + len >= sizeof(focus->text)) {
    pc_status s = {sizeof(pc_status), PC_ERR_LIMIT, 0, "typing exceeds focus buffer"};
    return s;
  }
  memcpy(focus->text + cur, utf8, len);
  focus->text[cur + len] = '\0';
  return ok_status();
}

// R50.2 - commit the buffer as one PC_CMD_FORM_SET: field-granular undo, same log as
// R48.3. After a successful commit the buffer equals the stored value.
pc_status pc_form_focus_commit(const pc_doc* doc, pc_txn* txn, pc_form_focus* focus) {
  if (!doc || !txn || !focus) {
    return arg_err("null argument");
  }
  if (focus->index < 0 || (uint32_t)focus->index >= doc->form_field_count) {
    pc_status s = {sizeof(pc_status), PC_ERR_STATE, 0, "no field focused"};
    return s;
  }
  const IrFormField* f = &doc->form_fields[focus->index];
  pc_status s = pc_form_fill_field(txn, f->name, focus->text);
  if (s.code == PC_ERR_NONE) {
    strncpy(focus->text, f->value, sizeof(focus->text) - 1);
    focus->text[sizeof(focus->text) - 1] = '\0';
  }
  return s;
}

// R50.2 - SPACE: checkbox toggles Off<->Yes as one command; a text field types one
// space. The toggle reads the IR value, not the buffer (checkboxes have no buffer).
pc_status pc_form_toggle(const pc_doc* doc, pc_txn* txn, pc_form_focus* focus) {
  if (!doc || !txn || !focus) {
    return arg_err("null argument");
  }
  if (focus->index < 0 || (uint32_t)focus->index >= doc->form_field_count) {
    pc_status s = {sizeof(pc_status), PC_ERR_STATE, 0, "no field focused"};
    return s;
  }
  const IrFormField* f = &doc->form_fields[focus->index];
  if (f->type == (uint32_t)PC_FORM_FIELD_CHECKBOX) {
    const char* next = (strcmp(f->value, "Off") == 0) ? "Yes" : "Off";
    return pc_form_fill_field(txn, f->name, next);
  }
  return pc_form_focus_type(doc, focus, " ", 1);
}

// R50.3 - the diffable a11y announcement for the focused field. The four shapes are
// quoted verbatim in docs/a11y/forms.md and pinned by tests/unit/test_forms_focus.cc;
// changing one is a script change, not a refactor.
pc_status pc_form_focus_announce(const pc_doc* doc, const pc_form_focus* focus, char* out,
                                 size_t capacity) {
  if (!doc || !focus || !out || capacity == 0) {
    return arg_err("null argument");
  }
  if (focus->index < 0 || (uint32_t)focus->index >= doc->form_field_count) {
    pc_status s = {sizeof(pc_status), PC_ERR_STATE, 0, "no field focused"};
    return s;
  }
  const IrFormField* f = &doc->form_fields[focus->index];
  if (f->type == (uint32_t)PC_FORM_FIELD_CHECKBOX) {
    int n = snprintf(out, capacity, "%s, checkbox, %s", f->name,
                     (strcmp(f->value, "Off") == 0) ? "not checked" : "checked");
    if (n < 0 || (size_t)n >= capacity) {
      return arg_err("announcement would overflow");
    }
    return ok_status();
  }
  if (focus->text[0] == '\0') {
    int n = snprintf(out, capacity, "%s, edit, empty", f->name);
    if (n < 0 || (size_t)n >= capacity) {
      return arg_err("announcement would overflow");
    }
    return ok_status();
  }
  int n = snprintf(out, capacity, "%s, edit, value %s", f->name, focus->text);
  if (n < 0 || (size_t)n >= capacity) {
    return arg_err("announcement would overflow");
  }
  return ok_status();
}

// ================ Flatten (R53.x, story 7.4) ================

// R53.1 - build the flatten command. Value type; the transaction steals the IR's field
// array at apply time so undo restores editability.
pc_status pc_form_cmd_flatten(pc_command* cmd) {
  if (!cmd) {
    return arg_err("null argument");
  }
  memset(cmd, 0, sizeof(*cmd));
  cmd->size = sizeof(pc_command);
  cmd->type = PC_CMD_FORM_FLATTEN;
  return ok_status();
}

// R53.1 - bake + log: the engine bakes widgets into static content and saves a NEW
// file to out_path (the source is never modified in place); only then is the command
// applied to the IR, so a failed bake leaves both the file and the IR untouched.
// Undo restores the IR's editability model - it does not un-bake the saved file.
pc_status pc_form_flatten(const pc_backend_api* api, void* backend_doc, pc_doc* doc, pc_txn* txn,
                          const char* out_path) {
  if (!api || !backend_doc || !doc || !txn || !out_path) {
    return arg_err("null argument");
  }
  if (!api->form_flatten) {
    return cap_err("backend does not declare PC_CAP_FORMS");
  }
  pc_status s = api->form_flatten(api, backend_doc, out_path);
  if (s.code != PC_ERR_NONE) {
    return s;
  }
  pc_command cmd;
  s = pc_form_cmd_flatten(&cmd);
  if (s.code != PC_ERR_NONE) {
    return s;
  }
  return pc_txn_apply(txn, &cmd);
}
