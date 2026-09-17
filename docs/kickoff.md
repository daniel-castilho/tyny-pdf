# Tyny PDF - project kick-off

Owner: the person who opens the pull requests. This document is the operational brief: what we are
building, what we refuse to build, in what order, and how each claim is measured. It is not a
business plan and not a vision statement. Where it names a number, that number is a gate a machine
can evaluate; where a number is not yet measurable, the item says who goes and measures it.

Everything below is derived from 11 accepted ADRs and a decision log of nine rounds of
deliberation. If this document and an ADR disagree, the ADR wins and this file is a bug.

---

## 1. Thesis

Most PDF software has quietly become a client for a service. Tyny PDF is the opposite bet: a native
Windows reader and editor whose value is that it does not need you, does not watch you, and keeps
your marks in a file you can inspect, diff, keep under version control and open in ten years.

Three falsifiable claims carry the bet, and each has an owner and a test:

| Claim | How it is falsified |
| --- | --- |
| C1 - it is genuinely offline | `tests/network/`: a run with no route to any destination but localhost; any connect() attempt fails the build. The denial is default-deny per operation (ADR-0003), not a settings checkbox that defaults to on |
| C2 - annotations survive in a form a human can audit | `*.tynypdf.json` is byte-canonical, schema-validated, and diffable (ADR-0007); the fixture test asserts a two-byte change is two lines of diff, not a binary blob |
| C3 - it is good enough to replace the installed default | measured against SumatraPDF 3.6.1 (latest stable, 2026-04-06) *and* 3.7 pre-release on the same corpus and machine (D12, corrected by the 2026-09-16 audit) |

C1 and C2 are cheap and mostly already provable in this repository. C3 is where projects like this
die, so it is the only claim with an explicit kill criterion (section 10).

**Correction recorded 2026-09-16 (audit).** C3 was first written as "the six deltas exist because
the incumbents do not do them". That is falsified by SumatraPDF's own changelog for the 3.7
pre-release, which already has: an Edit PDF mode with the full annotation set and an annotation
browser; redaction marks with **Apply Redactions**; **Undo/Redo built on MuPDF's journal**
covering annotations, form fields and applied redactions; **Sign Document** from a `.pfx`/`.p12`
*or from the Windows certificate store*, with Document Properties showing the hash, LTV-enabled
flag, RFC 3161 timestamp, PAdES level and the EU Trusted List; about twenty command-line tools
(`sumatrapdf-tool.exe`, portable EXE included); and an AI chat sidebar driven by a *local* agent
CLI. The bet is still alive, but its sentence changed: we are not the only offline editor any
more, we are the one whose edits are auditable files and whose document model is a published
library.

## 2. Who it is for, and the effect we want

Primary: people who handle PDFs as documents rather than as a workflow - students, researchers,
small firms, translators, anyone in a jurisdiction where "upload to the cloud to sign" is not an
option. Portuguese first-class support is not a courtesy; it is a requirement (D4 diacritics
correctness, ABNT2 keyboard, pt-BR spell-check hooks) because the incumbents treat it as an
afterthought.

The effect measure we accept: a reader recommends the tool to a colleague because it did the one
thing the colleague needed and did not ask for an account. That is proxied by retention of the
annotation workflow (documents opened where a sidecar already exists, per install, reported only if
the user opts in - which is off by default; see ADR-0003) and, more honestly, by issues and
discussion in the repository, because offline software has no telemetry to point at.

## 3. Non-goals (the list that keeps the design small)

- No cloud, no sync, no accounts, no collaboration. Not "later"; the seams stay clean enough that
  somebody else can build it on top of `pdfcore`.
- No built-in chat, "AI assistant", OCR as a headline feature, or document analysis that phones
  home.
- No reflow "reading mode" as a different document model; a viewer must not change what the page is.
- No XFA support. Dynamic XFA is a licensing minefield and a rendering swamp; we detect and say so.
- No PDF/A writer claims beyond validation reporting; veraPDF is the oracle, we do not compete with
  it.
- No viewer inside a browser, no Electron, no WebView2, no plugin host.
- No telemetry. Crash reports are local files with an explicit "send" that does not exist in v1.
- No installer requirement for the portable path; no admin rights, ever.
- No signing certificate or timestamp authority in v1 (D7b): we verify and we sign locally with a
  `.pfx` the user owns, labelled `PAdES-B-B (local)`; online revalidation is a button the user
  presses knowingly.

## 4. What v1 is

A Windows viewer (`tynypdf`), a headless CLI (`tynypdf-cli`), and the library they share
(`pdfcore`), shipping six deltas. After the 2026-09-16 audit, each delta is justified by *mechanism*
rather than by "the incumbent cannot do it" - the mechanism is what a user can check. Order
stays the ladder that retires the riskiest assumption first (D-6 to D-3, round 6):

| # | Delta | Why it is in v1 | Primary seam it forces |
| --- | --- | --- | --- |
| D-6 | Undo/redo over an IR command log | Sumatra 3.7 has journal-based undo inside its app; ours is a public API (`pc_txn_*`) so the CLI and third parties get it too, and doing it first proves the IR can carry semantics | `src/core/doc` |
| D-4 | Diacritics, line breaking, font fallback | correct breaks and caret movement over combining marks, with a golden corpus in pt-BR; a fallback that picks a face per run, or reports the missing glyph instead of a tofu box | `src/core/text` |
| D-2 | Form validation and flatten | 3.7 fills forms; the delta is the rules (required, format, range, export values), tab order matching the visual order, and flatten verified by veraPDF rather than by eye | `src/core/form` |
| D-5 | Accessibility of the editor (UIA) | readers expose UIA for viewing and read-aloud; editing is where they fail. EN 301 549 clauses become requirements with tests, which is also the procurement gate | `src/os/win32/uia` |
| D-1 | The annotation sidecar on disk | the differentiator that survives the audit: nobody else keeps marks in a file you can diff, review and version instead of only inside the PDF | `src/features/sidecar` |
| D-3 | Redaction with proof | 3.7 already applies redactions; the delta is the *proof* - a text-extraction audit of the output as an artefact, not a visual check | `src/core/redact` |

The library and CLI are not extras: they are how the project stays honest. The CLI is the
Linux-side testable face of everything except presentation (ADR-0010), and a public C API is what
forces the boundaries to be real (ADR-0011 R-M1..R-M4).

## 5. Shape of the system

- `src/core` - document model and intermediate representation. No OS widgets, no networking, no
  engine headers. Undo, flatten, redaction and the sidecar operate on the IR, never on engine
  handles (R-M8).
- `src/backends/mupdf` + `src/backends/null` - the engine, behind a versioned C vtable
  (`pc_backend.h`) with a capability struct (R-M3, R-M5). The `null` backend is built and tested in
  CI so the boundary cannot rot.
- `src/render`, `src/os/win32` - Direct2D over a DComp/D3D11 swapchain, Per-Monitor V2 DPI, UIA,
  clipboard, print. Contains no parsing and no annotation mutation (R-M10).
- `src/app` - composition root. Thin on purpose: if it grows logic, the review says no.
- `src/cli` - `tynypdf-cli`, exit codes 0/1/2/3, JSON and JUnit reporters, the transport for CI.
- Public surface: `include/pdfcore/*.h`, the only headers a consumer may use.

Two numbers make "maximum modularity" reviewable instead of rhetorical (ADR-0011):
`backend_line_ratio <= 0.15` (engine-coupled lines outside vtable implementations, over total
engine-facing lines) and the layering check that fails when the render or OS layer contains
document semantics. Both are reported by CI on every pull request.

Language and toolchain: C++20 with a deliberately small subset - no exceptions across the public
API, no RTTI, no `std::function` in hot paths, error model per ADR-0003; C for the public headers.
Clang and MSVC compile the same CMake targets; LLVM-MinGW is the local cross toolchain
(ADR-0010). Dependencies per ADR-0004: MuPDF vendored with a patch ledger, Conan 2 lockfile for
support libraries, nothing added without an ADR that states its cost (R-M9).

## 6. Milestones

No time boxes (D15); gates instead. A milestone is reached when its exit criteria are green in CI,
not when the calendar says so.

### M0 - the repository becomes real

Exit criteria:
1. `main` protected (ADR-0009): required checks, no force push, auto-merge on.
1b. The `gates` workflow ships with PR #1 and is green: the checks plus their self-tests, so CI
   proves the gates can still fail (`/.github/workflows/gates.yml`). PR #2 extends it to the
   eight checks and six self-tests: `check.sh`'s six, the inline line-ending check, and
   `tools/format-check.sh` (with its own `--self-test`).
2. CI matrix green: `core-linux`, `windows-mingw-cross`, `windows-msvc` - wired by PR #2 with
   `CMakeLists.txt`, `CMakePresets.json`, `tools/win-probe/` and the toolchain hash. gtest
   consumption and the contract suite land with story 1.4; `layering` arrives with
   `tools/layering-check.sh` (PR #4).
3. A window opens on Windows that renders page 1 of a PDF through `src/backends/mupdf`, and the
   same CLI command renders to PNG on Linux; the `null` backend passes the contract suite.
4. `tests/baseline.json` holds SumatraPDF numbers measured on the reference corpus and machine,
   for **3.6.1 (what users have) and 3.7 pre-release (what the real bar is)**, with the measurement
   script committed (D12, corrected by the audit).
5. Corpus submodule pinned at a commit hash; the intake plan is written (D13).
6. `NOTICE`, `docs/references.md` and the SBOM stub wired into the build. The licence text itself
   is not an M0 item: `LICENSE` (AGPL-3.0-or-later, FSF text) ships with PR #1, because a public
   repository without a licence is a legal ambiguity for every contributor from day one.

### M1 - the UI spike, three days, with a kill criterion (D6b)

The spike answers one question: can a hand-written Win32/Direct2D surface hold the performance and
accessibility floor without a framework? Measured, not estimated:

| Criterion | Target |
| --- | --- |
| Blit a 4000x3000 page region | 60 fps sustained, no frame over 33 ms p99 |
| RSS at 1000 pages open, 3 tiles each | <= 250 MB, and it must not grow when scrolling back |
| Cold start to first painted page | <= 300 ms on the reference machine (vs measured Sumatra) |
| DPI | Per-Monitor V2, no bitmap stretch at 150%/200% |
| Keyboard and screen reader | every control reachable; Narrator and NVDA announce page, zoom, focus |
| Text input | ABNT2 and pt-BR composition in a text field, correct caret movement over combining marks |
| Pinch/zoom, wheel | 1:1 tracking, no gesture lag over 16 ms |

Kill criterion: if the swapchain or the blit cannot hold 60 fps after the tile cache and the DComp
visual tree exist, we do not "optimise harder" - we revisit D3 with the spike's numbers in a new
ADR. The spike is time-boxed at three days precisely so that the revisit happens with data instead
of with hope.

### M2 - D-6, undo through the IR

Exit: the command log is the only undo mechanism; `pc_txn_*` in the public API; a fuzzed document
with 10k operations undoes and redoes to byte-identical state; the CLI has `tynypdf-cli txn replay`;
`src/render` contains no undo logic (asserted by `tools/layering-check.sh`, PR #4).

### M3 - D-4, text correctness

Exit: a golden corpus of pt-BR/ro/de/vi line breaks and hyphenation decisions; combining-mark
handling covered by the D-6 replay test on text edits; font fallback chooses a face that renders
every code point of the run or reports the missing glyph explicitly.

### M4 - D-2, forms

Exit: AcroForm validation (required, format, range, export values), tab order that matches the
visual order for the conformance subset, flatten that keeps appearance streams and is verified by
veraPDF for the PDF/A conversion path, and the field-walker contract test passing for both backends.

### M5 - D-5, editor accessibility

Exit: UIA patterns implemented for canvas, annotation list and form fields; Narrator and NVDA
scripts (manual, recorded in `docs/a11y/`) with the exact announced text; EN 301 549 clauses mapped
to requirements in `SPEC.md`, and `tools/spec-check.py` reporting zero pending items for the
capability.

### M6 - D-1 and D-3, then 0.1.0

Exit: the sidecar passes the schema and canonical-bytes gates and the four pending verification
artefacts in `src/features/sidecar/SPEC.md` are green; redaction removes content from the document
stream (verified by a text-extraction audit on the redacted output, not by visual inspection);
portable ZIP, MSI, winget manifest, `docs/SWAP-CHECKLIST.md` filled in, changelog generated from
commit types. Announcement is gated on the ADR-0008 open items (DNS records, trademark clearance).

## 7. The four standing gates (D15)

1. **Per-delta gate** - a delta is not done while any of its `SPEC.md` requirements is pending
   (machine-checked, `tools/spec-check.py`).
2. **Reality gate** - before a delta starts, its main assumption is measured on real data: the
   baseline numbers, the corpus, or a throwaway prototype. No measurement, no delta.
3. **Downgrade gate** - a feature may be cut or simplified only by editing the `Out of scope`
   section of the relevant `SPEC.md` in the same PR, so the loss is visible and reviewable.
4. **Ergonomics gate** - `build_clean_time_s <= 180` for the core+tests build on the reference
   machine, and the full `tools/check.sh` under 20 s. Developer patience is an architectural
   constraint; when it breaks, the build structure is a bug.

## 8. Testing strategy

- **Unit** - gtest for semantics, one file per requirement cluster, each test citing its `R<n>.<m>`
  id because the gate demands it (a test that is not traced is not evidence).
- **Contract** - the same suite against `mupdf` and `null` backends; the null backend is not a mock
  for show, it is the proof that the vtable is the whole interface.
- **Conformance** - clause-keyed corpus (`tests/conformance/<iso-clause>/`), each file documented
  with the clause and the expected outcome, so a failure names a requirement, not just a diff.
  veraPDF is the oracle for validity and UA claims; we do not grade our own homework.
- **Approval tests** - structured dumps of the IR (annotations, form fields, geometry) compared to
  golden files in `tests/approvals/`, with a documented accept flow.
- **Property tests** - sidecar round-trip and canonicalisation (idempotence, key order, tombstone
  merge) and undo replay, with fuzzed inputs.
- **Fuzz** - libFuzzer targets for parsing, sidecar loading and the sealer; 4 hours nightly with
  the corpus as seed; a crash is a bug with a repro file committed next to it.
- **Performance** - the M1 table plus a cold-start and memory trend job per release on the reference
  machine; trends are recorded in `tests/baseline.json` next to Sumatra's numbers.
- **Accessibility** - manual scripts in `docs/a11y/` with exact expected narration; UIA tree
  assertions where programmable.
- **Privacy** - no-network proofs: `strace`/ETW capture during a full open-edit-save run; any
  outbound socket is a build failure.
- **Windows-specific** - DPI, spooler, Direct2D timing, UIA, IME. These run on the Windows CI job
  and on the physical machine, never assumed from the Linux side (ADR-0010).

## 9. How a story moves (living specifications, per ADR-0002 and ADR-0009)

1. Draft or amend the capability `SPEC.md`: EARS requirements, `Out of scope`, `Verification:`.
2. Open `feat/<delta>-<slug>` from `main`; write the failing test first and see it fail.
3. Implement. Keep the diff to ~400 lines; stack a second PR if it does not fit.
4. `sh tools/check.sh` locally; CI decides.
5. PR body carries the trailer (`Requirement:`, `Check:`, `Evidence:`, `Generated-by:` /
   `verified-by:`). Squash-merge; `main` is now releasable.
6. If something was learned the hard way, it goes into `docs/lessons.md` in the same PR, not in a
   retrospective nobody reads.

## 10. Risks worth naming, with the tripwire

| Risk | Tripwire (what makes us see it early) | Response |
| --- | --- | --- |
| Hand-written UI costs more than predicted | M1 kill criterion; or any UI bug that takes a day to diagnose because the framework we refused would have fixed it | Revisit D3 with the spike's numbers in a new ADR; Direct2D-on-a-DComp-visual-tree is a small, tested surface, but a second backend for Skia/GPU is legitimate |
| MuPDF AGPLv3 is the whole product's licence | a contributor complaint, or a corporate fork request | Already accepted: the product is open source (AGPLv3); the paid path, if any, is support and packaging, never a closed core |
| Conformance corpus becomes a pile of PDFs | corpus PRs without a clause key or without an expected outcome | The corpus submodule only accepts files with a manifest entry; `tools/spec-check.py` fails an orphan |
| The incumbent ships our delta before we do | Sumatra 3.7 pre-release already has annotations, undo, Apply Redactions, signing from the store and a CLI; re-run the gap analysis against the pre-release notes at every milestone | Restate the delta by mechanism (this section, §4), and keep the auditability claim testable rather than rhetorical |
| Solo review capacity, amplified by AI-generated diffs | PRs over 400 lines; a merge with a red job; a "verified" claim with no artefact | ADR-0009 rules 2 and 9; auto-merge queues small PRs so the incentive to be small is time, not virtue |
| Performance target unreachable on low-end hardware | trend job shows p99 frame time above 33 ms on the reference machine; RSS growth on scroll | Tile cache sizing and the memory budget struct are the levers; D6b's numbers become the contract in `SPEC.md` |
| Two toolchains drift | `windows-msvc` green while `windows-mingw-cross` red, or vice versa | Both are required checks; MinGW-only failures get fixed by removing the dependency on the vendor extension, not by adding a `#ifdef` |
| Signature verification without revocation reads as overclaiming | user trust; one bug report of "it said valid but the certificate was revoked" | The UI text is a string under review: it says what was verified, and "revalidate online" is the only way to the network |
| The project never ships because the standards are too high | M2 not reached while M1 polish continues | The gates are exit criteria, not perfection criteria; a milestone may be declared "reached with a downgrade" through gate 3, never silently |

## 11. First five pull requests (this is the actual kick-off)

1. `chore/seed-repository` - the 40 files of this seed: `README.md`, `AGENTS.md`, `CHANGELOG.md`,
   `LICENSE` (AGPL-3.0-or-later), the 11 ADRs,
   `docs/{kickoff,naming,git-workflow,dev-environment,lessons,coding-standards}.md`,
   `docs/sidecar.schema.json`, `src/features/sidecar/SPEC.md`, `tests/fixtures/sidecar/`,
   `tools/*`, `generated/*`, `.github/{PULL_REQUEST_TEMPLATE.md,workflows/gates.yml}`,
   `.gitattributes`, `.editorconfig`, `.githooks/pre-commit`, `.gitignore`.
   Exit: `sh tools/check.sh` green in CI (it is green locally), all 11 ADRs indexed by
   `docs-check.py`, and the copy includes the dotfiles - `cp -r kickoff/* .` does not,
   `rsync -a` does.
2. `chore/ci-gates` - the five jobs in M0.2, artifacts, caching, `just`/`make` wrappers for the
   local loop. No status badges (build, tests, coverage, adopters) until the job that produces
   them exists and can fail; `README.md` carries static stack badges only, and `docs-check.py`
   forbids it from promising anything else. Exit: a deliberately broken fixture makes the docs
   job red, proving the gate is wired.
3. `chore/baseline-sumatra` - reference machine spec, corpus pin, `tools/bench-measure.sh` (new in
   this PR), and `tests/baseline.json` with Sumatra 3.6.1 numbers for open, first paint, scroll,
   search and RSS. Exit: numbers reproducible within 10% on a second run.
4. `chore/layering-and-deps` - new in this PR: `tools/layering-check.sh`, `tools/deps-refresh.sh`,
   `tools/patch-report.sh`, `tools/sbom.sh`, plus the vendored MuPDF tree with its patch ledger.
   Exit: the ratio and layering checks run on a tree that has no code yet and are wired to fail
   when it does.
5. `feat/m0-render-page-1` - `pc_doc_open`, `pc_doc_page_count`, `pc_page_render` in the C API, the
   MuPDF backend, the `null` backend, the CLI `render --page N --out x.png`, the Win32 window that
   blits the same bytes. Exit: M0.3 green, contract suite green for both backends.

M1 (the spike) runs in parallel with PRs 3 and 4, because its three days are cheapest while nobody
is blocked on it.

## 12. Open questions, with a default so nobody waits

| Question | Default until decided | Deadline |
| --- | --- | --- |
| `tyny.ca` DNS: records for `www`, `releases`, `updates` | GitHub Pages plus `github.com` release assets; the reverse-DNS id needs no DNS | before the first announcement |
| MSI vs winget-only distribution in 0.1 | portable ZIP plus winget manifest; MSI when an enterprise ask exists | M6 |
| ARM64 in 0.1 | build it, do not advertise it; the cross toolchain supports AArch64, the test machine does not | M6 |
| Skia as a second render backend | no; revisit only if M1's kill criterion fires | after M1 |
| Portuguese documentation of the UI | UI strings English + pt-BR resource file from D-1 on; docs in English | M6 |
| Trademark clearance | name is used as-is with the risk documented in ADR-0008 | before announcement |
| Whether the sidecar ever holds view state | never; ADR-0007 R6.1 | permanent |
| Sign from the Windows certificate store, as 3.7 pre-release does | v1 stays `.pfx`-only and says so in the UI; the store path is D9's first amendment candidate | before M6, owner decides |
| Local agent AI in the viewer, as 3.7 pre-release offers | no: default-deny network in-process (ADR-0003) and scriptability through `tynypdf-cli` and `pdfcore` instead | permanent unless an ADR reverses it |

## 12b. Owner-only actions (a pull request cannot do these)

1. Tick the six branch-protection settings in `docs/git-workflow.md` before PR #2 merges. The list
   is short because the repository is public, which is what makes required checks free.
2. Create `daniel-castilho/tyny-pdf-corpus` (empty, with a `MANIFEST.md` explaining the clause key)
   and add it as a submodule at `tests/conformance/`. Until then the conformance job has no input
   and must not be faked with a synthetic directory (ADR-0010 rule 6).
3. DNS for `tyny.ca`: an `A`/`CNAME` for `www`, a `TXT` SPF record if the address is ever used for
   mail, and `updates.tyny.ca` pointing at the GitHub releases redirect. `ca.tyny.pdf` needs none of
   this to be a valid reverse-DNS identifier (ADR-0008 rule 5).
4. Decide on trademark clearance for "Tyny" in software, and check whether `Tyny.TynyPDF` is free in
   winget-pkgs. Both are announcement gates, not development gates.

## 13. Reading list (what we treat as normative, in what order)

`AGENTS.md`, then `adr/0001` (canon), `adr/0011` (modularity), `adr/0009` (workflow),
`adr/0010` (environment), `docs/dev-environment.md`, and the ADR relevant to the capability being
touched. `docs/lessons.md` before starting a milestone, so the same mistake is not made twice in a
row. Anything not in this list is context, not instruction.
