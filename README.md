# Tyny PDF

![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![Windows](https://img.shields.io/badge/Windows-x64--ARM64-0078D6?style=for-the-badge&logo=windows&logoColor=white)
![Engine](https://img.shields.io/badge/engine-MuPDF-black?style=for-the-badge)
![Build](https://img.shields.io/badge/CMake-Ninja-064F8C?style=for-the-badge&logo=cmake&logoColor=white)
![Domain](https://img.shields.io/badge/Domain-tyny.ca-8A2BE2?style=for-the-badge)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPLv3-blue?style=for-the-badge)](LICENSE)

**Tyny PDF** is an offline, local-first PDF reader and editor for Windows, written in C++ against
Win32 and Direct2D, with no account, no cloud, no telemetry and no WebView. Annotations live in a
byte-canonical JSON file next to the document (`<document>.tynypdf.json`) so they can be diffed,
versioned and audited, and the document model is a published library (`pdfcore`, C ABI) with a
headless companion CLI (`tynypdf-cli`). The bet is that a PDF tool should not need you: it should
not watch you, not upload your files, and not hide your marks inside a binary only it can read.

There is deliberately **no build or test-status badge**: the workflow runs today (docs gates since
story 1.1, the four-job matrix since story 1.2), but a badge would be measuring tooling unless it
waits for a job that has actually failed on bad code - which hasn't happened yet. It gets added the
day a build job fails on a real defect.

Official Domain: [https://tyny.ca](https://tyny.ca) (registered; DNS records are not configured yet,
and nothing in the product requires them).

---

## Table of Contents

- [Key Features](#key-features)
- [Tech Stack](#tech-stack)
- [Architecture & Design Rules](#architecture--design-rules)
- [Requirements](#requirements)
- [Quality Gates](#quality-gates)
- [Current State](#current-state)
- [Roadmap](#roadmap)
- [Documentation & License](#documentation--license)

---

## Key Features

Every item states what exists now: `enforced` means a committed check proves it today, `design`
means it is specified in an ADR or a milestone and not yet built.

- **Diffable Annotations (`design`, M6/D-1):** highlights, notes, ink, stamps and form edits are
  written to a human-readable, byte-canonical sidecar rather than only into the PDF - your marks
  survive a git checkout and can be reviewed line by line.
- **Offline by Construction (`enforced` as a rule, `design` as a job):** networking is
  default-deny per operation with in-UI disclosure ([ADR-0003](adr/0003-error-model.md)); the CI
  proof is a run with no route but localhost.
- **Engine Independence (`design`, M0):** the PDF engine is one backend behind a versioned C
  vtable; a `null` backend exists so the boundary is exercised by two implementations, and
  `backend_line_ratio <= 0.15` bounds the cost of swapping (ADR-0011 R-M11)
- **Supply-Chain Verification (`enforced` as a rule, `design` as a job):** the engine is vendored
  at a pinned commit with a patch series; no floating versions, no unreviewed patches
  (ADR-0004, ADR-0011 R-M11)
- **Accessibility by Design (`design`, M1/D-5):** UIA provider and Narrator/NVDA tested paths;
  ABNT2 + pt-BR IME composition; pinch/zoom/wheel under 16 ms
- **No Telemetry, No Cloud (`enforced` as a rule, `design` as a job):** zero network by default;
  networking is opt-in per operation with disclosure (ADR-0003 section 2); CI proves no external
  calls
- **Annotations as Source of Truth (`design`, M6/D-1):** marks live in a byte-canonical sidecar
  (`*.tynypdf.json`), not baked into the PDF; diffable, reviewable, portable

---

## Tech Stack

| Category                  | Technology                                                            |
| :------------------------ | :-------------------------------------------------------------------- |
| **Language**             | C++20 with a deliberately small subset (no exceptions across the public API, no RTTI) + C for the public ABI |
| **Core Library**         | `pdfcore` - engine-agnostic document model, IR, annotations, forms, text, redaction, sidecar |
| **PDF Engine**           | MuPDF (AGPLv3) as a pinned submodule with an ordered patch series; `pdfium` evaluated as a second backend |
| **Desktop UI**           | Win32 + Direct2D over DirectX 11/DComp, Per-Monitor V2 DPI, UIA for accessibility, no WebView |
| **Build**                | CMake + Ninja, one preset per target; Conan 2 with a committed lockfile for support libraries |
| **Toolchains**           | LLVM-MinGW cross-compilation from Linux for the shipped binary; MSVC (v143 + Windows SDK) for CI parity |
| **Testing**              | GoogleTest, contract suite per backend, approval tests, property tests, libFuzzer, clause-keyed conformance corpus, veraPDF as oracle |
| **Platforms**            | Windows x64 (product); ARM64 built from day one; Linux used for core development, sanitizers and CI |
| **Licence**              | AGPL-3.0-or-later, no CLA, no assignment |

---

## Architecture & Design Rules

The dependency arrow always points inward, toward `src/core`, and never toward a vendor or an OS
(ADR-0002).

```
tyny-pdf/
|-- include/pdfcore/            # PUBLIC ABI: pdfcore.h, backend.h, sealer.h, print.h, sidecar.h
|-- src/
|   |-- core/                   # ENTITIES + USE CASES: document model, IR, undo, sidecar semantics.
|   |                           # no Windows header, no engine header
|   |-- backends/               # ADAPTERS: mupdf/, null/ - the only code allowed to know an engine
|   |-- render/                 # swapchain, tile cache, cachemap - presentation, never parsing
|   |-- os/win32/               # window, dpi, uia, clipboard, policy - the only OS-specific tree
|   |-- sealer/                 # pkcs7_local/, stub/ - signature production port
|   |-- print/                  # win32_gdi/, preview/
|   |-- features/               # VERTICAL SLICES: one directory per capability, each with SPEC.md
|   |-- app/                    # tynypdf.exe - composition root, wiring only, no logic
|   `-- cli/                    # tynypdf-cli - headless twin, JSON/JUnit reporters, exit codes
|-- tests/                      # unit/, contract/, conformance/<iso-clause>/, approvals/, fixtures/
|-- docs/                       # kickoff, naming, dev-environment, git-workflow, lessons, a11y/
|-- adr/                        # one file per decision, each with a "Cost of swapping" section
`-- tools/                      # the gates: check.sh and the checks it runs
```

One rule decides where a line of code lives: the dependency arrow always points inward, toward
`src/core`, and never toward a vendor or an OS (ADR-0002).

### Design decisions (ADRs)

| ADR | Subject | Status |
| :-- | :------ | :----- |
| [0001](adr/0001-engineering-canon.md) | Engineering canon: which literature is normative here | Accepted |
| [0002](adr/0002-repository-layout.md) | Repository layout: vertical slices with horizontal seams | Accepted |
| [0003](adr/0003-error-model.md) | Error model across three layers; default-deny networking | Accepted |
| [0004](adr/0004-dependency-management-and-supply-chain.md) | Dependencies, vendored engine, SBOM, supply chain | Accepted |
| [0005](adr/0005-repository-language-is-english.md) | The repository is written in English | Accepted |
| [0006](adr/0006-product-name-and-identifier.md) | Naming rules and reverse-DNS identifiers | Superseded by 0008 (rules retained) |
| [0007](adr/0007-sidecar-format.md) | Annotation sidecar format | Accepted |
| [0008](adr/0008-product-name-tyny-pdf.md) | Product name is `Tyny PDF` | Accepted |
| [0009](adr/0009-git-workflow.md) | Git workflow: branch per PR, squash, protected `main` | Accepted |
| [0010](adr/0010-build-environment-wsl2.md) | Build environment: WSL2 workshop, cross-built Windows target | Accepted |
| [0011](adr/0011-modularity-rules.md) | Modularity rules R-M1..R-M13 | Accepted |

---

## Requirements

The M0 exit criteria from `docs/kickoff.md` section 6:

| # | Criterion | Exit Gate |
|---|-----------|-----------|
| 1 | Build matrix + vendored MuPDF + C API + page 1 rendered on both targets | `cmake --preset linux-core && cmake --build --preset linux-core && ctest --preset linux-core` green; `sh tools/build-mupdf-windows.sh && cmake --preset win-cross-x64` produces `tynypdf.exe` at `build/win-cross-x64/Release/` |
| 2 | Four ADR-0010 assumptions retired by measurement | `tools/win-probe/build.sh` output: headers compile, static runtime, identical symbol tables, hardware adapter observed |
| 3 | Page 1 renders twice from one API (Linux CLI + Win32 window) | `tynypdf-cli render --backend null or mupdf` + Win32 window blit same pixels |
| 4 | Baselines measured on both SumatraPDF channels | `tools/bench-measure.sh` runs, two runs within 10%, relative bar in `docs/kickoff.md` |
| 5 | Competitive baseline measured on both SumatraPDF channels | `tests/baseline.json` with machine spec, two runs within 10% |

---

## Quality Gates

Coverage numbers would be theatre in a repository whose product is still being built, so the gate
is structural: every documented claim must be checkable, and every check must be able to fail.
`sh tools/check.sh` runs all eleven sections in review order; `.github/workflows/gates.yml` runs
`tools/gates-selftest.sh` and `sh tools/check.sh` on every PR.

| Gate | What it proves | Self-test |
| :--- | :------------- | :-------- |
| `tools/lang-check.py` | the repository is English, no mojibake, no stray Portuguese | 5 properties |
| `tools/sidecar-fmt.py` | sidecars are schema-valid and byte-canonical; `fix` is idempotent | 5 properties |
| `tools/naming-sync.py` | derived identifiers match `docs/naming.md`; retired brand tokens are gone | 5 properties |
| `tools/spec-check.py` | EARS shape, no orphan requirement ids, "done" has an artefact | 14 properties |
| `tools/docs-check.py` | tables, links and fence balance; a forward reference must name the PR | 11 properties |
| `tools/canonical-check.sh` | no CRLF, BOM, trailing whitespace or missing final newline | - |
| `tools/format-check.sh` | C/C++ style against the committed `.clang-format`, plus `--staged` for the hook | 2 properties |
| `tools/diff-scan.py` | the D1-D8 change-hygiene scan, on a diff or the whole tree | 17 properties |
| `tools/layering-check.sh` | ADR-0011 R-M10/R-M11: header/symbol layering and `backend_line_ratio <= 0.15` | 8 properties |
| `tools/gates-selftest.sh` | every gate's own self-test in one command | 9/9 suites |
| `.github/workflows/gates.yml` | `gates-selftest.sh` plus `sh tools/check.sh`, on every PR | - |

---

## Current State

**Epic 1: 5/5 stories complete. Epic 2: documentation complete. Epic 3+: not started.**

**Epic 1 (Stories 1.1-1.5 delivered):**

- **1.1 [DONE]** Seed + gates + branch protection (PR #1, #4).
- **1.2 [DONE]** Build system: CMake presets (`linux-core`, `win-cross-x64`,
  `windows-msvc`), LLVM-MinGW pinned toolchain + SHA-256, style configs +
  `tools/format-check.sh`, `conanfile.py` + `conan.lock` (`gtest/1.17.0`), four
  ADR-0010 probes (`abi_probe`, `d2d_probe`, `runtime_probe`, `gpu_probe`), the
  4-job CI matrix (`gates`, `core-linux`, `windows-mingw-cross`, `windows-msvc`),
  actions pinned by full SHA.
- **1.3 [DONE]** MuPDF 1.26.8 vendored (`third_party/mupdf/`, SHA256
  `e8d248a666d2386f4a2014d680b6e88de5ce9fd8c847b0e274cbecc124f33cc7`),
  UPSTREAM.toml pinned, 4 patch series files, `tools/layering-check.sh`,
  `tools/deps-refresh.sh`/`patch-report.sh`/`sbom.sh`, `NOTICE`,
  `docs/references.md`.
- **1.4 [DONE]** M0 API surface (`include/pdfcore/` headers), null backend
  (`src/backends/null/`), MuPDF backend (`src/backends/mupdf/`) with a real
  `page_render`, CLI (`tynypdf-cli render --page N --dpi D --backend null|mupdf`),
  contract tests (`test_backend_contract.cc`, run against both backends), CI
  matrix green, `pdfcore` INTERFACE library.
- **1.5 [DONE]** Benchmark infrastructure ready (`tools/bench-measure.sh`,
  `tests/bench/harness/`, `tests/baseline.json`, `tools/corpus-check.py`, corpus
  with 3 PDFs + manifest). SumatraPDF baseline measurements are performed locally
  on the reference Windows machine and pasted into `tests/baseline.json`; CI runs
  only `tynypdf` benchmarks. Acceptance criteria defined in `tests/baseline.json`.

**Epic 1 gates:** `sh tools/check.sh` -> all 11 sections green (clean clone verified);
the 4-job CI matrix is required on `main`; branch protection enforced.

**Epic 2 (UI spike): documentation complete** (`tasks/epic-02/` five files:
overview, stories, technical-tasks, testing, dod). The Win32/DComp/tile-cache
scaffolding is parked in `spikes/epic2-win32/` (not built, not a capability)
until R14/R15 have real verification.

**Distribution:** Windows Store / MSIX is not pursued. The portable Windows ZIP (no installer, no
admin rights) attached to a GitHub Release is the only channel; the download is not live until the
first tagged release is published.

---

## Roadmap

- [x] Deliberation closed: 11 ADRs recorded (10 accepted), six v1 deltas, four standing gates
- [x] Repository gates written and self-tested
- [x] Annotation sidecar format specified, with schema and normative fixture
- [x] **M0** - repository becomes real: CI matrix, vendored MuPDF, `pdfcore` C API, page 1 rendered
      on Windows and as a PNG on Linux, Sumatra 3.6.1 **and** 3.7 pre-release baselines measured
- [ ] **M1** - three-day UI spike with the performance and accessibility floor, including its kill
      criterion (if the blit cannot hold 60 fps with the tile cache and DComp visual tree, the stack
      decision is revisited with the spike's numbers in a new ADR)
- [ ] **M2** - D-6: undo/redo as a command log over the IR, public as `pc_txn_*`
- [ ] **M3** - D-4: diacritics, line breaking, font fallback with a golden corpus
- [ ] **M4** - D-2: form validation, tab order, flatten verified by veraPDF
- [ ] **M5** - D-5: editor accessibility (UIA, Narrator, NVDA) as tested requirements
- [ ] **M6** - D-1 and D-3: the sidecar, redaction with proof, portable ZIP on GitHub Releases
      plus winget, `0.1.0`
- [ ] After 0.1.0: revocation and TSA behind the explicit revalidate action, MSI, signing
      reputation, and the second render backend only if M1's numbers demand it

---

## Documentation & License

| Document                                                          | Purpose                                                          |
| :---------------------------------------------------------------- | :--------------------------------------------------------------- |
| [`docs/kickoff.md`](docs/kickoff.md)                              | Thesis, non-goals, milestones, CI, testing, risks, first 5 PRs   |
| [`AGENTS.md`](AGENTS.md)                                          | Operating rules for agents and humans (the same rules)           |
| [`docs/dependency-policy.md`](docs/dependency-policy.md)          | The operating procedure ADR-0004 section 6 points at            |
| [`docs/git-workflow.md`](docs/git-workflow.md)                    | Branch-per-PR loop, stacked PRs, hooks, branch protection         |
| [`docs/dev-environment.md`](docs/dev-environment.md)              | WSL2 setup, cross toolchain, what is only testable on Windows     |
| [`docs/naming.md`](docs/naming.md)                                | The single table every identifier is generated from              |
| [`docs/sidecar.schema.json`](docs/sidecar.schema.json)            | Normative JSON Schema for the sidecar                            |
| [`src/features/sidecar/SPEC.md`](src/features/sidecar/SPEC.md)     | The first living specification, with traceable verification      |
| [`docs/coding-standards.md`](docs/coding-standards.md)              | Day-to-day coding detail: naming, boundaries, do versus don't |
| [`docs/testing-playbook.md`](docs/testing-playbook.md)               | How tests are designed, run and diagnosed; what runs today   |
| [`docs/release-runbook.md`](docs/release-runbook.md)                  | The release procedure, with every blocker and decision named  |
| [`docs/lessons.md`](docs/lessons.md)                              | Mistakes that became rules                                       |
| [`CHANGELOG.md`](CHANGELOG.md)                                    | Release history (Keep a Changelog)                               |
| [`LICENSE`](LICENSE)                                              | AGPL-3.0-or-later, FSF text verbatim                             |
| [`docs/a11y/`](docs/a11y/)                                        | Accessibility scripts and test plans                             |

---

Tyny PDF is released under the **GNU AGPLv3**; there is no contributor licence agreement and no
assignment - your patch stays yours and stays under the same licence. Deliberation notes and
analyses that led to the ADRs were written in Portuguese outside this repository and are not
shipped: the repository is English-only, enforced by `tools/lang-check.py`.

Binaries, when they exist, are published unsigned with the reason stated on the download page
until a signing decision is recorded in an ADR.
