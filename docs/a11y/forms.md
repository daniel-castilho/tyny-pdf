# Forms keyboard accessibility script — Tyny PDF viewer (story 7.3)

Run against the physical Windows machine with the form fixture
(`tests/fixtures/forms/field.pdf`, SHA256 `eb6d67fb...`). This script verifies
that every form field is keyboard reachable and that the UIA ValuePattern
announces the focused field with the exact text below. The announced text is
diffable — it is the output of `pc_form_focus_announce` (pinned by
`tests/unit/test_forms_focus.cc` and `tests/unit/test_forms_keyboard.cc`),
not a free-form description.

## Keyboard contract

- `Tab` commits the typing buffer and moves focus to the next field (document
  order, wrapping at the ends). `Shift+Tab` moves back.
- Typing (WM_CHAR) appends to the focused text field's buffer; the field's
  `/MaxLen` rejects an over-limit character (the buffer keeps its previous
  content - the keystroke is dropped, never the field).
- `Space` toggles a checkbox (`Off` <-> `Yes`, one undo step); on a text field
  it types one space.
- `Escape` discards the buffer without committing (the committed value stays).
- Every commit and toggle is one command in the transaction log: `Ctrl+Z`
  (once wired to the viewer, story 7.3+ undo binding) undoes per field, not per
  keystroke.

## Steps

1. Launch `build/win-cross-x64/Release/tynypdf.exe
   tests/fixtures/forms/field.pdf`.
2. Press `Tab` — focus lands on `Name`; the window announces exactly:

   ```
   Name, edit, empty
   ```

3. Type `Foo` — the announcement updates as the buffer changes:

   ```
   Name, edit, value Foo
   ```

4. Press `Tab` — commits (Name = Foo), focus moves to `Email`; announces:

   ```
   Email, edit, empty
   ```

5. Press `Tab` twice — focus lands on `Subscribe` (checkbox, Off); announces:

   ```
   Subscribe, checkbox, not checked
   ```

6. Press `Space` — toggles to Yes; announces:

   ```
   Subscribe, checkbox, checked
   ```

7. Press `Shift+Tab` — focus returns to `Email` without wrapping.
8. Press `Escape` after typing into `Email` — the buffer reverts to the
   committed value; no command is pushed.

## Expected result (paste into epic-7-dod.md §7.3)

```
forms keyboard: every field Tab/Shift+Tab reachable (4 fields, wraps)
forms keyboard: typing fills buffer, MaxLen drops over-limit chars
forms keyboard: Space toggles checkbox (one command, undoable)
forms a11y: ValuePattern announces exact "Name, edit, ..." strings above
forms a11y: blur reverts the announcement to "Page N of M, zoom Z%"
```

## Notes

- The four announcement shapes are the only accepted texts for a focused
  field; changing one is a script change, not a refactor.
- While no field is focused, the window keeps announcing the page state
  ("Page N of M, zoom Z%", R24.7) — the forms announcement is an override, not
  a replacement.
