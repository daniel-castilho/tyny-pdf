# OS Win32 - platform-specific implementations

Status: drafted before implementation. This directory contains Windows-specific implementations
for the OS abstraction layer. Design and rationale: ADR-0002, ADR-0011.

## Requirements

### R24.1 The Windows OS abstraction SHALL provide sidecar lock operations using Win32 APIs.

Verification: unit:tests/unit/test_sidecar_lock.cc

## Out of scope

- Linux-specific implementations (see src/os/linux).
- Higher-level sidecar semantics (see src/core/sidecar).
