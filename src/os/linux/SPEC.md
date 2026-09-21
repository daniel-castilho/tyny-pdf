# OS Linux - platform-specific implementations

Status: drafted before implementation. This directory contains Linux-specific implementations
for the OS abstraction layer. Design and rationale: ADR-0002, ADR-0011.

## Requirements

### R25.1 The Linux OS abstraction SHALL provide sidecar lock operations using POSIX APIs.

Verification: unit:tests/unit/test_sidecar_lock.cc

## Out of scope

- Windows-specific implementations (see src/os/win32).
- Higher-level sidecar semantics (see src/core/sidecar).
