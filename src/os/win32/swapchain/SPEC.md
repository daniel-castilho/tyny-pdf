# Swapchain Subsystem (M1)

Status: implemented (story 5.2). The swapchain requirement R25.1 is defined and
verified in `src/render/SPEC.md`; this slice holds the implementation
(`src/os/win32/swapchain/swapchain.cc`, in the OS tree because a Windows or DirectX
header may not appear under `src/render` - AGENTS.md rule 1, ADR-0011 R-M10).

## Out of scope

- Tile cache with deterministic eviction (a later story defines and verifies it, story 5.3)
- Blitting `pc_page_render` output to the swapchain via Direct2D (story 5.3)
- Per-Monitor V2 DPI scaling without bitmap stretch (story 5.3)
