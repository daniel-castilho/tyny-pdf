# Epic 7: Forms — AcroForm/FDF, Fill and Flatten (D-2)

**Project:** tyny-pdf
**Context:** C++20 core behind C ABI (`pdfcore`),
Win32 + Direct2D over DComp/D3D11 swapchain,
viewer loop + tiles/cachemap/budget,
selection/search/annot as txn (Epic 6),
`tynypdf-cli` + `bench-measure.sh`
(ADR-0001, ADR-0002, ADR-0010, ADR-0011).
**Goal:** every AcroForm field is fillable,
undoable and flattenable without opening a browser.
After this epic, text/checkbox/radio/combo fields
fill headless, `pc_txn_undo` restores, `flatten`
bakes appearance streams, and FDF import/export
round-trips byte-identical via `sidecar`.

---

## Repo state (measured 2026-09-24, execution time)

Measured on `main @ 05ce35c` (post Epic 6) — tree
this epic starts from. Every number pasted from
command output in `epic-7-dod.md` §7.0.

- **14/14 gates green.** `sh tools/check.sh` — 14
  sections, `gates-selftest 13/13`, `spec-check
  26 specs / 90+ reqs / 0 orphans / 0 pending`,
  `layering 0.0236` on 56+ files, 0 violations.
- **Interaction done.** `src/core/selection` +
  `search` + `annot` (R32-R44) with `re-anchor`
  byte-offset + fuzzy, `ctest 45/45`.
- **Forms is spec-only.** `src/core/forms/` and
  `src/features/forms/SPEC.md` are
  `planned: implementation in progress`;
  `src/backends/mupdf` already exposes `fz_widget`
  via vtable but no `pc_form_*` yet.
- **Toolchain proven.** `win-cross-x64` +
  `windows-msvc` both build `tynypdf.exe` with
  viewer + selection; `tests/bench/corpus/1000p.pdf`
  has no forms (new `tests/fixtures/forms/` needed).
- **Baseline honest.** `forms` baseline not measured
  — first `bench-measure.sh --forms` run will be
  pasted in §7.0.

## Why this epic now

- **Tripwire from kickoff §10.** "Hand-written
  form fill costs more than predicted" — if fill
  lives in `src/os` or `src/render`, every later
  delta (sign, redaction, a11y) spends on the wrong
  layer.
- **Annot proved txn works.** Epic 6 made `annot`
  an undoable `pc_command`. Forms is the same
  shape: `field set` is a `pc_command`, `flatten`
  is a `pc_command` — if not, sidecar has no anchor.
- **FDF is the contract.** Browser fill is not the
  target — `FDF import/export` byte-identical is.
  Without it, `tynypdf-cli` cannot be the headless
  repro for a field bug (R-M8).
- **No new dep.** No PDFium/PDFBox for forms without
  ADR + row in `docs/dependency-policy.md` (R-M9).

## What this epic is and is not

**Is:**
- `src/core/forms/` — AcroForm field model,
  fill, validate, flatten, FDF.
- `src/backends/mupdf/forms.cc` — `fz_widget`
  bridge only (no IR mutation outside bridge).
- `src/os/win32/input/` — field focus/tab/key
  wiring (presentation only).
- `docs/a11y/forms.md` — keyboard + UIA
  `ValuePattern` for fields.

**Is not:**
- Not D-3 redaction, D-5 full a11y editor,
  digital signatures — Epics 8+.
- Not browser embedding — no WebView2.
- Not new dep — no form lib without ADR.

## Architecture impact

```
src/core/forms/      # owns field model, fill, FDF
src/backends/mupdf/forms.cc  # owns fz_widget bridge
src/os/win32/input/  # owns tab/focus wiring
src/core/annot/      # reused for flatten appearance
```

**Arrow:** `os -> core(forms) -> backends`
— `grep windows.h src/core` 0,
`grep fz_ src/core/forms` 0 outside bridge,
`layering-check --strict` ≤0.07.

## Acceptance criteria (grounded)

1. **5 stories have pasted evidence in
   `epic-7-dod.md`.** Field fill without headless
   `tynypdf-cli` repro is not a result.
2. **FDF round-trip byte-identical.** `export FDF`
   → `import FDF` → `export FDF` `diff 0` via
   `pc_form_fdf_*` + `sidecar-fmt.py`.
3. **Flatten bakes appearance.** `flatten` produces
   page content that renders equal before/after
   (hash CLI render before vs after pasted).
4. **Layering held.** `layering-check --strict`
   `≤0.07` while forms exists (R-M10).
5. **SPEC with first file.** Each
   `src/features/forms/SPEC.md` written with first
   file in that dir, never before, `spec-check`
   0 orphans.
6. **A11y.** Every field keyboard reachable +
   `ValuePattern` announced, script in `docs/a11y/`
   diffable.

**Traceability:**

| Story | Ref | Aspect |
|-------|-----|--------|
| 7.1 | kickoff D-2, ADR-0011 R-M10 | Field model + FDF |
| 7.2 | kickoff D-2, R-M8 | Fill + validate |
| 7.3 | kickoff D-2, R-M6 | Tab/focus + a11y |
| 7.4 | kickoff D-2, ADR-0007 | Flatten |
| 7.5 | kickoff D-2 + kill | Verdict |

---

*Next: stories 7.1-7.5 in order. Tasks in
`epic-7-technical-tasks.md`.*
