# Epic 2 – Testing Strategy [grounded]

Test levels are the taxonomy of `docs/testing-playbook.md` section 2 (Repo gates, Unit, Contract,
Approval, Conformance, Performance, Privacy, Windows-specific), and every item names the command
that executes it and the artefact its output goes into. Something that cannot be run yet is labelled
with the story or PR that introduces it, exactly as `docs/testing-playbook.md` section 3 does with
blockers. A spike adds no new test level: it is measured with the levels the repository already
owns.

## 2.1 The measurement spine

- **Goal:** the instrument is trusted before its numbers are used, and the entry conditions are
  results rather than assumptions.
- **Action:**
  - Run each entry condition command and paste the whole output, including refusals:
    `python3 tools/layering-check.py`, `cmake --preset win-cross-x64`,
    `sh tools/win-probe/build.sh --probe gpu`
  - `python3 tools/bench-measure.py --target tynypdf --binary <exe> --corpus <dir> --runs 2
    --record-machine --out tests/spike-tynypdf.json`, then
    `python3 tools/bench-measure.py --compare tests/spike-tynypdf.json <second run>`
  - Negative cases, deliberately: a binary path that does not exist, a corpus directory containing
    a non-PDF, and a compare across two runs recorded on different machines. All three must refuse;
    the tool's own behaviour is `exit 2` for a missing file and a failure for a CPU change between
    runs
  - Check that the harness's machine block matches `docs/dev-environment.md`'s stated box
    (`memory=12GB`, `processors=8`, `swap=0`) or the divergence is recorded in the DoD
- **Acceptance criterion:** two runs within the tool's 10 % tolerance on every metric, both pasted,
  plus one pasted refusal per negative case. A harness that has never been shown to say no is not an
  instrument; `docs/lessons.md` has the pattern ("a check that cannot detect its own violation").

## 2.2 The surface

- **Goal:** criterion M1 row 1 is a frame table, and the presentation path produces bytes identical
  to the headless path.
- **Action:**
  - Performance: the 4000x3000 region blit, with `scroll_frame_p99_ms` and the raw frame list from
    the CLI's JSON reporter; sustained means the whole run, so the run length is recorded next to
    the number (a 60 fps figure from 12 frames is a rounding error)
  - Approval: the same page region through `tynypdf-cli render --page N --out x.png` and through
    the window, compared as hashes; the pair of hashes is pasted, and the pixel files are not
    committed as evidence of themselves
  - Layering: `python3 tools/layering-check.py --strict`, and read the L6 verb rule as the tool
    reads it - a parsing or mutation verb inside `src/render` or `src/os` is the failure R-M10
    names
  - `python3 tools/format-check.sh` and `python3 tools/spec-check.py` on the new files
- **Acceptance criterion:** no frame over 33 ms p99 with the frame table attached; identical hashes
  between CLI and window; `--strict` layering exit 0 with the ratio printed. A "60 fps (observed)"
  line with no table behind it fails this story, not the measurement.

## 2.3 Tiles and the budget

- **Goal:** the RSS ceiling, and the eviction decision made testable where it can be tested.
- **Action:**
  - Contract-style performance run: 1000 pages, three tiles each, scroll forward, scroll back, one
    harness invocation; `peak_rss_kib` from `getrusage(RUSAGE_CHILDREN)` (the tool's own
    mechanism), both passes in the same file
  - Unit: eviction policy on `src/core/budget` - a fixture of page sizes and a byte ceiling,
    asserting which tiles survive; no window, no GPU, runs in `linux-core`
  - Negative control: shrink the budget in the fixture and confirm the resident set shrinks; if the
    test still passes, the budget is being ignored somewhere and the test is decoration
  - R-M6 audit: `grep -n "free(" src/render src/os` must return nothing over engine memory, and the
    grep is pasted, because "we never free engine memory" is a claim the spike is exactly when
    people break
- **Acceptance criterion:** `<= 250 MB` on the forward pass and no increase on the return pass, both
  printed by the same run; the eviction unit test green on Linux; the corpus path and its sha256
  list recorded beside the numbers.

## 2.4 Cold start

- **Goal:** the absolute number is measured on the reference machine and the relative comparison is
  reported as unavailable, with its cause.
- **Action:**
  - Read the startup pair from the `tynypdf.ui` log line; paste the line, not a summary
  - Repeat three times on the same machine and report all three; cold means a fresh process, so the
    record says whether the file cache was dropped between runs
  - `python3 tools/corpus-check.py --root .` and `test -e tests/baseline.json` run as the proof of
    why the comparison cannot be made yet; both outputs pasted in the same block as the number
  - If Epic 1 story 1.5 has landed by then, the comparison is `--target sumatra-3.6.1` and
    `--target sumatra-3.7pre` against `tests/baseline.json`, and the relative row becomes measurable
    rather than aspirational
- **Acceptance criterion:** the absolute row answered with three numbers and a machine block; the
  relative row recorded as open with the two outputs above. Downgrading row 3 to "absolute only"
  here would be the move the downgrade gate (`docs/kickoff.md` section 7, gate 3) exists to prevent.

## 2.5 DPI, composition, caret, gesture

- **Goal:** four criteria that are trivially fake-able, each tied to an artefact a reviewer can
  re-take.
- **Action:**
  - DPI: approval pair per scale (150 %, 200 %) under `tests/approvals/`, comparing the render asked
    at the scaled size against the render asked at 100 % then upscaled; a stretch is visible as a
    byte difference, so the two files are the test
  - Gesture: the per-frame log line carrying input and present timestamps; report p50 and p99 of the
    difference and the sample count. An average over a laggy run looks fine, which is why the brief
    names p99 for frames and a hard 16 ms ceiling for gestures
  - Caret: `src/core/text` unit tests over combining marks and ABNT2 composition strings, run
    headless in `linux-core`; the IME plumbing itself is a Windows-only test, executed on the
    Windows job
  - Privacy: `tools/check.sh` has no network job yet, so the spike's own discipline is that the
    timing runs happen with no route to anything but localhost (ADR-0003 default-deny). Recording
    the absence is part of this story, not a footnote to M6
- **Acceptance criterion:** the approval pairs exist and match; p99 gesture delta at or under 16 ms
  with the sample count; caret tests green headless; a Windows job log line for the IME case.
  Nothing inferred from the Linux side, which is ADR-0010's rule rather than a preference.

## 2.6 Narration and the verdict

- **Goal:** the accessibility floor in M1 row 5, and a verdict that can say "stop".
- **Action:**
  - UIA tree assertions from the Windows job for page indicator, zoom and focus; where UIA is not
    programmable, the `docs/a11y/` script is the record, and it contains the exact expected text
  - Narrator and NVDA passes run manually on the physical machine, with the announced text pasted
    from the script file rather than retyped into prose
  - Keyboard traversal: a checklist of the spike's controls with the tab order written down, each
    row marked from the run, not from the intent
  - Verdict: one of the two shapes from `epic-2-technical-tasks.md` 2.6. If the kill criterion
    fired, the evidence attached to the new ADR is the frame table from 2.2 and the swapchain setup
    cost from the spike's own logs
- **Acceptance criterion:** row 5 answered with the script files and the pasted announced text; the
  verdict committed. A "generally accessible, we will polish later" line is not an acceptance
  criterion met; `docs/testing-playbook.md` treats exact narration as the artefact for a reason.

## Regression gates (per story and at the end)

- Per story: `sh tools/check.sh`, and for 2.2 and 2.3 additionally
  `python3 tools/layering-check.py --strict` and `python3 tools/spec-check.py`
- At the end: `sh tools/gates-selftest.sh` (12 suites), `ctest --preset linux-core`,
  `python3 tools/bench-measure.py --compare <run1> <run2>`, `sh tools/win-probe/build.sh
  --self-test` and the `epic-2-dod.md` self-audit, all with output pasted
- The epic does not re-wire `tools/check.sh`: the layering gate enters it in the PR that adds the
  first header under `include/pdfcore/` (Epic 1 story 1.3's exit condition), and this epic either
  satisfies that or leaves both open

---

*Next step: run 2.1's negative controls before any blit exists. An instrument proven to refuse is
the only thing that makes the other five stories' numbers worth pasting.*
