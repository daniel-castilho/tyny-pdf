# Testing Playbook — Tyny PDF

Role: define how to design, run, diagnose and maintain tests for **Tyny PDF** across `pdfcore` (the
engine-agnostic document model and IR), the backend adapters, the annotation sidecar, the headless
CLI, and the Win32/Direct2D presentation layer.

**Official Domain:** [https://tyny.ca](https://tyny.ca) | **App ID:** `ca.tyny.pdf`

> **What runs today.** The gates in `tools/`: `tools/check.sh` runs nine checks in eleven sections
> (canonical-check has no self-test of its own), and `tools/gates-selftest.sh` proves the nine
> self-tests, wired into `.github/workflows/gates.yml`. Everything else here is
> the harness that PR #5 lands with the first compiled code, and the milestones it is gated on
> (`docs/kickoff.md`). A `cmake` or `ctest` line in this file is a specification of what will exist,
> not a command that works yet - which is the one way this project's test documentation differs
> from a running project's.

---

## 1. Testing Principles

1. Test observable document behaviour and the public contract, not internal call sequences. A test
   that asserts `mupdf_backend_render_page` was called is a test of the adapter wiring.
2. Keep the fastest feedback loop at the lowest layer that can hold the assertion: `pdfcore` on the
   `null` backend, then the CLI, then the window. A bug reproducible without a window is fixed
   without one (ADR-0011 R-M10).
3. Core invariants are tested with no engine in the picture. The `null` backend is a real
   implementation, not a mock for show - it is the proof that `include/pdfcore/backend.h` is the
   whole interface (`docs/kickoff.md` section 8).
4. Every rejection path is a test: corrupt page, encrypted document, missing font, unsupported
   capability, oversized tile, truncated sidecar, tombstone conflict. A green happy path with no
   rejection path proves nothing about ADR-0003.
5. Tests are deterministic and byte-stable: no wall clock, no locale, no absolute path, no address.
   Anything that prints a geometry prints it at the sidecar's 3-decimal rounding, and the approved
   artefact is committed.
6. A test that is not traced is not evidence: each one cites the `R<n>.<m>` id it proves, and each
   requirement's `Verification:` line points back at a file (`tools/spec-check.py`, ADR-0011 R-M13).
7. Never delete or weaken a valid test to pass a build. Change the expectation with a paragraph in
   the PR, or change the code. Lowering a floor, widening an exclusion list or disabling a check is
   an ADR-sized act, not a fix (`docs/lessons.md`).
8. Where an external oracle exists, use it: veraPDF grades validity and tagged-PDF claims, and a
   rendering digest is compared to an approved golden file - we do not grade our own homework.

---

## 2. Test Taxonomy

| Level                | Location                                | Runtime                                | Purpose and exit signal                                                          |
| :------------------- | :-------------------------------------- | :------------------------------------- | :------------------------------------------------------------------------------- |
| **Repo gates**       | `tools/`                                | `python3`, `sh` (today)                | language, naming, sidecar bytes, spec trace, docs honesty, byte stability, diff and tree hygiene |
| **Unit**             | `tests/unit/test_<subject>.cc`          | GoogleTest via `ctest` (M0)            | sidecar encode/decode, IR geometry, canonical rounding, id stability             |
| **Contract**         | `tests/contract/`                       | same suite, every backend (M0)         | the vtable is the interface; `unsupported` is never reported as `empty` (R-M5)  |
| **Approval**         | `tests/approvals/`                      | golden files, `--accept` flow (M0)     | structured IR dumps, CLI JSON, rendered page digests                             |
| **Property**         | `tests/unit/test_sidecar_property.cc`   | randomised inputs (M2)                 | canonicalisation idempotence, key order, tombstone three-way merge, undo replay  |
| **Conformance**      | `tests/conformance/<iso-clause>/`       | corpus submodule + veraPDF (M4)        | validity and accessibility claims per clause, each file documented with the clause |
| **Fuzz**             | `tests/fuzz/`                           | libFuzzer, 4 h nightly (M0+)           | parsers (PDF, sidecar, font), the engine bridge, the sealer                      |
| **Performance**      | `tests/baseline.json`, trend job        | reference machine (M0.4, M1)           | blit frame time p99, RSS at 1000 pages, cold start, versus Sumatra's numbers     |
| **Accessibility**    | `docs/a11y/` scripts (planned, M5)      | Narrator + NVDA on Windows             | exact expected narration; UIA tree assertions where programmable                 |
| **Privacy**          | no-route job + `strace`/ETW capture     | CI (M0.2)                              | a full open-edit-save cycle opens no socket; any socket is a build failure       |
| **Windows-specific** | `windows-msvc` CI job, physical machine | MSVC parity build (M0.2, M1)           | DPI V2 behaviour, spooler, Direct2D timing, IME (ADR-0010: never assumed from Linux) |

---

## 3. Commands & Execution

### 3.1 The loop that exists today

```bash
sh tools/check.sh                    # nine checks, eleven sections, all gates
sh tools/gates-selftest.sh           # the checks-on-the-checks, 9/9 suites
python3 tools/spec-check.py          # requirement shape, orphans, artefacts
python3 tools/sidecar-fmt.py check tests/fixtures/sidecar
```

Expected on a clean tree: `check: all gates green`, and `spec-check` reporting the pending ids it is
supposed to still report (10, at the time of writing). A run that stops listing pending requirements
before the artefacts exist is a broken checker, not a finished feature.

### 3.2 Fast core loop (from M0, PR #5)

```bash
cmake --preset linux-core && cmake --build --preset linux-core
ctest --preset linux-core --output-on-failure
ctest --preset linux-core -R sidecar --output-on-failure      # one cluster
```

Core tests run against the `null` backend by default and against `mupdf` in the contract job, so a
failure names its layer: `null` red means our logic, `mupdf` red means the adapter or the engine.

### 3.3 Headless CLI runner

```bash
./build/linux-core/Release/tynypdf-cli sidecar check document.pdf
./build/linux-core/Release/tynypdf-cli txn replay document.tynypdf.json
```

The CLI is the same `pdfcore` the viewer uses, with no window, and it is the transport for CI
(`docs/kickoff.md` section 5). Exit-code contract from
[../adr/0003-error-model.md](../adr/0003-error-model.md) section 6: `0` success, `1` assertion
failure, `2` usage or input error, `3` document or backend error -
`tests/unit/test_cli_exit_codes.cc` covers all four. Reports are JSON or JUnit, chosen by the
report file extension, so a run can be attached to a PR.

### 3.4 Approval tests and the accept flow

```bash
ctest --preset linux-core -R approval
./build/linux-core/Release/tynypdf-approvals --update        # writes *.received.* as approved
git diff -- tests/approvals                                   # the reviewable artefact
```

`.gitignore` keeps `*.received.*` out of the tree; the committed file is the approved one, and the
diff of `tests/approvals/` is what a reviewer reads. An approval change without a paragraph of
justification in the PR is a rejected PR.

### 3.5 Trace and drift guards

```bash
python3 tools/spec-check.py                    # every requirement cites an artefact
python3 tools/naming-sync.py check             # derived identifiers match docs/naming.md
python3 tools/diff-scan.py --tree              # exec bits, symlinks, invisible codepoints,
                                               # CI triggers, action pins, dependency names,
                                               # manifest and lock agreement
git status --porcelain generated/              # must be empty in CI
```

`generated/` is written by `python3 tools/naming-sync.py write`, and CI fails when the tree
carries drift - the same pattern as the "regenerate the golden error-code header and expect a
clean `git status`" rule in [../adr/0003-error-model.md](../adr/0003-error-model.md). A test
asserting on a header that nobody regenerated is theatre.

### 3.6 Fuzz targets (nightly, from M0+)

```bash
cmake --preset linux-core -DPC_FUZZ=ON
./build/linux-core/Release/fuzz_sidecar -max_total_time=14400 -dict=tests/fuzz/json.dict tests/fuzz/seed
```

A crash is a bug with a committed repro file next to the target, minimised, and the input that
triggered it is reduced to the smallest case that still reproduces. Fuzz findings never reopen a
canonical rule by editing the corpus: the format is frozen (ADR-0007), so a finding is fixed in
code.

### 3.7 Conformance corpus

```bash
git submodule update --init tests/conformance
veraPDF-cli --format text tests/conformance/ISO-19005-3/clause-6-2/*.pdf
```

The corpus is `daniel-castilho/tyny-pdf-corpus`, pinned as a submodule, and it does not exist yet -
until it is created the conformance job has no input and **must not be faked with a synthetic
directory** (ADR-0010 rule 6, `docs/kickoff.md` section 12). Each file is documented with the clause
and the expected outcome, so a failure names a requirement instead of a diff.

### 3.8 Performance and baseline

```bash
sh tools/bench-measure.sh --reference        # planned: written by PR #3
```

Numbers land in `tests/baseline.json` next to the SumatraPDF reference (3.6.1 stable and the 3.7
pre-release), measured on the machine spec recorded in the same PR. The acceptance rule is
`docs/kickoff.md`'s: numbers must be reproducible within 10 % on a second run, and a
measurement in a document carries its query, its unit and its date.

### 3.9 Privacy and accessibility runs

```bash
strace -f -e trace=network -o /tmp/no-net.log ./build/linux-core/Release/tynypdf-cli open sample.pdf
grep -c 'socket(' /tmp/no-net.log            # expected: 0 (CLI never opens a route)
```

Accessibility is not automated away: `docs/a11y/` holds the scripts with the exact narration
expected from Narrator and NVDA on a Windows desktop, run against the physical machine before M5
closes, and UIA assertions cover what can be asserted programmatically.

---

## 4. Mandatory Patterns & Rules

| Area                   | Rule                                                                                       |
| :--------------------- | :----------------------------------------------------------------------------------------- |
| **Core tests**         | Build the document with the `null` backend; no engine header, no Windows header, no mock of our IR. |
| **Contract tests**     | The same assertions run per backend; a capability the backend does not declare is skipped, not passed. |
| **Fixtures**           | Small, in-tree, generated where possible, never a real person's document; a private corpus stays in `tests/conformance/private/` (`.gitignore` enforces it). |
| **Time and locale**    | Timestamps come from an injected clock; the `C` locale is asserted where formatting matters. |
| **Secrets**            | No key material, password or decrypted bytes in a fixture, an approval file or test output; signing fixtures use a generated test key whose bytes are in the test. |
| **Boundary check**     | Enforce `AGENTS.md` rule 1:                                                             |

```bash
grep -rEn '#include *[<"](windows|windef|unknwn|d2d1|dwrite|fitz|mupdf|pdfium)' src/core
```

_Expected result: 0 matches. `src/core` has no C++ source yet, so the grep finds nothing - that is
not the pass, and no PR may record it as one. `tools/layering-check.sh` starts from this grep and
adds the `backend_line_ratio <= 0.15` budget (ADR-0011 R-M10/R-M11). Today it reports 0 violations
and a ratio under budget._

---

## 5. Regression Checklist

| Area                  | Must verify                                                                                          |
| :-------------------- | :--------------------------------------------------------------------------------------------------- |
| **Sidecar**           | round-trip is byte-identical; unknown keys survive; a tombstone beats an absent record; a two-byte edit is a two-line diff |
| **Annotations**       | geometry rounding at 3 decimals, page-space origin, colour components, id stability across a reopen   |
| **Staleness**         | fingerprint change versus page-count change are reported as different causes, not one "out of date"    |
| **Redaction**         | the redacted output has no extracted text at the redacted region, and the audit artefact says so       |
| **Forms**             | required/format/range validation, tab order following visual order, flatten verified by veraPDF        |
| **Text**              | combining marks, grapheme-safe caret movement, hyphenation off by default, pt-BR golden corpus         |
| **Undo**              | 10k operations then replay to byte-identical state; the CLI sees the same history as the UI            |
| **Signing**           | verify is offline (V0+V1); a locally produced signature is labelled `PAdES-B-B (local)`; the online revalidate action is opt-in |
| **Presentation**      | DPI 100/150/250/400 %, scroll at the memory budget, tile cache does not grow when scrolling back      |
| **Engine swap**        | a build with only the `null` backend links, and every capability-gated feature reports unsupported     |

---

## 6. Definition of Done for Testing

- [ ] `sh tools/check.sh` prints `check: all gates green`, with the output in the PR body.
- [ ] Every requirement touched has a test that cites its `R<n>.<m>` id, and `python3
  tools/spec-check.py` reports 0 orphans.
- [ ] `null`-backend unit tests and the contract suite pass on Linux; the `mupdf` job passes or
  the failure is named as engine-specific.
- [ ] Approval diffs are explained in the PR, not only accepted; `tests/approvals/` changes cite
  the requirement they serve.
- [ ] No new dependency, no widened exclusion, no disabled check (R-M9: a new tool or flag needs
  an ADR that prices it).
- [ ] Any measurement quoted in the PR names machine, unit, date and the reproduction run.
- [ ] From M0: `ctest --preset linux-core` green, sanitizers clean, the fuzz target ran without a
  new crasher.
- [ ] Docs moved with the code: the capability `SPEC.md`, `README.md`, this file if a command
  changed, and the debt matrix in `../AGENTS.md`.

---

> _"A test proves a requirement, or it proves that the harness still compiles. When in doubt, name
> which one you wrote."_
