# Epic 5 – Testing Strategy [grounded]

Levels from `docs/testing-playbook.md` §2.
Everything names command and artefact.

> AI-Guard: each level has guard.

## Gates (every PR)

- Repo: `sh tools/check.sh` 14/14;
  `gates-selftest 13/13`. Guard: rewrap 11->14
  → `docs-check` catches.
- Style: `sh tools/format-check.sh`. Guard: engine
  include in `src/core` → `layering` red.
- Language: `python3 tools/lang-check.py` 0.
- Naming: `python3 tools/naming-sync.py check` 0.
- Spec: `python3 tools/spec-check.py` 0 orphans.

## 5.1 Entry + measure spine

- **Goal:** harness trusted before numbers trusted.
- **Harness:** `tools/bench-measure.sh --runs 2
  --record-machine` twice within 10%, both JSONs
  committed, machine block from tool.
- **Refusals:** missing binary, non-PDF, cross-machine
  — each exit code pasted.
- **Cmd:** `bench-measure.sh` + `win-probe gpu`
  LUID.

## 5.2 Window, swapchain, blit

- **Goal:** 60fps p99 ≤33ms + byte identity.
- **Perf:** `tools/bench-measure.sh` `frame_ms` p50/p99 +
  sample count, `sha256sum` CLI vs window bytes
  equal.
- **Layering:** `grep windows.h src/core` 0;
  `grep mupdf src/render` 0; `layering --strict`
  ≤0.07.
- **Spec:** `src/features/render/SPEC.md` with first
  file, `spec-check` 0 orphans.
- **Cmd:** `check.sh` + `ctest` + `layering`.

## 5.3 Tiles, cachemap, budget

- **Goal:** 250MB + return pass headless.
- **Unit:** `test_budget.cc` `max_tiles=1` insert 2
  → evict 1, headless on Linux, `ctest -R budget`.
- **Perf:** `peak_rss_kib` forward vs return from
  one harness run, corpus `sha256sum`.
- **Guard:** `grep free( src/render` 0 (R-M6).
- **Cmd:** `ctest -R budget` + harness.

## 5.4 Cold start, DPI, gesture, caret

- **Goal:** cold ≤300ms + DPI byte-equal + gesture
  ≤16ms + caret headless.
- **Perf:** cold 3 numbers + machine block;
  relative vs Sumatra open if baseline missing.
- **Approval:** `tests/approvals/dpi-150.png`
  byte-for-byte vs scaled render.
- **Gesture:** `tynypdf.ui` per-frame log p50/p99
  + sample count.
- **Caret:** `test_caret.cc` headless
  `e+U+0301` one step; IME only Windows job.
- **Cmd:** harness + `ctest -R caret`.

## 5.5 A11y + verdict

- **Goal:** keyboard + Narrator/NVDA + verdict.
- **Manual:** `docs/a11y/keyboard.md` +
  `narrator-nvda.md` with exact announced text
  diffable.
- **UIA:** `src/os/win32/uia` `IRawElementProvider`
  assertions on Windows job.
- **Verdict:** keep (7 numbers) or kill (new ADR +
  frame table, spike deleted, kickoff §12 amended).
- **Cmd:** `check.sh` green on last commit.

## Regression (per story + end)

- [ ] `check.sh` after every story, on clean clone
- [ ] `docs-check` 0 with 5 new epic docs
- [ ] `spec-check` 0 orphans, pending 0 or kill ADR
- [ ] `naming-sync` 0, `lang-check` 0
- [ ] `layering --strict` ≤0.07
- [ ] `ctest 0 failed`, contract both backends

**Checklist:**

- [ ] 5.1 entry + 2 runs + 3 refusals
- [ ] 5.2 p99 ≤33ms + byte identity
- [ ] 5.3 250MB + return pass
- [ ] 5.4 cold + DPI + gesture
- [ ] 5.5 a11y + verdict
- [ ] Final gates green on `main`

---

*Run as each story executes, paste into `epic-5-dod.md`.*
