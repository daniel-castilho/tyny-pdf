# Core Render (M1) — Window, Swapchain, Tiles

Status: story 5.2 implements the window/swapchain ABI (R15.2) and the boundary
rules (R15.3, R15.4); R15.1's frame budget is measured by `tools/bench-measure.sh`.

## Requirements

### R15.1 The core SHALL present frames at 60fps (p99 ≤33ms) for a 4000×3000 region using a DComp/D3D11 swapchain and Direct2D blit, with byte-identical output to `tynypdf-cli render --region 4000x3000`.

Verification: script:tools/bench-measure.sh

### R15.2 The core SHALL provide `pc_window_create`/`pc_window_run` (Per-Monitor V2 DPI, DComp visual, WM_SIZE swapchain resize, render callback) and `pc_swapchain_create`/`pc_swapchain_resize`/`pc_swapchain_begin_draw`/`pc_swapchain_end_draw` (DXGI flip-sequential swapchain, D2D target bitmap, Present(1,0)).

Verification: unit:tests/unit/test_window_swapchain.cc

### R15.3 The core SHALL keep `src/os/win32/window/` and `src/os/win32/swapchain/` free of engine includes (`grep -rn "fitz\|mupdf" src/os/win32 src/render` = 0).

Verification: script:tools/layering-check.sh

### R15.4 The core SHALL NOT perform hit-testing, annotation parsing, or document logic in `src/os/win32/` or `src/render/` — those live in `src/core` (R-M10).

Verification: script:tools/layering-check.sh

## Out of scope

- Tiles/cachemap (story 5.3).
- Cold start, DPI, gesture, caret (story 5.4).
- UIA accessibility (story 5.5).
