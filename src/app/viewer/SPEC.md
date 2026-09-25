# App Viewer (M1) — Content Viewer Loop

Status: story 1.5. The viewer opens a document through the active backend and
presents the visible region as 256x256 tiles, bounded by a `pc_budget` from the
core (ADR-0011 R-M8). The loop is the first content-bearing caller of the
window ABI (R15.2) and the tile cache/cachemap (R27.x, R28.x); `tools/
bench-measure.sh` measures the resident-memory ceiling (R30.2).

## Requirements

### R30.1 The viewer SHALL open a PDF document through the active backend and present the visible region as 256x256 tiles served by the tile cache and cachemap bounded by a pc_budget, drawing every frame in the window render callback.

Verification: unit:tests/unit/test_viewer_loop.cc

### R30.2 The viewer SHALL hold peak resident memory at or below 250 MiB when scrolling a 1000-page document at three tiles per page, with the return-pass peak within 10% of the forward-pass peak (growth beyond that means tiles accumulate without eviction).

Verification: script:tools/bench-measure.sh

### R51.1 The viewer SHALL own the forms IR and transaction log and wire the keyboard to the core focus model (R50.x): Tab commits the buffer and moves focus (Shift+Tab moves back), WM_CHAR typing fills the buffer, Space toggles a checkbox, Escape discards the buffer - all headless-testable on Linux through the same TU Windows drives.

Verification: unit:tests/unit/test_forms_keyboard.cc

### R51.2 The viewer SHALL expose `viewer_forms_announcement` returning the focused field's exact announcement (R50.3) as UTF-16, answering PC_ERR_STATE when no field is focused so the caller keeps the page announcement (R24.7).

Verification: unit:tests/unit/test_forms_keyboard.cc

## Out of scope

- Page content beyond the tile loop: text selection, annotations, zoom/pan gestures
- Undo, redaction and flatten coercion (owned by `src/core`, R-M10)
- Engine-specific tile rendering (owned by backends, R-M4)
- The 4000x3000 frame-budget measurement itself (R15.1, `src/features/render/`)
