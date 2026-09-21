# AGENTS.md — Guidelines for AI & Human Contributors

**Tyny PDF** — an offline, local-first PDF reader and editor for Windows, built as a **C++20 core
with a C ABI (`pdfcore`)** over **Win32 + Direct2D**, implementing strict **Clean Architecture**
and **SOLID principles** behind seams a script can check.

- **Official Domain:** [https://tyny.ca](https://tyny.ca)
- **App Identifier:** `ca.tyny.pdf` (`python3 tools/naming-sync.py get identifiers.app_id`)
- **Target Platforms:** Windows x64 and ARM64; Linux under WSL2 is where the core is built and
  tested
- **Licence:** AGPL-3.0-or-later, no CLA, no assignment

Sources of truth: `README.md`, `docs/kickoff.md`, `adr/`, `docs/naming.md`,
`docs/coding-standards.md`, `docs/testing-playbook.md`, `src/features/<capability>/SPEC.md`,
`docs/git-workflow.md`, and `docs/release-runbook.md` for anything that produces an artefact.
Re-read the relevant parts before starting any task. Where the two files disagree on day-to-day
style, `docs/coding-standards.md` fills the gap; where they disagree on a rule, this file wins.

**Read the status line before the rules:** this repository is a seed - decisions, a specification
and gates, with no `src/` and no build system yet. Rules that bind source code are written with
the milestone that makes them checkable, and a verification command that currently prints nothing
because the directory does not exist is **not** a pass. Claiming one is a pass is rule 3.

---

## Critical Rules (Never Violate)

1. **Architecture Boundaries (`src/`, binding from M0):** `src/core` includes no Windows header and
   no engine header; the engine is reached only through the versioned vtable declared in
   `include/pdfcore/backend.h`, and every object crossing a seam is copied out into our own value
   types (ADR-0011 R-M1..R-M4).
   _Verification command before declaring a task done:_

   ```bash
   grep -rEn '#include *[<"](windows|windef|unknwn|d2d1|dwrite|fitz|mupdf|pdfium)' src/core src/render
   ```

   _Must print 0 matches. `tools/layering-check.sh` turns the same rule into a gate alongside the
   swap budget, `backend_line_ratio <= 0.15` (ADR-0011 R-M10/R-M11)._

2. **Frontend is presentation only:** the viewer holds no document semantics. Hit-testing, geometry
   reconciliation, undo, redaction and flatten live in `src/core` and operate on the IR, so a bug in
   them reproduces in `tynypdf-cli` on Linux (R-M8, R-M10). UI code that parses a PDF is a defect
   regardless of how correct its output looks.

3. **Zero raw network, zero raw secrets:** no socket is opened for a preference, an update check
   or a font lookup; any operation that may reach a destination asks, discloses what leaves, and
   is individually switchable (ADR-0003). No key, password, token, `.pfx` bytes or absolute
   document path in a log line, a crash report or a fixture. _Verification (must print 0 matches
   once `src/` exists):_

   ```bash
   grep -rEn 'URLDownload|WinHttp|WinINet|getaddrinfo|connect *\(' src 2>/dev/null
   ```

4. **Never claim a check passed without running it.** The PR body carries the exact command and its
   output. Never `--no-verify`, never merge with a red job, never mark a requirement `done` without
   the artefact its `Verification:` line names (`tools/spec-check.py`, ADR-0011 R-M13).

5. **Documentation may not promise what does not exist.** A tool, job, flag, file or binary that is
   not in the tree needs `(planned, PR #n)` in the same paragraph; CI, release and coverage claims
   stay in the future tense until the thing has actually run. `tools/docs-check.py` enforces both
   halves and refuses a document whose code fence it cannot close.

6. **English only in the codebase:** files, code, comments, identifiers, commit messages, PR text,
   issue text, error strings (ADR-0005). `tools/lang-check.py` fails the build on Latin-1 or CP1252
   mojibake and on Portuguese tokens outside the content under test (`tests/**`, `third_party/`).
   Deliberation notes in Portuguese live outside this repository.

7. **No unapproved dependency, build flag or toolchain** (R-M9). A library that would force a
   compiler flag, a second runtime, or a pinned target triple needs an ADR that prices it first. The
   engine arrives as `third_party/mupdf/`, a submodule pinned by a 40-hex commit in
   `third_party/UPSTREAM.toml`, with patches as an ordered series in `third_party/patches/`
   (ADR-0004) - never as commits on a private branch.

8. **Names come from `docs/naming.md`:** never hand-edit a derived identifier or a file under
   `generated/`; edit the table, then run `python3 tools/naming-sync.py write`. The public API stays
   brand-free (`pdfcore`, `pc_`), and a retired brand token fails the build (ADR-0006, ADR-0008).

9. **The sidecar is byte-canonical:** regenerate a `*.tynypdf.json` fixture only with
   `python3 tools/sidecar-fmt.py fix <path>` (ADR-0007). Hand-formatting one turns somebody else's
   two-line diff into a whole-file change.

10. **Line endings are LF and whitespace is owned by `.gitattributes`:** never set `core.autocrlf`
    locally as a workaround and never re-save a fixture; fix the file, with
    `sh tools/canonical-check.sh` proving it holds.

11. **Doc sync is part of "done":** a behaviour change updates the capability `SPEC.md` in the same
    commit; a surface change updates `README.md`, `CHANGELOG.md` and the debt matrix below. Work is
    **not** done while documentation describes a stale state - that is the failure this project was
    seeded to avoid, recorded in `docs/lessons.md`.

12. **Gate integrity:** `sh tools/check.sh` green plus every tool self-test before a turn or
    commit is declared complete. A gate may not be "temporarily" disabled, downgraded to a
    warning, or widened by an exclusion list without an ADR. From M0 the set also requires the
    build and `ctest` to pass.

---

## Commands Matrix

| Purpose                                     | Command                                                           | Location                     |
| :-----------------------------------------  | :---------------------------------------------------------------  | :--------------------------  |
| **Run every gate**                          | `sh tools/check.sh`                                               | `Root` (today)               |
| **Prove the gates can still fail**          | `sh tools/gates-selftest.sh`                                      | `Root` (today)               |
| **Diff and tree hygiene (D1-D8)**           | `python3 tools/diff-scan.py --tree`                               | `Root` (today)               |
| **Move the format gate on a staged diff**   | `sh tools/format-check.sh --staged`                               | `Root` (today)               |
| **Language policy only**                    | `python3 tools/lang-check.py`                                     | `Root` (today)                |
| **Naming drift and retired tokens**         | `python3 tools/naming-sync.py check`                              | `Root` (today)               |
| **Regenerate derived identifiers**          | `python3 tools/naming-sync.py write`                              | `Root` (today)               |
| **One identifier, for scripts**             | `python3 tools/naming-sync.py get identifiers.app_id`             | `Root` (today)               |
| **Sidecar canonical form and schema**       | `python3 tools/sidecar-fmt.py check .`                            | `Root` (today)               |
| **Fix a sidecar fixture**                   | `python3 tools/sidecar-fmt.py fix <path>`                         | `Root` (today)               |
| **Spec shape and requirement traceability** | `python3 tools/spec-check.py`                                     | `Root` (today)               |
| **Docs may only promise what exists**       | `python3 tools/docs-check.py`                                     | `Root` (today)               |
| **Byte stability of the tree**              | `sh tools/canonical-check.sh`                                     | `Root` (today)               |
| **Formatting gate**                         | `sh tools/format-check.sh`                                        | `Root` (today)               |
| **A gate's own self-test**                  | `sh tools/gates-selftest.sh` or `--self-test` on one tool         | `Root` (today)               |
| **Configure and build the core**            | `cmake --preset linux-core && cmake --build --preset linux-core`  | `Root` (today)              |
| **Run unit and contract tests**             | `ctest --preset linux-core --output-on-failure`                   | `Root` (today)              |
| **Cross-build the Windows target**          | `sh tools/build-mupdf-windows.sh && cmake --preset win-cross-x64` | `Root` (today)              |
| **Run the ADR-0010 probes in CI's order**   | `tools/win-probe/build.sh`                                        | `Root` (today)              |
| **Run the viewer through WSL interop**      | `./build/win-cross-x64/Release/tynypdf.exe ~/tmp/sample.pdf`      | Windows process (M0)        |

Every row marked `(today)` was run for the seed and is reproducible in a clean checkout; the rows
marked `M0` name artefacts that do not exist yet, which is why they cite the PR that writes them.
The build rows became `(today)` in PR #2 together with `CMakeLists.txt`, `CMakePresets.json`,
`tools/win-probe/` and the CI matrix; the `tynypdf.exe` row stays `M0` because the product binary
is delivered by story 1.4/1.5.

---

## Architecture & Layer Responsibilities

One rule decides where a line of code lives: the dependency arrow always points inward, toward
`src/core`, and never toward a vendor or an OS (ADR-0002).

```
tyny-pdf/
|-- include/pdfcore/            # PUBLIC ABI: pdfcore.h, backend.h, sealer.h, print.h, sidecar.h
|-- src/
|   |-- core/                   # ENTITIES + USE CASES: document model, IR, undo, sidecar semantics.
|   |                           # no Windows header, no engine header
|   |-- backends/               # ADAPTERS: mupdf/, null/ - the only code allowed to know an engine
|   |-- render/                 # swapchain, tile cache, cachemap - presentation, never parsing
|   |-- os/win32/               # window, dpi, uia, clipboard, policy - the only OS-specific tree
|   |-- sealer/                 # pkcs7_local/, stub/          - signature production port
|   |-- print/                  # win32_gdi/, preview/
|   |-- features/               # VERTICAL SLICES: one directory per capability, each with SPEC.md
|   |-- app/                    # tynypdf.exe - composition root, wiring only, no logic
|   `-- cli/                    # tynypdf-cli - headless twin, JSON/JUnit reporters, exit codes
|-- tests/                      # unit/, contract/, conformance/<iso-clause>/, approvals/, fixtures/
|-- docs/                       # kickoff, naming, dev-environment, git-workflow, lessons, a11y/
|-- adr/                        # one file per decision, each with a "Cost of swapping" section
`-- tools/                      # the gates: check.sh and the checks it runs
```

### Layer Rules

- **`src/core`** owns meaning: geometry in document units, annotation lifecycle, transaction log,
  sidecar encode/decode. It compiles and is tested on Linux, with the `null` backend, and nothing in
  it may include `<windows.h>` or `mupdf/`.
- **`src/backends/<name>`** implements the vtable, translates engine errors into `pc_status` at one
  bridge, copies results into our types, and declares capabilities (`PC_CAP_*`) instead of being
  asked to guess them (R-M5). Adding a method here without a `null` counterpart is a boundary bug.
- **`src/render` and `src/os/win32`** own pixels and the window: DPI awareness, swapchain, cache
  policy, UIA providers. They call inward and never reconcile geometry or mutate a document.
- **`src/features/<capability>`** is where a story lands: the slice holds its UI wiring, its
  `SPEC.md`, and its tests. There is no `src/utils` and no shared mutable singleton.
- **`include/pdfcore`** is the only stable surface. A change here is an ABI event: it needs the
  version bump, the migration note, and the golden error-code test.

---

## Conventions & Standards

House style below is deliberately cheap to reverse (R-M12), so it is documented here rather than in
an ADR; anything that changes a boundary, a format or the ABI belongs in an ADR.

### C - the public ABI

- **Symbols:** `pc_` for functions and typedefs, `PC_` for macros and enum constants; error codes
  are `pc_status` values and never a bare `int` (ADR-0003 section 1).
- **Files:** headers under `include/pdfcore/`, lower snake case, no abbreviation invented for taste.
- **Ownership:** every function that returns an allocated object names the release function in its
  doc comment; a function that does not allocate says so. Ownership is a comment only where a type
  cannot express it.
- **No exceptions, no RTTI across the boundary;** the bridge converts engine throws at one point
  (`exception_bridge.h`) and nothing else may catch.

### C++ - the implementation

- **Naming:** files `snake_case.cc` / `snake_case.h`; types `PascalCase`; functions and variables
  `snake_case` with a trailing underscore on members; test files `test_<subject>.cc`.
- **Errors:** `std::expected<T, E>` internally, mapped to `pc_status` at the boundary; no
  `assert()` for input that a document can control - a hostile PDF must produce a reported failure,
  not a trap (ADR-0003).
- **Comments:** explain why, or cite the requirement (`R4.1`); no commented-out code, no
  aspirational API, no header that exists only as a declaration in one file.

### CMake, warnings, CI

- One preset per target (`linux-core`, `win-cross-x64`, `windows-msvc`); no absolute host paths and
  no floating versions - the engine pin is a 40-hex commit and dependencies are a locked Conan graph
  (ADR-0004).
- Our targets build with warnings as errors on both toolchains, including the MinGW job
  (ADR-0010); a suppression needs a comment naming the diagnostic id and why it is wrong here.
- A CI job that cannot fail is not a check: every new gate ships with the `--self-test` that proves
  it detects its own violation.

### Requirements and commits

- A requirement is one line in a capability `SPEC.md`: `### R<n>.<m>`, an EARS keyword, and a
  `Verification:` target that names a test file or a tool (R-M13).
- Commits are Conventional Commits (below); a PR cites the ids it closes in the trailer.

---

## Testing Strategy

- **The eight gate self-tests first:** a check that cannot detect its own violation is decoration.
  Run with `sh tools/gates-selftest.sh`; CI runs the same list in `gates.yml`.
- **Unit per capability (`tests/unit/test_<subject>.cc`, from M0):** core semantics on the `null`
  backend, so a test never depends on MuPDF to make its point. Run with `ctest --preset linux-core`.
- **Contract suite per backend (`tests/contract/`, from M0):** the same assertions run against every
  installed backend, so an unsupported capability is reported as unsupported and never as empty.
- **Approval and golden tests (`tests/approvals/`, from M0):** sidecar bytes, rendered page digests,
  CLI JSON; a reformat or a rounding change has to be argued in the diff.
- **Conformance corpus (`tests/conformance/<iso-clause>/`, from M2 onward):** clause-keyed PDFs
  graded against veraPDF as the oracle, not against our own expectation of what the spec says.
- **Fuzzing (libFuzzer, nightly):** the sidecar parser, the font and text paths, and the engine
  bridge; corpus in-tree, seeds minimized.
- **A requirement is done when the artefact its `Verification:` line names exists** -
  `spec-check.py` says so mechanically, and the seed's honest answer today is 10 pending ids.

---

## Commit & Git Standards

Follow **Conventional Commits**, one logical change per PR (ADR-0009):

- `feat:` new capability or user-facing behaviour
- `fix:` defect, with the failing test in the same PR
- `refactor:` structure changes, behaviour does not
- `perf:` measured improvement (before/after number in the body)
- `docs:` README, ADR, SPEC, `AGENTS.md`
- `build:` / `ci:` / `chore:` toolchains, presets, gates, dependencies

Branch per PR, squash-merge into `main`, no `develop`; rebase on your own branch then
`git push --force-with-lease`; never force-push a shared branch. PR trailer:

```
Requirement: R3.1 R3.2
Check: python3 tools/spec-check.py -> 1 specs, 14 requirements, 0 orphans
Evidence: tests/unit/test_sidecar_lock.cc (2 new)
Generated-by: agent (implementation); verified-by: owner (criteria and merge)
```

---

## Known Technical Debt (Traceability Matrix)

Nothing here is "resolved" by assertion; each item stays until the number that measures it moves. Do
not silently add an item; flag it in the PR and open it here with the number that measures it.

1. **The build now produces a product, but only a thin one:** PR #2 landed the build system and
   `tools/win-probe/`; story 1.4 added `tynypdf-core`, the `null` and `mupdf` backends,
   `tynypdf-cli` and the contract tests, so a green matrix run builds real targets and the
   `windows-mingw-cross` job now cross-compiles MuPDF itself and links it into `tynypdf-cli.exe`
   (fz symbol and import table checks in the job). Epic 3 landed `src/core`'s first C++ (doc IR,
   geom, sidecar encode/decode, sha256, per-document MuPDF locks) and closed the backend swap
   budget (item 8). What is still missing is everything above the IR: no viewer, no undo, and
   Story 1.5's baseline is blocked (item 5), so "builds" is not yet "does the job".
2. **Only the doc gates and the matrix's first runs are recorded:** `gates` ran green on `main`
   since run `35170901622` (at `94f1950`) and `35171544840` (at `0d64999`); the build matrix
   arrived with PR #2 (`2ec9977`) and its first all-four-green run is `35179132038` (head
   `5dc5767`). The runs that built a target are still few; treat them as the start of the record,
   not the record. No status badge may appear in `README.md` until a job that can fail on bad code
   has failed on bad code.
3. **The four toolchain claims are measured, not open:** PR #2 ran `tools/win-probe/` and moved
   the ADR-0010 assumption table into results (headers link and factories create; static runtime
   holds in import tables; the host/cross symbol tables are identical; a hardware GPU adapter and
   a `D3D_FEATURE_LEVEL` were observed through WSL interop). What stays open is version drift of
   the pinned toolchain - the hash gate catches it, CI verifies it each run.
4. **Four SPEC requirements have no artefact** (R15.1-R15.4); they are all in the tables/text
   capability that story 3.5 deliberately shows as `PC_ERR_CAPABILITY` (R-M5), and R16.1 (the
   capability contract) is the artefact that guards them. The rest of the original 17 were retired
   by epic 3 stories (R14.1-R14.3, R2.2, R2.3, R3.1, R3.2, R4.1, R4.2, R5.1, R5.2, R6.1, R6.2).
   `tools/spec-check.py` prints the list on every run, so the debt cannot be forgotten by scrolling
   past it.
5. **`tynypdf-cli sidecar gc` is out of v1:** tombstones accumulate until that command exists
   (ADR-0007 records the decision and its cost).
6. **The competitive baseline must be re-measured per milestone:** the seed measured SumatraPDF
   3.6.1 as stable and 3.7 pre-release as the moving edge, and the 3.7 changelog already shipped
   two of our six deltas in different form - so `tests/baseline.json` covers both channels
   (kickoff M0.4).
7. **Two toolchains on one source is a permanent tax** (ADR-0010 negative consequence): warning
   sets, header coverage and link differences will keep producing `build:` PRs that look like
   noise.
8. **The swap budget is under the 0.07 target but the core is still thin:**
   `tools/layering-check.sh` now measures `backend_line_ratio = 0.0683` against the 0.07 target set
   by story 3.5 (query: `sh tools/layering-check.sh --strict`, 2026-09-21), down from 0.1014 at the
   epic 3 baseline. The fall came from trimming backend non-vtable code (per-doc lock helpers
   inlined into the vtable bodies, `exception_bridge.h` shrunk), not from core growth; the
   budget starts to mean something only when `src/core`'s C++ outgrows it.

---

## Operational Discipline & Debugging Guidelines

- **Read the owner document first.** Before touching a seam, read the ADR that owns it and the
  capability `SPEC.md`; the header comment of `pc_*` is normative, the implementation is not.
- **Reproduce headless before touching the UI.** If `tynypdf-cli` cannot reproduce it, it is a
  presentation bug in `src/render` or `src/os/win32`; if it can, fix it in `src/core` where the test
  will keep it fixed.
- **Measure before blaming the engine.** The concurrency story that shaped R-M2 came from a
  binding's process-wide lock array (13.3x threaded versus 3.3x multiprocess on a 6-core box), not
  from the library itself; the number existed only because someone measured.
- **A measurement in a document carries its query, its unit and its date.** A number without them is
  a rumour, and this project has already been burned once by a table whose label did not match the
  query that produced it (`docs/lessons.md`).
- **Isolate the environment before suspecting the code.** WSL interop, `/mnt/c` filesystems and a
  second Windows-side checkout of the same tree account for a class of bugs that cannot reproduce
  otherwise; the working copy lives in Linux filesystems only.
- **Re-wrapping is a content change until proven otherwise.** Any tool that re-flows this file has
  to leave the word sequence and every fenced block byte-identical; check both before committing,
  because a swallowed continuation line reads fine to a person and silently detaches a rule from
  its list item.
- **Clean workspace:** no build directories, no `/mnt/c` copies, no reflowed fixtures, no `.env`, no
  locally generated `generated/` files left uncommitted after `naming-sync.py write`.
- **Out of scope is said, not smuggled:** name it in the PR, open an issue with the measured number,
  and if the detour was a mistake, add an entry to `docs/lessons.md`. Quietly widening scope,
  disabling a gate, or writing a document no command executes are all violations of rule 5.

---

Built for [tyny.ca](https://tyny.ca).

