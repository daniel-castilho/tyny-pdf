# OS Win32 - platform-specific implementations

Status: the sidecar lock (R24.1) and the window implementation (R24.2, story 5.2) are in;
the rest of the OS abstraction (dpi, uia, clipboard, policy) is still planned. Design and
rationale: ADR-0002, ADR-0011.

## Requirements

### R24.1 The Windows OS abstraction SHALL provide sidecar lock operations using Win32 APIs.

Verification: unit:tests/unit/test_sidecar_lock.cc

### R24.2 The Windows window implementation SHALL provide the `pc_window` ABI with Per-Monitor V2 DPI awareness, a DComp visual and a WM_SIZE swapchain resize, keeping every Windows type inside `src/os/win32` (R15.2, R25.1).

Verification: unit:tests/unit/test_window_swapchain.cc

### R24.3 The Windows swapchain implementation SHALL provide the `pc_swapchain` ABI on DXGI flip-sequential buffers with a D2D target bitmap, keeping every DirectX type inside `src/os/win32` (R25.1).

Verification: unit:tests/unit/test_window_swapchain.cc

## Out of scope

- Linux-specific implementations (see src/os/linux).
- Higher-level sidecar semantics (see src/core/sidecar).
