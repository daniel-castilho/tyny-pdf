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

There is deliberately **no build or test-status badge**: the repository has no CI history yet, and a
badge that cannot fail is decoration. It gets added when `gates` has run green on `main`.

Official Domain: [https://tyny.ca](https://tyny.ca) (registered; DNS records are not configured yet,
and nothing in the product requires them).

---

## Table of Contents

- [Key Features](#key-features)
- [Tech Stack](#tech-stack)
- [Architecture & Design Rules](#architecture--design-rules)
- [Requirements](#requirements)
- [Getting Started](#getting-started)
- [CLI Usage](#cli-usage)
- [Commands](#commands)
- [Quality Gates](#quality-gates)
- [The Annotation Sidecar](#the-annotation-sidecar)
- [Backend Support](#backend-support)
- [Privacy & Local-First Model](#privacy--localfirst-model)
- [Non-Goals](#non-goals)
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
  `backend_line_ratio <= 0.15` bounds the cost of swapping
  ([ADR-0011](adr/0011-modularity-rules.md)).
- **Undo as a Published API (`design`, M2):** a command log over an intermediate representation,
  exposed as `pc_txn_*`, so the CLI and third parties get the same history the UI does.
- **Diacritics-First Text (`design`, M3):** line breaking, caret movement and font fallback
  correct for pt-BR and other combining-mark languages, graded by a golden corpus instead of
  opinion.
- **Forms You Can Trust (`design`, M4):** required/format/range validation, tab order matching the
  visual order, and flatten verified with veraPDF as the oracle.
- **Redaction with Proof (`design`, M6):** applying a redaction produces an artefact - a
  text-extraction audit of the output - not just a black box on screen.
- **Accessible Editor (`design`, M5):** UIA patterns for the canvas, the annotation list and form
  fields; EN 301 549 clauses become requirements with tests, which is also the procurement gate.
- **Local Signing, Honestly Labelled (`design`, M6):** verify integrity plus the local chain
  (V0+V1); sign with a user-owned `.pfx`, labelled `PAdES-B-B (local)`; revocation, TSA and LTV only
  behind an explicit "revalidate online" action.
- **Native, Small, Portable (`design`, M0/M6):** Win32 + Direct2D over a DComp swapchain, no
  WebView, no installer required, no admin rights; a cross-built binary that needs no runtime
  redistributable.
- **Headless CLI (`design`, M0):** `tynypdf-cli` is the Linux-side testable face of everything
  except presentation, with CI-friendly exit codes and JSON/JUnit reporters.
- **The Repository Checks Itself (`enforced`):** language policy, naming, canonical bytes, spec
  traceability and documentation honesty are six checks plus a runner in `tools/`; the runner is
  `sh tools/check.sh`.

---

## Tech Stack

| Category                  | Technology                                                                                                             |
| :------------------------ | :--------------------------------------------------------------------------------------------------------------------- |
| **Language**             | C++20 with a deliberately small subset (no exceptions across the public API, no RTTI) + C for the public ABI          |
| **Core Library**         | `pdfcore` - engine-agnostic document model, IR, annotations, forms, text, redaction, sidecar                           |
| **PDF Engine**           | MuPDF (AGPLv3) as a pinned submodule with an ordered patch series; `pdfium` evaluated as a second backend |
| **Desktop UI**           | Win32 + Direct2D over DirectX 11/DComp, Per-Monitor V2 DPI, UIA for accessibility, no WebView                          |
| **Build**                | CMake + Ninja, one preset per target; Conan 2 with a committed lockfile for support libraries                           |
| **Toolchains**           | LLVM-MinGW cross-compilation from Linux for the shipped binary; MSVC (v143) verified in CI - see [ADR-0010](adr/0010-build-environment-wsl2.md) |
| **Testing**              | GoogleTest, contract suite per backend, approval tests, property tests, libFuzzer, clause-keyed conformance corpus, veraPDF as oracle |
| **Platforms**            | Windows x64 (product); ARM64 built from day one; Linux used for core development, sanitizers and CI                      |
| **Licence**              | ![License: AGPL v3](https://img.shields.io/badge/License-AGPLv3-blue?style=for-the-badge) - copyleft, no CLA           |

---

## Architecture & Design Rules

The project implements Clean Architecture and SOLID with an explicit, machine-checked boundary: the
core knows nothing about Windows or about the engine, and the presentation layer is not allowed to
hold document semantics.

```
tyny-pdf/
|-- include/pdfcore/   the only stable public surface: C headers (pdfcore, backend, sealer,
|                      print, sidecar) exporting pc_ symbols
|-- src/
|   |-- core/          document model + IR: doc, annot, form, text, search, sig, budget
|   |-- backends/      mupdf/  null/    implement the vtable in include/pdfcore/backend.h (R-M2)
|   |-- render/        swapchain, tile cache, cachemap  - no parsing, no mutation
|   |-- os/win32/      window, dpi, uia, clipboard, policy
|   |-- sealer/        pkcs7_local/  stub/
|   |-- print/         win32_gdi/  preview/
|   |-- features/      vertical slices, each with its own SPEC.md (sidecar lives here)
|   |-- app/           tynypdf.exe  - composition root only
|   `-- cli/           tynypdf-cli  - headless, JSON/JUnit reporters
|-- tools/             check.sh, lang-check.py, sidecar-fmt.py, naming-sync.py,
|                      spec-check.py, docs-check.py, canonical-check.sh
|-- tests/             unit, contract, conformance/<iso-clause>, approvals, fixtures
|-- docs/              kickoff, naming, dev-environment, git-workflow, lessons, a11y/
`-- adr/               one file per decision, numbered, with a "Cost of swapping"
```

### Boundary Rules

- `src/core/**` never includes a Windows header and never includes an engine header; the engine is
  reached through the vtable in `include/pdfcore/backend.h`, and every object returned across a seam
  is copied
  out into our own value types (R-M1..R-M4).
- Capabilities are declared, not assumed: `pc_doc_find_tables` is only offered when the backend
  reports `PC_CAP_TABLES`, so "unsupported" is never reported as "empty" (R-M5).
- One allocation owner, one concurrency model, both documented at the seam (R-M6, R-M7); engine
  contexts are not shared across threads.
- Semantics (undo, redaction, flatten, sidecar) operate on the IR, never on engine handles (R-M8) -
  which is what makes a second backend plausible instead of a rewrite.
- `src/render` and `src/os/**` contain no parsing, no geometry reconciliation and no annotation
  mutation; a bug that cannot reproduce on the Linux side is a layering bug (R-M10).
- Zero network by default; any operation that may reach a destination asks, discloses and is
  individually switchable.

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

Development happens in **Ubuntu under WSL2** (or any Linux); the shipped binary is Windows, and it
runs from the same shell through WSL interop.

- **Linux (WSL2):** `git`, CMake >= 3.30, Ninja, Python 3.11+ with `jsonschema`, `clang-format`,
  `clang-tidy`, Conan 2.
  ```bash
  sudo apt update
  sudo apt install -y git cmake ninja-build python3 python3-jsonschema \
    clang-format clang-tidy build-essential
  ```
- **Cross toolchain (builds `tynypdf.exe` on Linux):** LLVM-MinGW, pinned by tarball under
  `third_party/toolchains/` with its SHA-256 recorded. Ubuntu's `g++-mingw-w64-x86-64` is the
  alternative; see [ADR-0010](adr/0010-build-environment-wsl2.md) for which claim is verified where.
- **Windows side (only for what cannot be simulated):** Visual Studio Build Tools (MSVC v143 +
  Windows SDK) for the CI-parity build, WinDbg, NVDA, and the printers used by the acceptance list.
- **Suggested `~/.wslconfig` on a 16 GB laptop:** `memory=12GB`, `processors=8`, `swap=0`.
- **One working copy, on the Linux filesystem.** Never `/mnt/c`, and never a second Windows-side
  clone of the same tree.

---

## Getting Started

### 1. Clone the repository

```bash
git clone https://github.com/daniel-castilho/tyny-pdf.git
cd tyny-pdf
git config core.hooksPath .githooks     # the pre-commit gate
git config core.autocrlf false          # line endings are owned by .gitattributes
```

### 2. Install the gate dependencies

```bash
python3 -m pip install --user jsonschema
```

### 3. Run the whole gate

```bash
sh tools/check.sh
```

Expected on a clean tree: seven numbered sections, all OK, and `check: all gates green`. This works
today; the product does not exist yet, which is what the Roadmap is for.

### 4. Build the core (available from M0, not before)

```bash
cmake --preset linux-core && cmake --build --preset linux-core
ctest --preset linux-core --output-on-failure
cmake --preset win-cross-x64 && cmake --build --preset win-cross-x64
./build/win-cross-x64/Release/tynypdf.exe document.pdf    # WSL interop: real Windows process
```

---

## CLI Usage

`tynypdf-cli` is the headless twin of the viewer: the same `pdfcore`, no window, scriptable, and the
surface most tests are written against.

> **Design, not implemented.** What the ADRs fix today is only this: the CLI is headless, it is
> the transport for CI, it exposes the transaction log as `tynypdf-cli txn replay`, and it reports
> through JSON and JUnit with exit codes 0,1,2,3 (ADR-0002, ADR-0003 section 6). Everything else
> below is the shape we intend to freeze in the CLI spec at M0 - do not script against it yet.

```bash
tynypdf-cli sidecar check document.pdf          # canonical bytes + schema
tynypdf-cli sidecar fix  document.pdf           # rewrite in canonical form
tynypdf-cli txn replay   document.tynypdf.json  # undo/redo to byte-identical state
tynypdf-cli form validate document.pdf
tynypdf-cli redact       document.pdf
tynypdf-cli sig          document.pdf
tynypdf-cli render       document.pdf
```

- **Settled by an ADR:** the four exit codes and their meanings (ADR-0003 section 6); JSON and JUnit
  reporters, the format following the report file extension; `sidecar check` and `sidecar fix`, the
  two verbs the committed `tools/sidecar-fmt.py` already uses; `txn replay` as a test target (`M2`).
- **Open, decided in the CLI spec (M0):** flag names, whether `--backend` is user-visible or
  preset-only, whether a memory ceiling is a flag or a preset, and how the viewer's dialogs map onto
  exit codes.
- **Deliberately out of v1:** `sidecar gc`, listed in [ADR-0007](adr/0007-sidecar-format.md) as a
  later cleanup command so the sidecar format itself does not grow a reclaim rule now.

Exit codes are fixed by [ADR-0003](adr/0003-error-model.md) section 6, and all four are tested:
`0` success - `1` assertion failure - `2` usage or input error - `3` document or backend error.

---

## Commands

| Purpose                                          | Command                                          |
| :----------------------------------------------- | :----------------------------------------------- |
| **Run every gate**                             | `sh tools/check.sh`                              |
| **Language policy only (ADR-0005)**            | `python3 tools/lang-check.py`                    |
| **Sidecar canonical form / validation**        | `python3 tools/sidecar-fmt.py check .`           |
| **Regenerate a sidecar fixture**               | `python3 tools/sidecar-fmt.py fix <path>`        |
| **Naming drift and retired tokens**            | `python3 tools/naming-sync.py check`             |
| **Spec shape and requirement traceability**    | `python3 tools/spec-check.py`                    |
| **Docs may only promise what exists**          | `python3 tools/docs-check.py`                    |
| **Byte stability of the tree**                 | `sh tools/canonical-check.sh`                    |
| **A gate's own self-test**                   | `--self-test` for lang/spec/docs, `self-test` for naming/sidecar |
| **Build / test / cross-build (from M0)**       | `cmake --preset linux-core`, `ctest --preset linux-core`, `cmake --preset win-cross-x64` |

---

## Quality Gates

Coverage numbers would be theatre in a repository with no code, so the gate is structural: every
documented claim must be checkable, and every check must be able to fail.

| Gate | What it proves | Self-test |
| :--- | :------------- | :-------- |
| `tools/lang-check.py` | the repository is English, no mojibake, no stray Portuguese | 5 properties |
| `tools/sidecar-fmt.py` | sidecars are schema-valid and byte-canonical; `fix` is idempotent | 5 properties |
| `tools/naming-sync.py` | derived identifiers match `docs/naming.md`; retired brand tokens are gone | 5 properties |
| `tools/spec-check.py` | EARS shape, no orphan requirement ids, "done" has an artefact | 14 properties |
| `tools/docs-check.py` | tables, links and fence balance; a forward reference must name the PR | 11 properties |
| `tools/canonical-check.sh` | no CRLF, BOM, trailing whitespace or missing final newline | - |
| `.github/workflows/gates.yml` | all of the above, plus the five self-tests, on every PR | - |

From M0 the set grows with: unit tests per capability, the contract suite run against every backend,
the clause-keyed conformance corpus graded against veraPDF, ASan/UBSan and nightly libFuzzer, the
layering check with `backend_line_ratio <= 0.15`
(`tools/layering-check.sh` (planned, PR #4)), and a no-route network job.

---

## The Annotation Sidecar

The one file format the project owns, frozen at the first public release
([ADR-0007](adr/0007-sidecar-format.md)). One file per document, next to it:
`document.pdf.tynypdf.json`.

```json
{
  "format_version": 1,
  "document": { "fingerprint": "sha256:9f2c...", "page_count": 12 },
  "annotations": [
    {
      "id": "abc2d3e4f5",
      "page": 3,
      "type": "highlight",
      "rect": [0.0912, 0.4178, 0.6124, 0.4431],
      "color": [1.0, 0.87, 0.31],
      "anchor": { "quote": "the effect of the gate", "text_sha256": "7a1e..." },
      "created": "2026-09-16T18:04:11Z"
    }
  ]
}
```

- **Canonical bytes:** UTF-8 without BOM, LF, two-space indent, keys in ascending code-point order,
  geometry rounded to at most 3 decimals, timestamps at second precision. `check` fails a file that
  is not canonical, so a two-byte edit is a two-line diff and never a reformatting storm.
- **Unknown keys are preserved**, which is how a newer writer stays readable by an older reader.
- **Deletion is a tombstone** (`"deleted": true`), because three-way merge on a JSON file only works
  if removal is visible.
- **View state is excluded on purpose** (page, zoom, sidebar): it changes on every interaction and
  would poison every diff.
- **Staleness is computed, not guessed:** the document fingerprint is the strong signal, the page
  count the structural one, and the tool says which fired.

```bash
python3 tools/sidecar-fmt.py check tests/fixtures/sidecar   # what CI runs today
```

---

## Backend Support

```
                      +---------------------------+
                      |        pdfcore (C)        |
                      |  document model + IR      |
                      +-------------+-------------+
                                    |   include/pdfcore/backend.h (vtable, R-M3)
        +---------------------------+---------------------------+
        |                           |                           |
 +------v-------+            +------v------+             +------v-------+
 |    mupdf     |            |    null     |             |   pdfium     |
 |  default     |            |  contract   |             |  evaluation  |
 |  AGPLv3      |            |  fixture    |             |  Apache-2.0  |
 +--------------+            +-------------+             +--------------+
```

- **`mupdf`** - the shipping backend: rendering, text, annotations, forms, signing, journal-based
  state. Vendored with a patch ledger
  ([ADR-0004](adr/0004-dependency-management-and-supply-chain.md)).
- **`null`** - a real, feature-limited backend that exists so the port is exercised by two
  implementations and so a broken boundary fails CI instead of being discovered at release.
- **`pdfium`** - kept as an evaluation, not a promise: its annotation API is marked experimental and
  its render path differs in kind (`FPDF_ANNOT` excludes widget and popup annotations), which is
  exactly the asymmetry the seam exists to absorb.

---

## Privacy & Local-First Model

- **Default-deny network.** No socket is opened to satisfy a preference, a check for updates, or a
  font lookup; operations that may reach a destination are individually switchable and disclose what
  leaves.
- **Your files stay on disk.** The document, its annotations and the settings are files; there is no
  proprietary store and no cloud to migrate to.
- **No telemetry, no crash uploads.** A crash writes a local report and shows the path.
- **No account, ever.** Nothing in the product requires an identity.
- **Signed, not trusted blindly.** v1 verifies the local chain and labels a locally produced
  signature `PAdES-B-B (local)`; online revalidation is an action, not a background default.

---

## Non-Goals

No cloud, sync, accounts or collaboration; no built-in chat or "AI assistant" (local-agent features
exist elsewhere; here the answer to "I want to script this" is the CLI and the library); no reflow
reading mode that changes what the page is; no XFA; no PDF/A writer claims beyond validation
reporting; no Electron or WebView2; no telemetry; no admin rights; no commercial certificate
authority or timestamp service in v1.

---

## Current State

**Pre-code: the repository contains the plan, the specification and the gates - not the product.**

- 47 files in the tree: 11 ADRs (10 accepted, ADR-0006 superseded by 0008), `docs/kickoff.md` with
  milestones M0-M6 and their exit criteria, the first living `SPEC.md` (14 requirements: 4 already
  enforced by committed tools, 10 honestly `pending`), the sidecar schema and its normative fixture,
  six checks plus a runner, the `gates` workflow, the PR template and the pre-commit hook,
  `AGENTS.md`, `CHANGELOG.md` and `LICENSE`.
- `sh tools/check.sh` is green locally; nothing has run in CI yet.
- Known limitations are not bugs but absences: no viewer, no CLI binary, no corpus, no releases, no
  binaries to download. `docs/kickoff.md` states the order they appear in and what gates each.

---

## Roadmap

- [x] Deliberation closed: 11 ADRs recorded (10 accepted), six v1 deltas, four standing gates
- [x] Repository gates written and self-tested
- [x] Annotation sidecar format specified, with schema and normative fixture
- [ ] **M0** - repository becomes real: CI matrix, vendored MuPDF, `pdfcore` C API, page 1 rendered
      on Windows and as a PNG on Linux, Sumatra 3.6.1 **and** 3.7 pre-release baselines measured
- [ ] **M1** - three-day UI spike with the performance and accessibility floor, including its kill
      criterion (if the blit cannot hold 60 fps with the tile cache and DComp visual tree, the stack
      decision is revisited with the spike's numbers)
- [ ] **M2** - D-6: undo/redo as a command log over the IR, public as `pc_txn_*`
- [ ] **M3** - D-4: diacritics, line breaking, font fallback with a golden corpus
- [ ] **M4** - D-2: form validation, tab order, flatten verified by veraPDF
- [ ] **M5** - D-5: editor accessibility (UIA, Narrator, NVDA) as tested requirements
- [ ] **M6** - D-1 and D-3: the sidecar, redaction with proof, portable ZIP + winget, `0.1.0`
- [ ] After 0.1.0: revocation and TSA behind the explicit revalidate action, MSI, signing
      reputation, and the second render backend only if M1's numbers demand it

---

## Documentation & License

| Document                                                          | Purpose                                                          |
| :---------------------------------------------------------------- | :--------------------------------------------------------------- |
| [`docs/kickoff.md`](docs/kickoff.md)                              | Thesis, non-goals, milestones, CI, testing, risks, first 5 PRs   |
| [`AGENTS.md`](AGENTS.md)                                          | Operating rules for agents and humans (the same rules)           |
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

Tyny PDF is released under the **GNU AGPLv3**; there is no contributor licence agreement and no
assignment - your patch stays yours and stays under the same licence. Deliberation notes and
analyses that led to the ADRs were written in Portuguese outside this repository and are not
shipped: the repository is English-only, enforced by `tools/lang-check.py`.

Binaries, when they exist, are published unsigned with the reason stated on the download page
until a signing decision is recorded in an ADR.

Built for [tyny.ca](https://tyny.ca).
