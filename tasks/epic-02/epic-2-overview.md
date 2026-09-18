# Epic 2: The UI spike - three days, with a kill criterion (M1, D6b)

**Project:** tyny-pdf
**Context:** C++20 core behind a C ABI (`pdfcore`), Win32 + Direct2D over a DComp/D3D11 swapchain,
LLVM-MinGW cross build from Linux, `tynypdf-cli` as the CI transport (ADR-0002, ADR-0003, ADR-0010).
**Goal:** answer one question with measurements instead of argument - can a hand-written
Win32/Direct2D surface hold the performance and accessibility floor without a framework? The brief
sets the wording of the criteria and this epic does not soften it: "Measured, not estimated"
(`docs/kickoff.md` section 6, M1).

---

## Repo state (measured 2026-09-17T16:33Z, planning pass, not execution)

Measured in this workspace with the commands pasted in `epic-2-dod.md` section 2.0. Re-taken after
these five documents were added: the planning measurement (2026-09-17T15:54Z) saw 72 files, 579692
bytes and `docs-check: OK (30 markdown files, 0 problems)`, and `docs/epics/` has doubled since
then.

- **77 files, 637488 bytes** excluding `build/`. `sh tools/check.sh` -> `check: all gates green`;
  the script runs eight gates across ten sections - `lang-check`, `naming-sync`, `sidecar-fmt`,
  `canonical-check`, `spec-check`, `format-check`, `diff-scan`, `docs-check` - including the
  diff-scan change-hygiene scan (D1-D8) with its own self-test. `sh tools/gates-selftest.sh` ->
  `gates-selftest: OK (8/8 suites hold)`. `python3
  tools/docs-check.py` -> `OK (35 markdown files, 0 problems)`: 10 in `docs/epics/` (five per
  epic), 9 under `docs/`, 11 in `adr/`, plus `README.md`, `AGENTS.md`, `CHANGELOG.md`,
  `.github/PULL_REQUEST_TEMPLATE.md` and `src/features/sidecar/SPEC.md`.
- **Epic 1's story 1.2 is delivered:** `CMakeLists.txt`, `CMakePresets.json` (`linux-core`,
  `win-cross-x64`, `win-msvc` behind a host condition), `tools/cmake/toolchain-llvm-mingw.cmake`,
  `src/backends/CMakeLists.txt`, `tests/CMakeLists.txt`, `tools/win-probe/` with four probes and a
  `build.sh` whose self-test reports 5/5. `cmake --preset linux-core` configured and `ctest`
  passed 2/2 earlier in this session, before this container lost `cmake` and `build/` was cleaned;
  the two lines carry that caveat in `epic-2-dod.md` section 2.0 rather than pretending to be
  current. `cmake --preset win-cross-x64` refuses with the missing-toolchain message rather than
  configuring against a system compiler.
- **Epic 1's stories 1.3 and 1.4 are not delivered in this tree**, which is what gates this epic:
  `third_party/`, `conanfile.py`, `conan.lock`, `NOTICE`, `include/pdfcore/`, `src/core/`,
  `src/backends/null/`, `src/backends/mupdf/`, `src/cli/`, `src/os/` and `src/render/` do not
  exist. `python3 tools/layering-check.py` prints `backend_line_ratio=undefined` with the
  parenthetical naming the absent header, and exits 1 under `--strict`.
- **There is no baseline to compare against yet.** `tests/baseline.json` is absent, `python3
  tools/corpus-check.py --root .` exits 3 with `tests/conformance/manifest.txt does not exist`,
  and `python3 tools/bench-measure.sh --compare a b` exits 2 naming the missing file. The Sumatra
  measurement is Epic 1 story 1.5, recorded as owner debt and to be resolved at the end.
- **This workspace has no `.git`.** `git rev-parse` fails here, so nothing in this epic may cite a
  commit, a run id or a protection setting as evidence: the evidence is a command output pasted
  into `epic-2-dod.md`, wherever the command is run.

## Why this epic now

- **The risk register says this is the expensive unknown.** `docs/kickoff.md` section 10 lists
  "Hand-written UI costs more than predicted" with the tripwire "M1 kill criterion; or any UI bug
  that takes a day to diagnose because the framework we refused would have fixed it". Every later
  delta spends its effort on the presentation layer this decision determines.
- **The brief schedules it in parallel on purpose.** Section 11's closing line: "M1 (the spike)
  runs in parallel with PRs 3 and 4, because its three days are cheapest while nobody is blocked
  on it." Waiting for the whole of Epic 1 before starting would convert a three-day spike into a
  dependency.
- **Open question with a deadline.** `docs/kickoff.md` section 12 leaves "Skia as a second render
  backend" at *no; revisit only if M1's kill criterion fires*, deadline "after M1". This epic is
  the only thing that can answer it; not running M1 leaves the question open and the codebase
  guessing.
- **ADR-0010 already constrains where the numbers may come from.** Its probe table's last row: the
  probe window reports the adapter LUID, and "if it reports the software rasteriser, M1 timing is
  measured on the Windows session only". The spike either runs there or records that it cannot.

## The seven criteria, as the brief states them

Quoted from `docs/kickoff.md` section 6, M1. The "Measured by" column is the mechanism this epic
builds or reuses; nothing in it is invented for this document, and where a mechanism does not
exist yet the story that writes it is named.

| # | Criterion (verbatim target)                                          | Measured by | Story |
|---|------------------------------------------------------------------------|-------------|-------|
| 1 | Blit a 4000x3000 page region: 60 fps sustained, no frame over 33 ms p99 | `tools/bench-measure.sh` frame metrics over the CLI's JSON reporter, on the Windows session | 2.2 |
| 2 | RSS at 1000 pages open, 3 tiles each: <= 250 MB, and it must not grow when scrolling back | `tools/bench-measure.sh` `peak_rss_kib`, forward pass against return pass | 2.3 |
| 3 | Cold start to first painted page: <= 300 ms on the reference machine (vs measured Sumatra) | absolute: the app's own `tynypdf.ui` log line for the presenting frame; relative: `tests/baseline.json`, which does not exist yet | 2.4 |
| 4 | DPI: Per-Monitor V2, no bitmap stretch at 150%/200% | rendered page compared against the same page asked at the scaled size; approval bytes in `tests/approvals/` | 2.5 |
| 5 | Keyboard and screen reader: every control reachable; Narrator and NVDA announce page, zoom, focus | the first two scripts in `docs/a11y/`, with the exact announced text recorded | 2.6 |
| 6 | Text input: ABNT2 and pt-BR composition in a text field, correct caret movement over combining marks | unit tests on the caret arithmetic in `src/core/text`, OS-layer composition tests | 2.5 |
| 7 | Pinch/zoom, wheel: 1:1 tracking, no gesture lag over 16 ms | per-frame log line carrying the input timestamp and the presenting timestamp | 2.5 |

**Kill criterion, verbatim:** "if the swapchain or the blit cannot hold 60 fps after the tile
cache and the DComp visual tree exist, we do not 'optimise harder' - we revisit D3 with the
spike's numbers in a new ADR. The spike is time-boxed at three days precisely so that the revisit
happens with data instead of with hope." Story 2.6 exists to write that decision down; a spike
whose verdict is "it does not hold" has met its acceptance criteria, and a spike that says "it
holds" without the seven numbers has not.

## Entry conditions (before hour one)

1. A backend exists that can produce pixels: `include/pdfcore/backend.h` and at least
   `src/backends/null` (Epic 1 story 1.4). `python3 tools/layering-check.py` stops reporting
   `undefined`. Without this the spike blits a synthetic pattern, which is a legitimate first day
   and is recorded as such - but criterion 2 (RSS at 1000 pages) is not measurable against a fake
   document and must not be quoted as if it were. 2. `cmake --preset win-cross-x64 && cmake
   --build --preset win-cross-x64` produces `build/win-cross-x64/Release/tynypdf.exe` (the path
   `docs/dev-environment.md` documents). 3. `sh tools/win-probe/build.sh --probe gpu` on the
   Windows session prints an adapter LUID that is not the software rasteriser. If it is,
   ADR-0010's consequence applies and the record says timing came from the physical machine, not
   from CI. 4. `docs/lessons.md` read before starting, per `docs/kickoff.md` section 13.

## Acceptance criteria (grounded)

1. **Each of the seven rows above has a number and the command that produced it**, pasted into
   `epic-2-dod.md`. "60 fps sustained" without a frame table is not a result; a row that could not
   be measured carries the reason and the story that unblocks it.
2. **The numbers came from the reference machine** described in `docs/dev-environment.md`
   (`~/.wslconfig`: `memory=12GB`, `processors=8`, `swap=0`), and from the Windows session if
   ADR-0010's rasteriser condition fired. The machine spec is the one `tools/bench-measure.sh`
   records with `--record-machine`, not a hand-written paragraph.
3. **Reproducibility matches the house rule:** a second consecutive run within 10 % per metric,
   which is `tools/bench-measure.sh`'s own tolerance (`TOLERANCE = 0.10`), both runs pasted.
4. **Layering held while the surface was written.** `python3 tools/layering-check.py --strict`
   exits 0 with `backend_line_ratio` a number at or below 0.15: R-M10 is the rule that keeps
   parsing, geometry reconciliation and annotation mutation out of `src/render/` and `src/os/`,
   and a spike is exactly where people put them.
5. **`src/features/render/SPEC.md` exists, written with the first file in that directory and never
   before it**, with `Verification:` targets that `python3 tools/spec-check.py` accepts - this is
   where the frame budget stops being a spike note and becomes a requirement (the response column
   in `docs/kickoff.md` section 10: "D6b's numbers become the contract in `SPEC.md`").
6. **The verdict is written.** Either the seven numbers are in `epic-2-dod.md` and the spike's
   code is the seed of the viewer, or the kill criterion fired and a new ADR - the next number in
   `adr/`, created at the moment it is needed, since `docs-check.py` requires every ADR to be
   indexed - records D3's revisit with those numbers. `docs/kickoff.md` section 12's Skia row is
   then amended by reference, not silently.
7. **The time-box is honest.** Three days means three sessions: each one records what it attempted
   and what it produced. A fourth day is allowed only with a written note about which criterion
   moved, because the box exists so the revisit happens with data (D6b), and `docs/kickoff.md`
   section 6 says milestones are gates, not calendars.
8. **Rule zero:** every number, hash or count in `epic-2-dod.md` is pasted from a command output
   included in that same document; a figure with no command above it is labelled a hypothesis.
   Nothing in these five documents is marked `[x]` by their author.

**Quick traceability:**

| Story | Reference doc                                          | Key aspect |
|-------|--------------------------------------------------------|---------------------------------------------------|
| 2.1   | `docs/kickoff.md` M1, section 11                       | entry conditions, measurement spine, the three days |
| 2.2   | `docs/kickoff.md` M1 row 1, ADR-0011 R-M10             | window, DComp swapchain, Direct2D blit at budget    |
| 2.3   | `docs/kickoff.md` M1 row 2 and section 10              | tiles, cachemap, the memory budget as core decision |
| 2.4   | `docs/kickoff.md` M1 row 3, Epic 1 story 1.5 (debt)    | cold start absolute now, relative when the bar exists |
| 2.5   | `docs/kickoff.md` M1 rows 4, 6, 7                      | Per-Monitor V2 DPI, composition and caret, gesture lag |
| 2.6   | `docs/kickoff.md` M1 row 5, kill criterion, section 12 | reachability, narration, the verdict and its ADR    |

---

*Next step: the entry conditions above, then stories 2.1-2.6 in `epic-2-stories.md`, in the task
order of `epic-2-technical-tasks.md`, pasting evidence into `epic-2-dod.md` as each criterion is
answered. A spike that ends in "we were wrong, here is the data" is a completed epic.*
