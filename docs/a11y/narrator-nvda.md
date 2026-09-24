# Narrator & NVDA announced-text script — Tyny PDF viewer (M1 a11y criterion, story 5.5)

Run against the physical Windows machine with a hardware keyboard. This script
records the *exact* announced text for the viewer window so that a regression in
the announcement is a diff, not an impression (kickoff §M5: "scripts (manual,
recorded in `docs/a11y/`) with the exact announced text").

The viewer has no page content yet (story 1.5 baseline), so the script drives the
document state the UI owns: 5 pages, page 1 visible, zoom 150%. The announced
sentence comes from the window's UIA ValuePattern, whose string is produced by
`pc_uia_format_announcement` and pinned verbatim by `tests/unit/test_uia.cc`.

## State fixed by this script

- Document: 5 pages, current page 1, zoom 150%.

## Part A — Narrator (Win+Ctrl+Enter)

1. Start Narrator (`Win+Ctrl+Enter`).
2. Launch `build/win-cross-x64/Release/tynypdf.exe`.
3. The window takes focus; Narrator announces the focused element. Record the
   verbatim sentence into the DOD below.
4. Expected:

   ```
   "Page 1 of 5, zoom 150%"
   ```

5. Move focus: `Tab` away and `Shift+Tab` back — Narrator re-announces the same
   sentence. Record that it is identical to step 4 (no trailing "group",
   "button" or coordinate suffix).

## Part B — NVDA (Insert+F1)

1. Start NVDA.
2. Launch `build/win-cross-x64/Release/tynypdf.exe`.
3. Move focus onto the window; NVDA announces it. Record verbatim.
4. Expected, identical to Narrator's:

   ```
   "Page 1 of 5, zoom 150%"
   ```

5. `NVDA+f` reports the focused "read-only value" — record that it matches the
   value in step 4 exactly (the ValuePattern is read-only; `get_IsReadOnly` is
   TRUE, so NVDA says "read-only" and never offers editing).

## Expected result (paste into epic-5-dod.md §5.5)

```
narrator: "Page 1 of 5, zoom 150%"
narrator re-announce after Tab away/back: identical
nvda: "Page 1 of 5, zoom 150%"
nvda read-only value: "Page 1 of 5, zoom 150%" (read-only)
```

## Reference

- The announced sentence is engine-free and Windows-free: it is formatted by
  `pc_uia_format_announcement` and asserted by `tests/unit/test_uia.cc` on
  Linux, so the scripts check the *machine's* rendering of that contract, not
  the contract itself.
- The UIA tree the screen reader walks: `src/os/win32/uia/uia.cc`
  (window fragment with page/zoom/focus children, ValuePattern on the root).
