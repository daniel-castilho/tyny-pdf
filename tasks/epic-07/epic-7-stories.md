# Epic 7 – Stories (Acceptance) [grounded]

Each AC cites real file it comes from. Last bullets are
executable checks. `epic-7-dod.md` requires pasted output.

| # | Story | Acceptance Criteria (grounded) | Real Reference |
|---|-------|--------------------------------|----------------|
| 7.1 | **Field model + FDF round-trip** | <br>- `src/core/forms/forms.cc` owns `pc_form_field` (text/checkbox/radio/combo) from `pc_doc` IR; `pc_form_fdf_export/import` byte-identical — `export → import → export` `diff 0` pasted<br>- `src/backends/mupdf/forms.cc` bridges `fz_widget` only — `grep -rn "fz_widget" src/core` 0 pasted<br>- `src/features/forms/SPEC.md` with first file, `R45.1` with `Verification:` and `spec-check` 0 orphans<br>- `layering-check --strict` 0; `sh tools/check.sh` green | kickoff D-2, ADR-0011 R-M10 |
| 7.2 | **Fill + validate + undo** | <br>- `pc_form_set_value(field, value)` validates (maxLen, format) and is a `pc_command` — `pc_txn_undo/redo` restores field + appearance — `test_forms_txn.cc` 5/5 pasted<br>- Headless `tynypdf-cli forms fill --field Name --value Foo --out` reproduces same `FDF` as window fill — `sha256` CLI vs window pasted equal<br>- `sh tools/check.sh` green | kickoff D-2, ADR-0007, ADR-0003 §6 |
| 7.3 | **Tab/focus + keyboard + UIA** | <br>- `src/os/win32/input/input.cc` maps `TAB/SHIFT+TAB` → field focus, `SPACE` → checkbox, typing → text field, `Present(0,0)` headless still works<br>- Every field keyboard reachable; `ValuePattern` announced — `docs/a11y/forms.md` with exact text diffable; `ctest -R forms` on linux-core<br>- `sh tools/check.sh` green | kickoff D-2, docs/a11y |
| 7.4 | **Flatten (bake appearance)** | <br>- `pc_form_flatten(doc)` bakes field appearances into page content, removes field, is a `pc_command` — `test_flatten.cc` headless pasted<br>- Render hash before vs after `flatten` pasted equal (appearance streams match); `sidecar-fmt.py` `diff 0` after flatten round-trip<br>- `grep fz_try src/core/forms` 0 outside `bridge`; `sh tools/check.sh` green | kickoff D-2, ADR-0007 |
| 7.5 | **Verdict** | <br>- Verdict is one of two shapes: (a) 4 numbers in `dod.md` and forms kept; (b) kill fired (forms cannot be undoable without engine in core) and new ADR revisits D-2 with table, forms code deleted in same PR<br>- `docs/lessons.md` updated in same PR as verdict (kickoff §9 step 6)<br>- `sh tools/check.sh` + `gates-selftest` green on last commit | kickoff D-2 + kill |

**Traceability:**

| Story | Ref |
|-------|-----|
| 7.1 | Field model FDF |
| 7.2 | Fill undo |
| 7.3 | Tab UIA |
| 7.4 | Flatten |
| 7.5 | Verdict |

---

*Story 7.1 first — model not trusted, no fill.*
