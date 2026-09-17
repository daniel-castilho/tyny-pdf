# Render Capability Specification

## Requirements

### R15.1 The render layer SHALL provide a DComp/D3D11 swapchain with Per-Monitor V2 DPI awareness.

Verification: manual: implementation in progress

### R15.2 The render layer SHALL provide a tile cache with deterministic eviction.

Verification: manual: implementation in progress

### R15.3 The render layer SHALL blit pc_page_render output to the swapchain via Direct2D.

Verification: manual: implementation in progress

### R15.4 The render layer SHALL support Per-Monitor V2 DPI scaling without bitmap stretch.

Verification: manual: implementation in progress

## Out of scope

- PDF parsing (handled by backends)
- Font rendering (handled by DirectWrite)
- Annotation rendering (handled by src/features/annotation)
- Text layout (handled by src/core/text)
