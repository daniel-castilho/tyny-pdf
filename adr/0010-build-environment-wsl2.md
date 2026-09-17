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
`tools/layering-check.sh` (planned, PR #4) asserts that `src/render` and `src/os/win32` contain no
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

## Assumptions that PR #2 must retire (not verified in the planning sandbox)

| Assumption | Probe (a 10-minute job, before anything else is built on top of it) |
| --- | --- |
| MinGW ships the Direct2D/DirectWrite/D3D11/DXGI headers we need, at a usable revision | `tools/win-probe/` : one translation unit including `d2d1_1.h`, `dwrite_2.h`, `d3d11_1.h`, `dxgi1_4.h`, instantiating `D2D1_RENDER_TARGET_PROPERTIES` and a swapchain descriptor, then linking with `-ld2d1 -ldwrite -ld3d11 -ldxgi` |
| A static runtime is actually static (no `libstdc++-6.dll`, no VC++ redist) | `x86_64-w64-mingw32-g++ -static` on the probe, then `objdump -p a.exe \| grep 'DLL Name'` and the result must be only system DLLs (`KERNEL32`, `USER32`, `GDI32`, ...) |
| The same CMake target builds under both toolchains | add `windows-msvc` to CI in the same PR, not the next one |
| WSL interop runs the GUI binary with a real GPU adapter | the probe window must report the adapter LUID it got; if it reports the software rasteriser, M1 timing is measured on the Windows session only |

The sandbox used to write these decisions has no MinGW and no root
(`apt-get install g++-mingw-w64-x86-64` -> `dpkg lock ... are you root?`; package candidate
14.2.0 exists but is not installable here), so the table is a scheduled check and not a claim.

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
