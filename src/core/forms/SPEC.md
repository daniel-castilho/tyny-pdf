# Forms Core Specification

Status: story 7.1 shipped the field model + FDF export (R46.x) with enumeration via the
AcroForm dict walk; story 7.2 added fill + validate + undo through the transaction log
(R48.x). FDF import stays deferred (7.1 partial issue, #73): writing /V back into the AcroForm
tree needs an incremental-save story first.

## Requirements

### R46.1 The core SHALL provide a field model for AcroForm with Text and Checkbox field types, and FDF export that is byte-stable for a given IR state.

Verification: unit:tests/unit/test_forms.cc

### R46.2 The backend SHALL enumerate AcroForm fields through the document's Root/AcroForm/Fields tree (the pdf dict walk - the fz_widget API does not exist in the vendored MuPDF), copying every name, value, flag, max_len and rect into core value types.

Verification: unit:tests/unit/test_forms.cc

### R46.3 The core SHALL ensure no engine type or function is referenced outside the MuPDF backend bridge.

Verification: script:tools/check-fz-widget-core.sh

### R48.1 The core SHALL provide `pc_form_ir_add_field` that adds one form field to the document IR as pure value types.

Verification: unit:tests/unit/test_forms_fill.cc

### R48.2 The core SHALL provide `pc_form_ir_load_from_backend` that populates the IR through the backend vtable, propagating the backend's status unchanged so a backend without PC_CAP_FORMS answers PC_ERR_CAPABILITY and leaves the IR untouched.

Verification: unit:tests/unit/test_forms_fill.cc

### R48.3 The core SHALL provide `pc_form_fill_field` that validates a fill against the IR's field constraints and applies it as a PC_CMD_FORM_SET command in the transaction log: unknown field answers PC_ERR_ARGUMENT, readonly answers PC_ERR_STATE, max_len exceeded answers PC_ERR_RANGE, a checkbox value other than Yes/Off answers PC_ERR_ARGUMENT, and undo SHALL restore the exact prior IR state (hash-equal) while redo re-applies the value.

Verification: unit:tests/unit/test_forms_fill.cc

### R48.4 The core SHALL provide `pc_form_fdf_export_ir` exporting the IR's form fields to FDF bytes that are identical for a given IR state and carry every field's current value.

Verification: unit:tests/unit/test_forms_fill.cc

### R50.1 The core SHALL provide a focus model (`pc_form_focus_init/next/prev/field`) that cycles the IR's field list in document order, wrapping at the ends, answering PC_ERR_STATE when no field is focused or the document has no fields.

Verification: unit:tests/unit/test_forms_focus.cc

### R50.2 The core SHALL provide the keyboard actions on the focus model: typing appends to the buffer with max_len enforced (over-limit answers PC_ERR_LIMIT and keeps the buffer), commit applies the buffer as ONE PC_CMD_FORM_SET, Space toggles a checkbox Off<->Yes as one command and types one space into a text field, and load/cancel resync the buffer to the field value.

Verification: unit:tests/unit/test_forms_focus.cc

### R50.3 The core SHALL provide `pc_form_focus_announce` producing the exact a11y strings quoted in docs/a11y/forms.md: "<Name>, edit, value <v>", "<Name>, edit, empty", "<Name>, checkbox, checked", "<Name>, checkbox, not checked" - changing one is a script change, not a refactor.

Verification: unit:tests/unit/test_forms_focus.cc

## Out of scope

- FDF import (deferred, #73: needs incremental save)
- Radio and Combo field types (7.3+; the dict walk maps Ch to the COMBO type constant but
  fill validation only models Text and Checkbox)
- Hierarchical field names (parent.child joining arrives with radio groups)
- page_index per field (the widget-to-page walk lands with tab order in 7.3)
- Digital signatures (Epic 8+)
### R53.1 The core SHALL provide `pc_form_flatten` baking widget appearances into static page content and removing the interactive form into a NEW file (the source is never modified in place), applied to the IR as ONE PC_CMD_FORM_FLATTEN whose undo restores the fields (editability), with the backend answering PC_ERR_CAPABILITY when it does not declare PC_CAP_FORMS; undo restores the IR's editability model, not a file already saved.

Verification: unit:tests/unit/test_forms_flatten.cc

### R53.2 The bake SHALL be pixel-identical: rendering a page with widgets before flatten equals rendering the baked page after (sha256 of the rendered bytes), and the saved file reopens with zero form fields.

Verification: unit:tests/unit/test_forms_flatten.cc

### R53.3 The render contract SHALL expose widget drawing (`pc_render_params.render_widgets`, abi 1.4) so the pre-flatten half of R53.2 is renderable; a backend that does not draw widgets leaves the flag a no-op.

Verification: unit:tests/unit/test_forms_flatten.cc
