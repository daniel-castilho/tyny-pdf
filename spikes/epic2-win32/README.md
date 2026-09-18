# Parked spike: Epic 2 Win32 render path

This directory holds unfinished Epic 2 work that is **not built, not a capability and not a
requirement artefact**. It was moved out of `src/` on purpose: `tools/spec-check.py` treats the
presence of any source file under `src/<capability>/` as proof that the capability has code, at
which point every requirement in that `SPEC.md` must name a real test that cites its id. The code
here is scaffolding whose requirements (`src/os/win32/SPEC.md` R14.x, `src/render/SPEC.md` R15.x)
are still `manual: implementation in progress`, so keeping it under `src/` would either force those
requirements to claim tests that do not exist or fail the gate.

- `window.cc` - Win32 window creation plus a DComp/D3D11 swapchain and Direct2D blit, only ever
  compiled on Windows (`CMAKE_SYSTEM_NAME STREQUAL "Windows"`); it never had a test.
- `swapchain.cc`, `tile_cache.cc`, `cachemap.cc` - stubs that return `PC_ERR_UNSUPPORTED`.

When Epic 2 implements these for real, each file returns to its `src/` capability with a test that
cites R14.x/R15.x in the same change. Until then the requirements stay pending, which
`tools/spec-check.py` reports on every run.
