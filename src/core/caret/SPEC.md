# Caret + Input Feature Specification

Status: story 6.2 implements caret keyboard navigation (R33.x), selection extend (R34.x),
clipboard (R35.x), and UIA caret announcement (R36.x).

## Requirements

### R33.1 The core SHALL provide `pc_caret_left` and `pc_caret_right` moving one extended grapheme cluster per step, crossing combining sequences and ABNT2 compositions in ONE step, never stopping inside them.

Verification: unit:tests/unit/test_caret.cc

### R33.2 The viewer SHALL wire keyboard arrows (←/→), Home, End, Ctrl+←/→ to `pc_caret_left/right` and update the caret position in the selection state.

Verification: unit:tests/unit/test_caret_keyboard.cc

### R34.1 The viewer SHALL support Shift+←/→, Shift+Home, Shift+End, Ctrl+Shift+←/→ to extend the selection from the caret position, updating the selection byte range and quads.

Verification: unit:tests/unit/test_selection_extend.cc

### R35.1 The core SHALL provide a platform-neutral `pc_clipboard_set_text` / `pc_clipboard_get_text` that copies the selected UTF-8 text to the system clipboard.

Verification: unit:tests/unit/test_clipboard.cc

### R36.1 The UIA state SHALL include caret position (byte offset) and selection range, and the announced text SHALL reflect caret/selection when focus is on the document.

Verification: script:tools/win32-ui-selftest.sh

## Out of scope

- Multi-page selection (story 6.2+ continues)
- Search/highlight all occurrences (story 6.3)
- Annotation authoring on selection (story 6.4)
- Re-anchoring (story 6.5)
