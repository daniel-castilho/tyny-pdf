# OS Win32 - platform-specific implementations

Status: the sidecar lock (R24.1), the window implementation (R24.2, story 5.2), the swapchain
(R24.3, story 5.2) and the DPI module with the viewer timing log (R24.4-R24.6, story 5.4) are
in; the rest of the OS abstraction (uia, clipboard, policy) is still planned. Design and
rationale: ADR-0002, ADR-0011.

## Requirements

### R24.1 The Windows OS abstraction SHALL provide sidecar lock operations using Win32 APIs.

Verification: unit:tests/unit/test_sidecar_lock.cc

### R24.2 The Windows window implementation SHALL provide the `pc_window` ABI with Per-Monitor V2 DPI awareness, a DComp visual and a WM_SIZE swapchain resize, keeping every Windows type inside `src/os/win32` (R15.2, R25.1).

Verification: unit:tests/unit/test_window_swapchain.cc

### R24.3 The Windows swapchain implementation SHALL provide the `pc_swapchain` ABI on DXGI flip-sequential buffers with a D2D target bitmap, keeping every DirectX type inside `src/os/win32` (R25.1).

Verification: unit:tests/unit/test_window_swapchain.cc

### R24.4 The DPI module SHALL declare Per-Monitor V2 process awareness (`SetProcessDpiAwarenessContext`) before any window is created and SHALL provide `get_scale_for_monitor` as the only source of the initial window scale (R24.2's `WM_DPICHANGED` remains the runtime source).

Verification: script:tools/win32-ui-selftest.sh

### R24.5 The DPI selftest (`tynypdf --dpi-selftest`) SHALL render the test pattern at 150% and 200% and SHALL require the scale-transformed render to be byte-for-byte equal to the render asked at the scaled size, writing the approvals `tests/approvals/dpi-150.png` and `dpi-200.png`.

Verification: script:tools/win32-ui-selftest.sh

### R24.6 The window message loop SHALL stamp wheel input (`WM_MOUSEWHEEL`, `WM_POINTERWHEEL`) and the frame-ready time of the following present into `build/tynypdf.ui.log` as `input_ts`/`present_ts` lines, and the first present SHALL log the cold-start triple (`init_to_create_ms`, `create_to_present_ms`, `init_to_present_ms`) plus a machine block; `present_ts` is the frame-ready stamp - the compositor owns the queue wait.

Verification: script:tools/win32-ui-selftest.sh

## Out of scope

- Linux-specific implementations (see src/os/linux).
- Higher-level sidecar semantics (see src/core/sidecar).
- Touch pinch gestures (no touch hardware on the measurement box; wheel carries the latency
  claim).
- IME composition (Windows job only, ADR-0010; caret logic itself is headless, R23.1-R23.3).
- Page content rendering in the live window (R10.1's pipeline; the viewer presents frames).
