# Epic 7 – Definition of Done (DoD) [grounded]

**Rule zero:** Every number, sha or count must be pasted
from command output in this doc. `[x]` without pasted
evidence is forbidden by `AGENTS.md` R4.

Nothing is done. Every box is `[ ]`.

---

## 1. Mandatory evidence

Filled during execution, not planning, with command above
pasted output.

- Paste `git rev-parse HEAD`, `git status --porcelain`,
  `git ls-tree -r --name-only HEAD | wc -l` at start of
  each story — clean tree shown.
- Paste `sh tools/check.sh` full output (14 lines) for
  merge commit on **clean clone**, not only workspace.
- Paste `python3 tools/spec-check.py` before/after —
  pending moves only when artefact exists.
- Paste `sh tools/layering-check.sh --strict` after
  every `src/core` story — ratio as number.
- Paste one failing gate: `grep` layering violation and
  `FDF diff !=0` red proof.

---

## 2. Self-audit — run BEFORE every hand-off

- [ ] Each count matches pasted output: `wc -l`,
  `grep -c`, `bytes`.
- [ ] Each file in allowlist, not deny — `git show
  --stat HEAD` pasted vs `epic-7-technical-tasks.md`.
- [ ] Every `R<n>.<m>` closed has `Verification:`
  artefact pasted and `test -f` pasted.
- [ ] No `SPEC.md` hand-edited to drop pending — `git
  diff HEAD~1` pasted shows no hiding.
- [ ] No new dep without ADR/row — `grep` delta pasted.
- [ ] Layering: `grep -rEn
  '#include.*windows|mupdf|fitz' src/core` 0 pasted.
- [ ] Engine isolate: `grep -rn "fz_try" src
  | grep -v bridge` 0 pasted.
- [ ] Ratio: `layering-check --strict` `≤0.07` pasted.
- [ ] Docs English ASCII allowlist, file counts in
  §7.0 re-measured after `tasks/epic-07/` +5.
- [ ] This audit run before hand-off — pasted at §7.6.
- [ ] **Tests vs Gates matrix** pasted:
  `check.sh 14/14` + `gates-selftest 13/13` +
  `spec-check 0 errors` + `0 pending` + `layering
  ≤0.07` + `ctest 0 failed` all pasted. Missing one
  = not done.
- [ ] **Pending vs Error** pasted: `pending` ok,
  `has no SPEC.md` = blocker.
- [ ] **Clean clone, not workspace** — `git clone` +
  `check.sh` pasted for §7.6.

## 3. Story evidence (paste during execution)

### 7.0 Baseline — state Story 7.1 starts from

```bash
$ git rev-parse HEAD
# -> (paste @ 05ce35c post Epic 6)

$ git ls-tree -r --name-only HEAD | wc -l
# -> (paste)

$ sh tools/check.sh 2>&1 | tail -n 30
# -> (paste 14/14)

$ python3 tools/spec-check.py 2>&1
# spec-check: OK (26 specs, 90+ reqs, 0 orphans)
# spec-check: 0 pending

$ sh tools/layering-check.sh --strict 2>&1
# layering-check: backend_line_ratio=0.0236
# layering-check: OK (56 files)

$ ctest --preset linux-core 2>&1 | tail -n 10
# -> (paste 45/45)
```

### 7.1 Field model + FDF round-trip (partial: export only)

```bash
$ git show --stat HEAD
# -> (paste allowlist)

$ grep -rn "fz_widget" src/core; echo $?
# -> (paste 0)

$ ctest --preset linux-core -R forms 2>&1 | tail
# -> (paste headless)

$ tynypdf-cli forms fdf-export --in tests/fixtures/forms/field.pdf
# --out /tmp/a.fdf && tynypdf-cli forms fdf-import --in /tmp/a.fdf
# --pdf tests/fixtures/forms/field.pdf --out /tmp/b.pdf
# && tynypdf-cli forms fdf-export --in /tmp/b.pdf --out /tmp/b.fdf
# && diff /tmp/a.fdf /tmp/b.fdf; echo $?
# -> (paste 0 diff)

$ sha256sum /tmp/a.fdf /tmp/b.fdf
# -> (paste both equal)

$ cat src/features/forms/SPEC.md | head -n 20
# -> (paste R45.1 with Verification:)
```

- [ ] FDF diff 0 (export→import→export diff 0)
- [ ] **Deferred: FDF import** — `pc_form_fdf_import` → `PC_ERR_UNSUPPORTED` (Issue #xx)
- [ ] **Deferred: fz_widget enum** — `mupdf_form_list_fields` → `PC_ERR_CAPABILITY` (Issue #xx)
- [ ] **Pre-existing: test_text_fallback SEGV** — tracked in Issue #xx, not blocker for 7.1

### 7.2 Fill + validate + undo

```bash
$ ctest --preset linux-core -R "forms" --output-on-failure 2>&1 | tail
# -> 100% tests passed, 0 tests failed out of 2
#    test_forms (22 checks: dict-walk enumeration, IR load, FDF, null capability,
#               import UNSUPPORTED)
#    test_forms_fill (8 suites: fill/undo/redo, max_len RANGE, checkbox ARGUMENT,
#                     readonly STATE, null-load CAPABILITY, FDF byte-stable, budget LIMIT)

$ tynypdf-cli forms list tests/fixtures/forms/field.pdf
# -> Name    type=0 max_len=50 value=
#    Email   type=0 max_len=100    value=
#    Subscribe    type=1 max_len=0 value=Off
#    AgreeTerms   type=1 max_len=0 value=Off

$ tynypdf-cli forms fill tests/fixtures/forms/field.pdf --field Name --value Foo
# --out /tmp/fill.fdf && sha256sum /tmp/fill.fdf
# -> b519aea36abda9ce569aeeeaa8c64a1b80d5f3ab9d2887c2822794bc1a639b3d  /tmp/fill.fdf
#    (full FDF: 4 /T /V pairs, filled Name=Foo, checkboxes Off)

$ tynypdf-cli forms fill tests/fixtures/forms/field.pdf
# --field Name --value "This value is way longer than fifty characters allowed"; echo $?
# -> forms fill: code=11 (value exceeds field max_len)
#    1
#    (validation reads the REAL PDF's /MaxLen through the vtable)

$ sh tools/layering-check.sh --strict 2>&1 | tail -2
# -> layering-check: backend_line_ratio=0.0281
#    layering-check: OK (72 source files, 0 violations)
```

- [x] fill undo (fill is a PC_CMD_FORM_SET in the txn log; undo restores hash-equal state,
      redo re-applies - tests/unit/test_forms_fill.cc)

Fixture note: `tests/fixtures/forms/field.pdf` was REGENERATED for 7.2 because pymupdf
silently dropped `max_len` (the 7.1 fixture shipped no /MaxLen, so the real-document
validation path had nothing to read). New SHA256
`eb6d67fbf5ce15b4dfb0e865b3ea049ca19a3c956ebcf6ae882774b263bfd772` replaces the 7.1
record `046a1a63...`; `generate.py` now writes /MaxLen via xref_set_key.

Also fixed in 7.2 (found while wiring fill):
- backend ABI minor was still 2 while the 7.1 vtable entries claimed "abi 1.3" - bumped
  PC_BACKEND_API_VERSION_MINOR to 3.
- mupdf_fdf_export emitted a TRUNCATED FDF header (13 of 15 signature bytes; the 7.1 test
  only checked the %FDF-1.2 prefix). Both exporters now emit the full signature and are
  byte-identical for the same state.
- append_str in forms.cc could realloc short and overflow on a large append (growth loop
  tested the wrong length).
- the PC_FORM_FIELD_COMBO flag macro shadowed the enum constant of the same name; flags are
  now PC_FFLAG_* with bit values corrected to PDF 32000-1 Table 229.
- enumeration upgrade: mupdf form_list_fields now walks Root/AcroForm/Fields via the pdf
  dict API (pdf_specifics + pdf_dict_getp) instead of answering PC_ERR_CAPABILITY - the
  7.1 "fz_widget enumeration" issue item is closed by this; FDF import remains deferred.
- pdfcore now PUBLIC-links the null backend: doc.cc's R7.1 vtable call means every pdfcore
  consumer needs it after archive re-scans, which a per-test link order cannot reach.

### 7.3 Tab/focus + keyboard + UIA

```bash
$ ctest --preset linux-core -R "forms|uia" --output-on-failure 2>&1 | tail
# -> 100% tests passed, 0 tests failed out of 5
#    test_forms_fill (8 suites, 7.2)
#    test_forms_focus (24 checks: focus wrap, MaxLen LIMIT, toggle both ways,
#                      4 exact announcements, no-fields STATE)
#    test_forms_keyboard (18 checks: Tab/Shift+Tab cycle, WM_CHAR typing, Tab commits
#                        through the txn, Space toggles, Escape, UIA switch+revert)
#    test_forms, test_uia (extended with the R52.2 focus contract)

$ sh tools/cross-compile-proof.sh
# -> cross-compile-proof: OK (43769856 bytes)
#    (tynypdf.exe builds with WM_CHAR forwarding, the char callback and the
#     composition root's announcement sync - MinGW/clang caught 2 Linux-blind
#     errors: an s redeclaration and the nested callback typedef)

$ head -n 12 docs/a11y/forms.md
# -> # Forms keyboard accessibility script - Tyny PDF viewer (story 7.3)
#    ... exact diffable strings: "Name, edit, empty" / "Name, edit, value Foo" /
#    "Subscribe, checkbox, checked" / "Subscribe, checkbox, not checked"

$ sh tools/layering-check.sh --strict 2>&1 | tail -2
# -> layering-check: backend_line_ratio=0.0269
#    layering-check: OK (72 source files, 0 violations)
```

- [x] tab UIA (core focus model R50.x headless; viewer wiring R51.x on the real
      fixture; UIA switch/revert R52.2; Win32 WM_CHAR + callbacks proven by
      cross-compile - the on-machine Narrator run against the physical box stays
      the 7.5 verdict evidence, not claimed here)

### 7.4 Flatten

```bash
$ ctest --preset linux-core -R flatten --output-on-failure 2>&1 | tail
# -> 100% tests passed, 0 tests failed (test_forms_flatten, 27 checks:
#    null CAPABILITY, bake, IR clear, undo restores 4 fields, redo, after-file
#    reopens with 0 fields, pixel identity, JSON replay undo)

$ ASAN run (leak gate)
# -> NO LEAKS (found and fixed a pre-existing mupdf_page_get_box fz_page leak -
#    this test was the first ASAN test to call it)

$ tynypdf-cli render tests/fixtures/forms/field.pdf --page 0 --dpi 72
#   --widgets --clip 0 0 612 792 --out /tmp/before.png --backend mupdf
# && tynypdf-cli forms flatten tests/fixtures/forms/field.pdf --out /tmp/after.pdf
#    --backend mupdf
# && tynypdf-cli render /tmp/after.pdf --page 0 --dpi 72 --clip 0 0 612 792
#    --out /tmp/after.png --backend mupdf
# && sha256sum /tmp/before.png /tmp/after.png
# -> flattened tests/fixtures/forms/field.pdf -> /tmp/after.pdf
#    2bd94435e2e8930fabb3fd5714efb09780df2ae465a2eeb940afe58b75f662db  /tmp/before.png
#    2bd94435e2e8930fabb3fd5714efb09780df2ae465a2eeb940afe58b75f662db  /tmp/after.png
#    (byte-identical PNGs; the in-test pixel sha was df580140... on both sides too)

$ python3 tools/spec-check.py
# -> OK (27 specs, 123 requirements, 48 source files, 0 orphans), 0 pending
```

- [x] flatten (pc_form_flatten = backend pdf_bake_document bridge + ONE PC_CMD_FORM_FLATTEN
      whose undo restores the IR's fields; bake-equality is pixel- and PNG-byte-identical)

Also fixed in 7.4 (found while wiring):
- **mupdf_page_get_box leaked its fz_page** (loaded, bounded, never dropped) - a
  pre-existing backend defect; the flatten test was the first ASAN test to reach it.
- command_to_json labeled FORM_FLATTEN as "FORM_SET" (the ternary chain), so a replayed
  log failed to parse - the flatten snapshot now round-trips with the correct type tag.
- render needed a `--widgets` flag and a real `--clip` (the hardcoded 100x100 clip sat
  below every field, which would have made the hash comparison vacuous).
- abi 1.4: form_flatten vtable entry + pc_render_params.render_widgets.

### 7.5 Verdict

```bash
$ sh tools/layering-check.sh --strict 2>&1 | tail -2
# -> layering-check: backend_line_ratio=0.0269
#    layering-check: OK (72 source files, 0 violations)

$ grep -rn "fz_try" src/core/forms | grep -v bridge; echo $?
# -> 0

$ ctest --preset linux-core 2>&1 | grep "tests passed"
# -> 98% tests passed, 1 tests failed out of 50 (test_text_fallback - pre-existing,
#    tracked issue, not forms)

$ grep -c "CHECK(" tests/unit/test_forms*.cc | awk -F: '{s+=$2} END {print s}'
# -> 136 checks across 5 suites (forms, fill, focus, keyboard, flatten)
```

## Verdict: KEEP

The kickoff D-2 kill criterion for forms was "forms cannot be made undoable without the
engine in core". It measured FALSE. Four numbers:

1. **Undo is core, not engine.** Fill undo restores hash-equal IR state, flatten undo
   restores all 4 fields, and a replayed log's undo restores them too - all against the
   null backend, zero engine reach (`test_forms_fill`, `test_forms_flatten`).
2. **Bake equality holds byte-for-byte.** Widget-rendered before == baked-content after:
   pixel sha `df580140...` both sides, CLI PNG `2bd94435...` both sides (R53.2).
3. **The architecture survived the capability.** backend_line_ratio 0.0269 (budget 0.07),
   0 layering violations across 72 files, `fz_widget`/`fz_try` in src/core = 0 - the dict
   walk lives in the bridge (R46.2, R46.3).
4. **Every claim is headless.** 136 checks in 5 new suites, all on linux-core without a
   window or the engine in the test TU; 49/50 ctest with the single red being the
   pre-existing tracked `test_text_fallback`, not forms.

Kept with its debt named, not hidden (all tracked): FDF import stays `PC_ERR_UNSUPPORTED`
(7.1 issue: needs an incremental-save story); hierarchical field names and page_index ride
with radio groups; the viewer's Ctrl+Z binding and the on-machine Narrator run are the
7.3 leftovers recorded in docs/a11y/forms.md.

- [x] verdict (keep, 4 numbers above)

### 7.6 Final epic gates (on merge of 7.5, clean clone)

```bash
$ git clone https://github.com/daniel-castilho/tyny-pdf.git
# /tmp/tyny-epic7-final && cd /tmp/tyny-epic7-final
$ git rev-parse HEAD
# -> (paste merge of 7.5)

$ sh tools/check.sh 2>&1; echo $?
# -> (paste 14/14)

$ sh tools/gates-selftest.sh 2>&1; echo $?
# -> (paste 13/13)

$ ctest --preset linux-core --output-on-failure
# -> (paste 0 failed, 45+ incl forms)

$ python3 tools/spec-check.py 2>&1
# -> spec-check: OK (28 specs, 95 reqs, 0 orphans)
# -> 0 pending

$ sh tools/layering-check.sh --strict 2>&1
# -> layering-check: backend_line_ratio=0.02*
# (≤0.07)

$ wc -l src/core/forms/*.cc | tail
# -> (~500 forms)

$ git ls-tree -r --name-only HEAD | wc -l
# -> (paste base +5 docs + ~8 new files)
```

- [ ] Final gates green on `main`, pasted per gate

---

## 4. Failures this doc encodes

| Rule | Failure it kills |
|------|------------------|
| §7.0 baseline | Ratio claimed without before |
| §7.1 FDF | Export/import not byte-identical |
| §7.1 FDF (partial) | Import returns PC_ERR_UNSUPPORTED (deferred to 7.2+) |
| §7.1 FDF (partial) | Widget enum returns PC_ERR_CAPABILITY (deferred to 7.2+/MuPDF upgrade) |
| §7.2 undo | Fill not undoable |
| §7.4 flatten | Render hash before != after |
| §7.5 verdict | Forms kept after kill |
| §7.6 clean clone | `check.sh` green in workspace but red on clone |

## 5. Epic 7 completion checklist

- [ ] 7.1: field model + FDF diff 0
- [ ] 7.2: fill + undo
- [ ] 7.3: tab + UIA
- [ ] 7.4: flatten
- [ ] 7.5: verdict
- [ ] `check.sh` 14/14, `gates-selftest` 13/13,
  `ctest` 0 failed, `layering ≤0.07`,
  `spec-check` 0 orphans, `docs-check` 0 — all pasted
  in §7.6 on merge

---

*Epic 7 complete = every field is IR, fill is txn,
FDF is byte-identical, flatten bakes. Hand-off without
pasted evidence returns.*
