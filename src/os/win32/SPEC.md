# Win32 OS Layer Capability Specification

## Requirements

### R14.1 The Win32 OS layer SHALL create a Per-Monitor V2 DPI-aware window.

Verification: manual: implementation in progress

### R14.2 The Win32 OS layer SHALL create a DComp/D3D11 swapchain and present frames.

Verification: manual: implementation in progress

### R14.3 The Win32 OS layer SHALL forward DPI change events to the render layer.

Verification: manual: implementation in progress

## Out of scope

- Direct2D rendering (handled by src/render)
- Input handling (handled by src/os/win32/input.cc)
- Accessibility (UIA provider in src/os/win32/uia.cc)
