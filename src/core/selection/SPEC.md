# Selection Feature Specification

Status: story 6.1 implements the hit-test core (R32.1, R32.2), CLI subcommand (R32.3),
window click parity (R32.4), and sidecar round-trip (R32.5).
The text layout capability (R13.4) is declared by backends implementing abi 1.2.

## Requirements

### R13.4 The backend SHALL declare PC_CAP_TEXT_LAYOUT and implement `page_text_layout` / `page_text_layout_free` returning one page's text as UTF-8 plus per-cluster quads in user-space points, or PC_ERR_CAPABILITY when unsupported.

Verification: unit:tests/contract/backend_contract.cc

### R32.1 The core SHALL provide `pc_selection_hit_test` that, given a device-space point, DPI, page crop box, and a backend page, returns the text box (quad + byte range) containing that point.

Verification: unit:tests/unit/test_selection.cc

### R32.2 The core SHALL use `pc_rect_to_device` inverse logic (or equivalent) to map the device point into page user-space coordinates before testing against text box quads.

Verification: unit:tests/unit/test_selection.cc

### R32.3 The CLI SHALL expose `tynypdf-cli select <pdf> <page> <x> <y> <dpi>` that prints the matching quad and byte range as JSON.

Verification: unit:tests/unit/test_cli_select.cc

### R32.4 The Win32 window SHALL forward left-click events to the viewer, which calls `pc_selection_hit_test` and updates the selection state. A subsequent render SHALL highlight the selected quad.

Verification: script:tools/win32-ui-selftest.sh

### R32.5 The selection (page index + byte range) SHALL round-trip through the sidecar without loss.

Verification: unit:tests/unit/test_selection_sidecar.cc

## Out of scope

- Multi-page selection (story 6.2+)
- Selection drag/extend (story 6.2+)
- Copy to clipboard (story 6.2+)
- Search/highlight all occurrences (story 6.3)
- Annotation authoring on selection (story 6.4)
