# ADR-0010: Build environment - WSL2 as the workshop, Windows as the target

- Status: Accepted (ratified by the project owner, 2026-09-16)
- Date: 2026-09-16
- Related: ADR-0002 (layout), ADR-0003 (errors), ADR-0004 (dependencies), ADR-0009 (git)

## Context

The development machine is Windows 11 (a Legion 5 Pro; kernel reported as
`6.18.33.2-microsoft-standard-WSL2`, x86_64), but the working environment is Ubuntu under WSL2,
because that is where the coding agent runs. The owner's requirement is that the project must
still compile and run on Windows.

This collides with the earlier stack decision (D3: native Win32 + Direct2D, chosen over WebView
shells) in one specific way: the toolchain that is comfortable on Windows (MSVC) is exactly the one
that is awkward to invoke from WSL. Verified sources:

- Driving `cl.exe` from WSL has been attempted in serious projects and documented as painful:
  Windows binaries do not understand Unix paths, `/tmp` is invisible to them, and the `INCLUDE`
  and `LIB` environment that MSVC expects does not survive the boundary cleanly, so every Windows
  interface needs manual path translation (Mozilla's WSL build experiment, bug 1311729). The
  community workarounds are a `.bat` wrapper plus `cmd.exe /q /c` (a gist pattern, widely
  repeated).
- `cmd.exe` refuses a UNC working directory, and a WSL path presented to Windows is
  `\\wsl.localhost\<distro>\...`, so any toolchain that shells out through `cmd.exe` breaks when
  the repository lives on the Linux filesystem; the usual advice is to map a drive letter.

## Decision

### 1. One working copy, on the Linux filesystem, driven from Ubuntu

`~/projects/tyny-pdf`. No second copy under `/mnt/c`, no Windows-side git against the same tree.
Rationale: cross-boundary file access is the slow and failure-prone direction, and ADR-0009 rule 7
depends on line endings that only hold when one side owns the checkout.

### 2. The shipped Windows binary is produced by a Linux cross-toolchain

**Primary: LLVM-MinGW** (`x86_64-w64-mingw32`, and `aarch64-w64-w64-mingw32` for ARM64), invoked
through a CMake preset. It runs as a normal Linux toolchain, targets i686/x86-64/ARM/AArch64,
supports AddressSanitizer and UndefinedBehaviorSanitizer, and can emit debug info as PDB
(mingw-w64 project documentation). Ubuntu also ships the GCC mingw-w64 cross packages
(`g++-mingw-w64-x86-64`); UCRT-targeting cross packages exist since Debian 13, so the practical
choice today on Ubuntu is either the `msvcrt` runtime packages or the LLVM-MinGW tarball, which is
the recommended one because it is versioned by us rather than by the distro.

Why this is better than "just use MSVC" for this project:

- No ABI mixing risk: our engine, MuPDF, is built from source (ADR-0004), so there is no
  MSVC-built import library to marry.
- **Static runtime**: an LLVM-MinGW build can link libc++ and libunwind statically, which serves
  decision D7b directly: the user of a "no installer, no account, no cloud" reader should not have
  to install a Visual C++ redistributable first. That is a product feature, not a build detail.
- Sanitizers and fuzzers work in the environment they are fastest in, and the same object files are
  then usable for the native Linux port later.

### 3. MSVC stays a supported target, verified in CI - not the local loop

A `windows-msvc` job on `windows-latest` builds the same CMake targets with the MSVC toolset.
This keeps three things honest: (a) contributors who only have Visual Studio can build;
(b) toolchain divergence shows up on the PR, not at release time; (c) any Windows-SDK-specific
header lag in MinGW is caught the same day. Nobody is asked to fight `cl.exe` through WSL interop;
if someone wants to, `tools/winbuild.cmd` (planned, PR #4) will be that wrapper,
documented as optional.

### 4. Where each part of the project is developed

| Layer | Where | Why |
| --- | --- | --- |
| `src/core/**` (document model, IR, annotations, forms, sidecar, undo) | Ubuntu, native gtest/ASan/UBSan/libFuzzer | most of v1's deltas live here; no Windows API involved |
| `src/backends/**`, `src/sealer/**`, `src/print/**` logic | Ubuntu, with the `null` backend and mocked OS interfaces | contract suite runs twice per ADR-0002 |
| `tools/tynypdf-cli` | Ubuntu, 100% | this is why the CLI exists: it is the Linux-side face of everything except presentation |
| `src/render/**`, `src/os/win32/**` (window, DComp/D3D11 swapchain, DPI, UIA, clipboard) | cross-built and **run** from Ubuntu as `./build/Release/tynypdf.exe` via WSL interop; debugger on the Windows side | a native Windows process launched from WSL gets a full Windows session, so GPU, spooler, UIA and IME are real |
| Accessibility and print acceptance (D-5, and the print parity gate) | Windows session: Narrator, NVDA, WinDbg, a physical printer | cannot be simulated from the Linux side |

Debugging rule that follows: **a bug is only interesting once it reproduces on the Linux side**, so
the presentation layer keeps no logic that the core cannot drive. The corollary is a test:
`tools/layering-check.sh` asserts that `src/render` and `src/os/win32` contain no
parsing, no
geometry reconciliation and no annotation mutation.

### 5. Practical setup, recorded so it is not folklore

`docs/dev-environment.md` holds the exact list; it starts as:

```
Ubuntu 24.04 (WSL2)   git, cmake >= 3.30, ninja, python3, clang-format, clang-tidy
LLVM-MinGW             pinned tarball under third_party/toolchains/ with its SHA-256 recorded
Conan 2                pipx install conan, one conan.lock per release (ADR-0004)
Windows side           Visual Studio Build Tools (MSVC v143 + Windows SDK) for the CI-parity build,
                       WinDbg, NVDA, and the printers used by the acceptance list
git config (both)      core.autocrlf=false, core.fileMode=false, init.defaultBranch=main
git config (repo)      core.hooksPath=.githooks   (ADR-0009 rule 8)
.wslconfig             memory=12GB, processors=8, swap=0 on this host: the C++ link step and the
                       sanitizer runs are what a 16 GB laptop will feel first
```

### 6. Corpus transport: submodule, not LFS

The clause-keyed conformance corpus (ADR-0007 and decision D13) is a separate public repository
`daniel-castilho/tyny-pdf-corpus`, pinned as a submodule; the big archives are fetched in CI from a
release asset with a recorded hash. Git LFS is not used for it: LFS quotas on the free tier would
become a silent build failure mode for a repository whose whole value is a corpus, and PDFs do not
delta-compress well anyway. (The exact free-tier LFS numbers were not verified here and do not
matter once the decision is "no LFS".)

## Assumptions retired by PR #2 (measured, 2026-09-16, `tools/win-probe/`)

Each row was a scheduled check in the planning sandbox (no MinGW, no root there). PR #2
runs the four probes and records what came back; the "planning sandbox" footnote below is
kept so the table reads as history and not as a claim about that machine.

| Assumption | Probe | Result (this machine, cross → interop) |
| --- | --- | --- |
| MinGW ships the Direct2D/DirectWrite/D3D11/DXGI headers we need, at a usable revision | `d2d_probe.c` including `d2d1_3.h`, `dwrite_3.h`, `d3d11.h`, `dxgi1_6.h`, linking `-ld2d1 -ldwrite -ld3d11 -ldxgi`, factories created at runtime | PASS. MinGW headers (LLVM-MinGW 20260812, clang 23.1.0-rc3) expose `ID2D1Factory7`/`IDWriteFactory7`/`IDXGIFactory6`; all four factories create through WSL interop; first adapter `0x10DE:0x2560`, 5994 MB dedicated. Two measured surprises: the versioned `dwrite_*.h` do not chain (each must be included in order) and the factory7-era interfaces sit behind `NTDDI_VERSION` guards whose C-mode declarations are MingW `/* FIXME */` stubs - so `d2d_probe.c` keeps its name but compiles as C++ (see its CMake comment) |
| A static runtime is actually static (no `libstdc++-6.dll`, no VC++ redist) | `runtime_probe.c` with `-static` (LLVM-MinGW) / `/MT` (MSVC), import table listed | PASS. `objdump -p winprobe_runtime.exe`: only `KERNEL32.dll` and the `api-ms-win-crt-*` api-set (system) appear; the probe also refuses to start if a compiler runtime DLL is mapped. MSVC `/MT` is enforced via `MSVC_RUNTIME_LIBRARY` and verified by `dumpbin /DEPENDENTS` in the `windows-msvc` CI job |
| The same CMake target builds under both toolchains | `abi_probe.cc` mirroring the story 1.4 `pc_*` surface, `nm -C --defined-only` diffed | PASS (host vs cross). `pc_doc_open`, `pc_doc_close`, `pc_doc_page_count`, `pc_page_render` both sides, symbol tables identical. The `windows-msvc` CI job (added in the same PR) builds the same `winprobe_test` aggregate from the same source |
| WSL interop runs the GUI binary with a real GPU adapter | `gpu_probe.cpp` via `CreateDXGIFactory1` + `EnumAdapters1` + `D3D11CreateDevice` | PASS. Two hardware adapters enumerate (NVIDIA `0x10DE:0x2560`, AMD `0x1002:0x1681`) plus the Microsoft software rasteriser (`0x1414`); device creates at `D3D_FEATURE_LEVEL_11_0`. A machine with only the software adapter makes the probe exit 2, which is the signal to change the GPU story - so the CI job treats the probe as informational and the hardware claim stays measured on this box |

The planning sandbox used to write these decisions had no MinGW and no root
(`apt-get install g++-mingw-w64-x86-64` -> `dpkg lock ... are you root?`; package candidate
14.2.0 exists but was not installable there), which is why the table above is a result and
not a claim. The toolchain hash recorded in `third_party/toolchains/llvm-mingw.sha256`
verified in CI by hashing the extracted `bin/clang` - the pin is the compiler, not a
tarball label.

## Consequences

- Positive: the owner's preferred environment is the primary one, and the "does it build on
  Windows" question is answered by a matrix job rather than by hope.
- Positive: static runtime and a single cross-toolchain give one reproducible build recipe that
  runs on Linux CI without a Windows license for day-to-day work.
- Negative: two toolchains on the same source is a permanent tax - warning sets, header coverage
  (`d2d1.h`, `d3d11.h`, `dwrite_3.h`) and `__declspec` behaviour differ. Mitigation: CI treats both
  as first-class, and the code must not use MSVC-only extensions in `src/core` (enforced by the
  MinGW job compiling with `-Werror`).
- Negative: PDB-based debugging of the Windows binary from inside WSL is not practical, so GUI
  bugs are read from logs or debugged on the Windows side. Accepted, because rule 4 pushes logic out
  of that layer.
- Open: WSL2 GPU rendering fidelity for a D3D11/DComp swapchain is good but not identical to bare
  metal; the 60 fps blit criterion (D6b spike) must therefore be measured twice, once in WSL for
  development and once on the Windows session for the acceptance record. That is a measurement
  location rule, not a guess about which is faster.
