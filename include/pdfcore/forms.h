#ifndef PDFCORE_FORMS_H
#define PDFCORE_FORMS_H

#include <stddef.h>
#include <stdint.h>

#include "pdfcore/geom.h"
#include "pdfcore/status.h"
#include "pdfcore/transaction.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration to avoid circular dependency with backend.h
typedef struct pc_backend_api pc_backend_api;

// Form field types (matches AcroForm field types)
typedef enum pc_form_field_type {
  PC_FORM_FIELD_TEXT = 0,
  PC_FORM_FIELD_CHECKBOX = 1,
  PC_FORM_FIELD_RADIO = 2,
  PC_FORM_FIELD_COMBO = 3,
} pc_form_field_type;

// Form field flags (PDF 32000-1 Table 229, the /Ff entry). Named PC_FFLAG_* so a flag can
// never collide with a pc_form_field_type constant below (the 7.1 macros shadowed the
// PC_FORM_FIELD_COMBO enum and only surfaced when a backend first assigned the type).
#define PC_FFLAG_READONLY (1u << 0)           // bit 1
#define PC_FFLAG_REQUIRED (1u << 1)           // bit 2
#define PC_FFLAG_NO_EXPORT (1u << 2)          // bit 3
#define PC_FFLAG_MULTILINE (1u << 12)         // bit 13
#define PC_FFLAG_PASSWORD (1u << 13)          // bit 14
#define PC_FFLAG_COMBO (1u << 17)             // bit 18
#define PC_FFLAG_RADIOS_IN_UNISON (1u << 25)  // bit 26

// One form field in the document
typedef struct pc_form_field {
  pc_form_field_type type;
  uint32_t page_index;
  pc_rect rect;
  char* name;           // UTF-8, null-terminated, caller frees
  char* value;          // UTF-8, null-terminated, caller frees
  char* default_value;  // UTF-8, null-terminated, caller frees
  char** options;       // For combo/radio: array of UTF-8 strings, null-terminated
  uint32_t options_count;
  uint32_t max_len;  // 0 = unlimited
  char* format;      // Format string (e.g., "AFNumber_Keystroke"), caller frees
  uint32_t flags;
  // Opaque backend-specific data (set by backend, freed by backend)
  void* backend_data;
} pc_form_field;

// List of form fields in a document
typedef struct pc_form_list {
  pc_form_field* items;
  uint32_t count;
} pc_form_list;

// FDF export/import
typedef struct pc_fdf {
  char* data;  // UTF-8, caller frees
  size_t size;
} pc_fdf;

// Get all form fields in a document
// Returns PC_ERR_NONE on success, PC_ERR_ARGUMENT for null args,
// PC_ERR_CAPABILITY if backend doesn't support forms.
pc_status pc_form_list_fields(const pc_backend_api* api, void* backend_doc, pc_form_list* out_list);

// Free a form list returned by pc_form_list_fields
void pc_form_list_free(pc_form_list* list);

// Export form fields to FDF (Forms Data Format)
// Returns PC_ERR_NONE on success, out_fdf->data is malloc'd UTF-8
pc_status pc_form_fdf_export(const pc_backend_api* api, void* backend_doc, pc_fdf* out_fdf);

// Import form fields from FDF
// Returns PC_ERR_NONE on success, PC_ERR_ARGUMENT for invalid FDF
pc_status pc_form_fdf_import(const pc_backend_api* api, void* backend_doc, const char* fdf_data,
                             size_t fdf_size);

// Free FDF data returned by pc_form_fdf_export
void pc_fdf_free(pc_fdf* fdf);

// ================ Fill + validate + undo (abi 1.3, story 7.2) ================

// Add one form field to the document IR (R48.1). The backend bridge that populates the
// IR from a real AcroForm is pc_form_ir_load_from_backend below; tests and the CLI use
// this entry point directly. Copies name/value/default_value/format into the IR.
// Returns PC_ERR_NONE, PC_ERR_ARGUMENT, PC_ERR_MEMORY.
pc_status pc_form_ir_add_field(pc_doc* doc, const pc_form_field* field);

// Populate the document IR's form fields through the backend vtable (R48.2): calls
// api->form_list_fields and copies every field out into IR value types, then releases
// the backend list. Propagates the backend's status unchanged - PC_ERR_CAPABILITY from
// a backend that does not declare PC_CAP_FORMS leaves the IR untouched (R-M5).
pc_status pc_form_ir_load_from_backend(pc_doc* doc, const pc_backend_api* api, void* backend_doc);

// Build a PC_CMD_FORM_SET command for a form field value change (R48.3). Value type;
// the transaction captures the old value at apply time.
pc_status pc_form_cmd_set(const char* field_name, const char* new_value, pc_command* cmd);

// Fill a form field with validation and undo (R48.3): validates against the IR's field
// constraints, then applies a PC_CMD_FORM_SET through the transaction log. Validation:
// unknown field -> PC_ERR_ARGUMENT; readonly -> PC_ERR_STATE; max_len exceeded ->
// PC_ERR_RANGE; checkbox value other than "Yes"/"Off" (case-insensitive) ->
// PC_ERR_ARGUMENT. Undo restores the previous value.
pc_status pc_form_fill_field(pc_txn* txn, const char* field_name, const char* new_value);

// Read a form field's current value from the IR into out_value (truncating at
// capacity-1). Returns PC_ERR_ARGUMENT for null args or unknown field.
pc_status pc_form_get_value(const pc_doc* doc, const char* field_name, char* out_value,
                            size_t capacity);

// Export the document IR's form fields to FDF (R48.4). Byte-stable for a given IR
// state, so fill -> export is reproducible headless. Caller frees with pc_fdf_free.
pc_status pc_form_fdf_export_ir(const pc_doc* doc, pc_fdf* out_fdf);

// ================ Focus model + keyboard actions (abi 1.3, story 7.3) ================

// Focus state: a value type pointing at one IR field (index) plus the text
// buffer for a focused TEXT field. index -1 means no field focused.
typedef struct pc_form_focus {
  int32_t index;
  char text[512];  // typing buffer, UTF-8, holds the focused TEXT field's pending value
} pc_form_focus;

// Initialize focus with no field focused (R50.1).
pc_status pc_form_focus_init(pc_form_focus* focus);

// Move focus to the next/previous field in IR (document) order, wrapping around the
// ends (R50.1). A document with no fields answers PC_ERR_STATE and focus stays none.
pc_status pc_form_focus_next(const pc_doc* doc, pc_form_focus* focus);
pc_status pc_form_focus_prev(const pc_doc* doc, pc_form_focus* focus);

// Name of the focused field, or PC_ERR_STATE when none is focused (R50.1).
pc_status pc_form_focus_field(const pc_doc* doc, const pc_form_focus* focus, char* out_name,
                              size_t capacity);

// Load the focused TEXT field's current value into the typing buffer (focus enter,
// Escape/cancel) (R50.2).
pc_status pc_form_focus_load(const pc_doc* doc, pc_form_focus* focus);

// Type UTF-8 bytes into the buffer, enforcing the field's max_len: an append that
// would exceed it answers PC_ERR_LIMIT and the buffer keeps its previous content (R50.2).
pc_status pc_form_focus_type(const pc_doc* doc, pc_form_focus* focus, const char* utf8, size_t len);

// Commit the buffer to the IR as one PC_CMD_FORM_SET through the transaction log -
// field-granular undo, matching R48.3 (R50.2).
pc_status pc_form_focus_commit(const pc_doc* doc, pc_txn* txn, pc_form_focus* focus);

// SPACE action (R50.2): on a checkbox, toggle Off<->Yes as one PC_CMD_FORM_SET; on a
// text field, type one space character. PC_ERR_STATE when no field is focused.
pc_status pc_form_toggle(const pc_doc* doc, pc_txn* txn, pc_form_focus* focus);

// The a11y announcement for the focused field, exact text diffable against
// docs/a11y/forms.md (R50.3):
//   text field, value present: "<Name>, edit, value <v>"
//   text field, empty buffer:  "<Name>, edit, empty"
//   checkbox value Off:       "<Name>, checkbox, not checked"
//   checkbox otherwise:       "<Name>, checkbox, checked"
// PC_ERR_STATE when no field is focused (the viewer announces the page state instead).
pc_status pc_form_focus_announce(const pc_doc* doc, const pc_form_focus* focus, char* out,
                                 size_t capacity);

// ================ Flatten (abi 1.4, story 7.4) ================

// Build a PC_CMD_FORM_FLATTEN command (R53.1). Value type; the transaction steals the
// IR's field array into the command at apply time, so undo restores editability.
pc_status pc_form_cmd_flatten(pc_command* cmd);

// Bake widget appearances into static page content and remove the interactive form
// (R53.1): the backend bakes + saves a NEW PDF to out_path (the source is never modified
// in place), then the command is applied to the IR so every form entry point answers
// "no fields" and undo restores them. A backend without PC_CAP_FORMS answers
// PC_ERR_CAPABILITY and nothing is logged. Undo restores the IR's editability model -
// it does not un-bake a file already saved to out_path (that file is a new artefact).
pc_status pc_form_flatten(const pc_backend_api* api, void* backend_doc, pc_doc* doc, pc_txn* txn,
                          const char* out_path);

#ifdef __cplusplus
}
#endif

#endif  // PDFCORE_FORMS_H