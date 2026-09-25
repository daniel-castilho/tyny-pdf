# Epic 7 – Testing Strategy [grounded]

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

## 7.1 Field model + FDF

- **Goal:** FDF diff 0 headless.
- **Unit:** `test_forms.cc` field list from
  `tests/fixtures/forms/field.pdf`, `ctest -R forms`.
- **Round-trip:** `export → import → export`
  `diff 0`, `sha256` both.
- **Guard:** `grep fz_widget src/core` 0.
- **Cmd:** `check.sh` + `ctest` + `layering`.

## 7.2 Fill + undo

- **Goal:** fill is txn.
- **Unit:** `test_forms_txn.cc` 5 undo/5 redo,
  `ctest -R forms`.
- **Contract:** `tynypdf-cli forms fill` `sha256`
  CLI vs window equal.
- **Guard:** `fz_try` only in `bridge`.
- **Cmd:** `check.sh` green.

## 7.3 Tab + UIA

- **Goal:** keyboard + ValuePattern.
- **Manual:** `docs/a11y/forms.md` exact text
  diffable.
- **Unit:** `ctest -R forms` tab cycle headless.
- **Cmd:** `check.sh` green.

## 7.4 Flatten

- **Goal:** bake + hash equal.
- **Unit:** `test_flatten.cc` headless, render
  hash before vs after equal.
- **Guard:** `fz_try` only in `bridge`.
- **Cmd:** `check.sh` green.

## 7.5 Verdict

- **Goal:** keep or kill.
- **Verdict:** keep (4 numbers) or kill (ADR +
  table, code deleted).
- **Cmd:** `check.sh` green on last commit.

## Regression (per story + end)

- [ ] `check.sh` after every story, on clean clone
- [ ] `docs-check` 0 with 5 new epic docs
- [ ] `spec-check` 0 orphans, 0 pending
- [ ] `naming-sync` 0, `lang-check` 0
- [ ] `layering --strict` ≤0.07
- [ ] `ctest 0 failed`, contract both backends

**Checklist:**

- [ ] 7.1 FDF diff 0
- [ ] 7.2 fill undo
- [ ] 7.3 tab UIA
- [ ] 7.4 flatten
- [ ] 7.5 verdict
- [ ] Final gates green on `main`

---

*Run as each story executes, paste into `epic-7-dod.md`.*
