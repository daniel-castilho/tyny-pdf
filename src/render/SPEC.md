# Render Layer (M1) — Swapchain, Tiles, Cachemap

Status: implemented (story 5.2); the DirectX implementation lives in
`src/os/win32/swapchain/` because rule 1 forbids a Windows header under `src/render`
(R-M10). See `src/features/render/SPEC.md` for R15.1-R15.4.

## Requirements

### R25.1 The render layer SHALL provide a DComp/D3D11 swapchain with Per-Monitor V2 DPI awareness.

Verification: unit:tests/unit/test_window_swapchain.cc

## Out of scope

- Tile cache with deterministic eviction (a later story defines and verifies it, story 5.3)
- Blitting `pc_page_render` output to the swapchain via Direct2D (story 5.3)
- Per-Monitor V2 DPI scaling without bitmap stretch (story 5.3)
- PDF parsing (handled by backends)
- Font rendering (handled by DirectWrite)
- Annotation rendering (handled by src/features/annotation)
- Text layout (handled by src/core/text)
