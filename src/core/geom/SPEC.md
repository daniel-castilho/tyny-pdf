# Core Geometry - capability specification

Status: drafted before implementation. Requirements whose artefact is a `unit:` test that does not
exist yet are reported as `pending` by `tools/spec-check.py`.

Document: geometry conversion between user space and device space. Design and rationale:
[`adr/0011-modularity-rules.md`](../../../adr/0011-modularity-rules.md) R-M4, R-M8.

## Requirements

### R8.1 The core SHALL provide `pc_rect_to_device` converting a user-space rectangle to
device-space (CropBox-relative at 72 DPI) for all four rotations (0, 90, 180, 270) and
CropBox != MediaBox.

Verification: unit:tests/unit/test_geom.cc

### R8.2 The conversion SHALL be accurate to 1e-9 tolerance for all four rotations.

Verification: unit:tests/unit/test_geom.cc

### R8.3 The function SHALL validate input and return `PC_ERR_ARGUMENT` for null pointers or
invalid rotation values.

Verification: unit:tests/unit/test_geom.cc

## Out of scope

- Rendering (belongs in `src/render`).
- GPU-specific transforms.

