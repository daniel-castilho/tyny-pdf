# Forms Feature Specification

Status: story 7.1 shipped FDF export + honest capability answers (R47.x); story 7.2 wired
the CLI fill/list through the core transaction log (R49.x). FDF import stays deferred
(7.1 partial issue).

## Requirements

### R47.1 The viewer SHALL wire FDF export to the core, exposing byte-stable FDF through the CLI and (later) the window.

Verification: unit:tests/unit/test_forms.cc

### R47.2 The feature SHALL ensure engine types and functions remain inside the MuPDF backend bridge.

Verification: script:tools/check-fz-widget-core.sh

### R49.1 The CLI forms fill/list commands SHALL flow through the core IR and the transaction log (pc_form_ir_load_from_backend, pc_form_fill_field, pc_form_fdf_export_ir), never the engine directly, so a CLI fill and a viewer fill produce the same artefact.

Verification: unit:tests/unit/test_forms_fill.cc

### R49.2 The forms feature SHALL surface a backend that does not declare PC_CAP_FORMS as PC_ERR_CAPABILITY on every entry point (list, IR load, FDF), never as an empty list or a silent success.

Verification: unit:tests/unit/test_forms_fill.cc

## Out of scope

- FDF import (deferred, 7.1 partial issue)
- Radio and Combo field types (7.3+)
- Tab/focus/keyboard wiring in the viewer (story 7.3)
- Flatten (story 7.4)
- Digital signatures (Epic 8+)
