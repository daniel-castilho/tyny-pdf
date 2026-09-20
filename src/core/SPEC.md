# Core - capability specification

Status: drafted before implementation. This is the parent specification for all `src/core/*`
subdirectories. Individual capabilities have their own SPEC.md.

Document: the tyny-pdf core IR library. Design and rationale:
[`adr/0011-modularity-rules.md`](../../adr/0011-modularity-rules.md) R-M1..R-M13.

## Requirements

### R9.1 The core SHALL compile with `-fno-exceptions -fno-rtti` on both Linux (GCC/Clang) and
Windows (LLVM-MinGW) toolchains.

Verification: unit:tests/unit/test_doc_ir.cc

### R9.2 The core SHALL include no Windows headers (`windows.h`, `windef.h`, `unknwn.h`,
`d2d1.h`, `dwrite.h`) and no engine headers (`fitz.h`, `mupdf.h`, `pdfium.h`).

Verification: unit:tests/unit/test_doc_ir.cc

### R9.3 The core SHALL link no engine libraries.

Verification: unit:tests/unit/test_doc_ir.cc

### R9.4 The core SHALL maintain `backend_line_ratio <= 0.07` after Epic 3 completion.

Verification: unit:tests/unit/test_doc_ir.cc

## Out of scope

- Presentation (`src/render`, `src/os/win32`).
- Engine-specific code (`src/backends/*`).