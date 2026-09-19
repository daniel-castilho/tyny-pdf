# Development environment

Normative for onboarding (including agents); the decisions behind it are
[ADR-0010](../adr/0010-build-environment-wsl2.md), [ADR-0009](../adr/0009-git-workflow.md) and
[ADR-0004](../adr/0004-dependency-management-and-supply-chain.md). If this page and a CI file
disagree, CI is right and this page is a bug.

## Layout of the machine

| Where | What | Notes |
| --- | --- | --- |
| `~/projects/tyny-pdf` (ext4, in WSL2) | the one and only working copy | never under `/mnt/c`, never a second Windows-side checkout of the same tree |
| Ubuntu userland | `git`, `cmake`, `ninja`, `python3`, `clang-format`, `clang-tidy`, Conan 2 | everything except presentation is developed here |
| LLVM-MinGW (pinned tarball) | cross build of `tynypdf.exe`, x64 and ARM64 | tarball + SHA-256 under `third_party/toolchains/` (ADR-0010); unpacked under `~/.toolchains/llvm-mingw-<ver>/`, version pinned in `CMakePresets.json` |
| Windows side | Visual Studio Build Tools (MSVC v143 + Windows SDK), WinDbg, NVDA, the printers in the acceptance list | used for the CI-parity build, GUI debugging, accessibility and print runs |

`~/.wslconfig` on this host: `memory=12GB`, `processors=8`, `swap=0`. The link step under
sanitizers is what a 16 GB laptop feels first, and WSL2's default balloon is too generous for the
parallel jobs CI emulates locally.

## Toolchain pins (measured, Stories 1.1-1.2)

Each version below was printed by the tool itself on the day it was recorded; a row whose tool is
not installed says so and names the PR that installs it. Versions are deliberately not a range: a
`git tag` name, a release number or a tarball hash pins what CI and the working copy run.

| Tool | Version / pin | State | Notes |
| --- | --- | --- | --- |
| `git` | 2.43.0 | installed | `git --version` |
| `cmake` | 3.31.6 | installed | `pipx install cmake==3.31.6`; `/usr/bin/cmake` stays 3.28.3 (apt), the pitx one is first on PATH. Recorded while 3.31.6 was the current release; CI jsonschema/cmake pins match this box exactly |
| `ninja` | 1.11.1 | installed | `ninja --version` |
| `python3` | 3.12.3 | installed | `python3 --version` |
| `clang` / `clang++` | 18.1.3 (Ubuntu) | installed | `clang --version` |
| `g++` | 13.3.0 (Ubuntu) | installed | `g++ --version` |
| `clang-format` | 18.1.3 | installed | `clang-format --version`; format gate `tools/format-check.sh` with `--self-test` |
| `clang-tidy` | 18.1.3 | installed | `clang-tidy --version`; first real run lands with story 1.4's code |
| `conan` (2.x) | 2.32.0 | installed | `conan --version`, via pipx; graph locked by `conan.lock` (ADR-0004) |
| `doxygen` | - | not installed | first needed by the docs build in story 1.4; not promised before then |
| LLVM-MinGW | 20260812-ucrt-ubuntu-22.04-x86_64 (clang 23.1.0-rc3) | installed | tarball under `~/.toolchains/llvm-mingw-20260812-ucrt-ubuntu-22.04-x86_64/`, hash of `bin/clang` in `third_party/toolchains/llvm-mingw.sha256`, version pinned in `CMakePresets.json`; verified by CI in the `windows-mingw-cross` job |
| MSVC v143 + Windows SDK | Windows-side | not installed | the `windows-msvc` CI job is the parity path; local install optional (ADR-0010) |

## One-time setup

```sh
# in Ubuntu (WSL2)
sudo apt update && sudo apt install -y git ninja-build python3 python3-pip \
  clang-format clang-tidy build-essential python3-jsonschema
pipx install cmake==3.31.6
pipx install conan==2.32.0
# LLVM-MinGW: /usr/local/bin must win the PATH race against the apt cmake (3.28.3).
sudo ln -sf ~/.local/pipx/venvs/cmake/bin/cmake /usr/local/bin/cmake
sudo ln -sf ~/.local/pipx/venvs/cmake/bin/ctest /usr/local/bin/ctest
sudo ln -sf ~/.local/pipx/venvs/cmake/bin/cpack /usr/local/bin/cpack
git clone git@github.com:daniel-castilho/tyny-pdf.git ~/projects/tyny-pdf
cd ~/projects/tyny-pdf
git config core.autocrlf false
git config core.fileMode false
git config core.hooksPath .githooks
chmod +x tools/*.sh tools/*.py .githooks/*   # git preserves the exec bit; re-apply after a fresh clone of a seed that lost it
python3 tools/naming-sync.py write && sh tools/check.sh
sha256sum ~/.toolchains/llvm-mingw-*/bin/clang   # must equal third_party/toolchains/llvm-mingw.sha256
```

The toolchain pin is a hash, not a name, and the hash is of the compiler binary, not of the
tarball label - a different build of the same named release trips it, which is the point:

```sh
curl -L -O https://github.com/mstorsjo/llvm-mingw/releases/download/20260812/llvm-mingw-20260812-ucrt-ubuntu-22.04-x86_64.tar.xz
tar -xJf llvm-mingw-20260812-ucrt-ubuntu-22.04-x86_64.tar.xz -C ~/.toolchains
sha256sum ~/.toolchains/llvm-mingw-*/bin/clang | sed 's|.*: ||'   # compare with third_party/toolchains/llvm-mingw.sha256
```

## Daily loop

```sh
cmake --preset linux-core          # core + CLI, native, ASan/UBSan
cmake --build --preset linux-core
ctest --preset linux-core --output-on-failure
sh tools/check.sh                  # the same six gates the pre-commit hook runs
sh tools/build-mupdf-windows.sh    # cross-compiled MuPDF for the win-cross-x64 preset
cmake --preset win-cross-x64 && cmake --build --preset win-cross-x64
./build/win-cross-x64/Release/tynypdf.exe ~/tmp/sample.pdf    # WSL interop: real Windows process, real GPU, real spooler
```

Running the `.exe` from bash is the normal path, not a hack. Two consequences worth knowing:

- Windows sees the repository as `\\wsl.localhost\<distro>\home\<user>\projects\tyny-pdf`, and
  `cmd.exe` refuses a UNC working directory, so any Windows tool that shells out through `cmd.exe`
  breaks on this tree. If a Windows-only tool must run, map a drive first
  (`net use W: \\wsl.localhost\Ubuntu\home\<user>\projects\tyny-pdf`) - and if that starts feeling
  normal, the tool belongs in CI instead.
- MSVC from inside WSL is possible through a wrapper (`.bat` calling `vcvars64.bat`, plus explicit
  `-I`/`-LIBPATH`, plus a `TEMP` under `/mnt/c`) and is documented as fragile in other projects that
  tried it. It is not the supported local path here; the `windows-msvc` CI job is.

## What is only testable on Windows

Direct2D/DComp presentational timing, Per-Monitor V2 DPI behaviour, UIA (Narrator and NVDA reading
annotation and form controls), IME composition with ABNT2 and the Portuguese keyboard, the print
dialog and the actual spooler, Authenticode and SmartScreen reputation. Everything else must
reproduce on the Linux side; if it cannot, the presentation layer has logic in it that belongs in
`src/core`, and `tools/layering-check.sh` is the check that says so.

## Debugging

| Symptom | Tool |
| --- | --- |
| wrong geometry, wrong bytes, wrong validation | native `gdb`/ASan in Ubuntu, driven by the CLI `tynypdf-cli` |
| crash in `tynypdf.exe` under WSL interop | WinDbg on Windows (`windbg -p <pid>`), or the crash dump under `%LOCALAPPDATA%\CrashDumps` |
| focus, reading order, narration | Narrator (Win+Ctrl+Enter) and NVDA on the Windows desktop |
| slow frame | `presentmon` on Windows plus the in-app frame recorder; never a guess |

## Reproducing CI locally

```sh
act -j core-linux          # optional; GitHub Actions runner images are not in the sandbox
# the honest local equivalent is the union of the jobs:
sh tools/check.sh && ctest --preset linux-core && sh tools/build-mupdf-windows.sh && cmake --preset win-cross-x64 --fresh
```
