# Epic 7 – Technical Tasks [grounded]

This is the only up-to-date plan. `[x]` filled during
execution; evidence in `epic-7-dod.md`.

> AI-Guard: allowlist/denylist + exact command per task
> + diff ceiling ≤350. Follow the list.

## 0. Pre-flight

- [ ] Read `AGENTS.md` R1-R12, `docs/coding-standards`
  §2, `adr/0011` R-M10/R-M11, `epic-7-overview.md`
- [ ] Read `docs/lessons.md` 2026-09-24
- [ ] Measure baseline in `epic-7-dod.md` §7.0:
  `git rev-parse HEAD`, `git ls-tree -r --name-only
  HEAD | wc -l`, `sh tools/check.sh`,
  `python3 tools/spec-check.py`,
  `sh tools/layering-check.sh --strict`,
  `ctest --preset linux-core | tail -n 20`
- [ ] Branch `feat/epic7-forms` from `main`,
  stack `feat/7.1-field-model` etc, each ≤350,
  squash-merge one at a time

---

## 7.1 Field model + FDF round-trip

**Allowlist:** `src/core/forms/*`,
`src/backends/mupdf/forms.cc`,
`src/features/forms/SPEC.md` (new),
`tests/fixtures/forms/*` (new)
**Denylist:** `src/render/*`, `src/os/*` (no field)

- [ ] `src/core/forms/forms.cc` — `pc_form_field`
  enum `TEXT/CHECK/RADIO/COMBO` + `pc_form_list`
  from `pc_doc` IR. `pc_form_fdf_export/import`
  byte-identical via canonical JSON (keys sorted).
- [ ] Bridge: `src/backends/mupdf/forms.cc` —
  `fz_widget` → `pc_form_field` only in `bridge`,
  `grep -rn "fz_widget" src/core` 0 pasted.
- [ ] FDF: `export → import → export` `diff 0` —
  `sha256sum` both pasted, `sidecar-fmt.py` ok.
- [ ] SPEC: `src/features/forms/SPEC.md` with first
  file, `R45.1` with `Verification: FDF diff 0` —
  `spec-check` 0 orphans.
- [ ] Gate: `layering-check --strict` ≤0.07,
  `check.sh` green; diff ≤350.

## 7.2 Fill + validate + undo

**Allowlist:** `src/core/forms/*`,
`tests/unit/test_forms_txn.cc` (new),
`src/cli/forms.cc` (extend)
**Denylist:** `src/render/*` (no IR mutation)

- [ ] `pc_form_set_value(field, value)` —
  validate `maxLen/format`, create `pc_command`
  `FORM_SET`; `pc_txn_undo/redo` restores —
  `test_forms_txn.cc` 5/5 pasted.
- [ ] CLI: `tynypdf-cli forms fill --field X
  --value Y --out` — `sha256` CLI vs window
  pasted equal.
- [ ] Gate: `check.sh` green; diff ≤350.

## 7.3 Tab/focus + keyboard + UIA

**Allowlist:** `src/os/win32/input/*`,
`docs/a11y/forms.md` (new)
**Denylist:** `src/core/*` (no windows.h)

- [ ] `src/os/win32/input/input.cc` — `TAB`
  cycles fields, `SPACE` toggles checkbox,
 typing fills text field, `Present(0,0)` headless.
- [ ] A11y: `docs/a11y/forms.md` with exact
  `ValuePattern` announced text diffable.
- [ ] Gate: `check.sh` green; diff ≤350.

## 7.4 Flatten

**Allowlist:** `src/core/forms/flatten.cc`,
`tests/unit/test_flatten.cc` (new)
**Denylist:** `src/render/*`

- [ ] `pc_form_flatten(doc)` — bake appearance
  streams into page content, remove fields,
  as `pc_command` — `test_flatten.cc` headless.
- [ ] Render hash before vs after pasted equal;
  `sidecar-fmt.py` `diff 0` after flatten.
- [ ] `grep -rn "fz_try" src/core/forms` 0
  outside `bridge` pasted.
- [ ] Gate: `check.sh` green; diff ≤350.

## 7.5 Verdict

**Allowlist:** `adr/*` (if kill),
`docs/lessons.md`
**Denylist:** keep forms code only if keep

- [ ] Verdict: (a) 4 numbers table + forms kept;
  (b) kill — new ADR revisits D-2 with table,
  forms code deleted in same PR.
- [ ] `docs/lessons.md` updated in same PR.
- [ ] Gate: `check.sh` green on last commit.

## 7.x Final epic gates

- [ ] `sh tools/check.sh` 14/14 on clean clone
  and PR head
- [ ] `gates-selftest 13/13`
- [ ] `ctest --preset linux-core` 0 failed,
  selection/search/annot/forms
- [ ] `docs-check` 0, `lang-check` 0, `naming 0`,
  `canonical 0`
- [ ] `spec-check` 0 orphans, 0 pending
- [ ] `layering-check --strict` ≤0.07
- [ ] `grep windows.h src/core` 0,
  `grep fz_try | grep -v bridge` 0

**Checklist (for `epic-7-dod.md` §7.6):**

- [ ] 7.1: field model + FDF diff 0
- [ ] 7.2: fill + undo
- [ ] 7.3: tab + UIA
- [ ] 7.4: flatten
- [ ] 7.5: verdict
- [ ] Final gates green on `main`

---

*Next: do 7.1 first — model not trusted.*
