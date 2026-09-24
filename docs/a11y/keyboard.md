# Keyboard accessibility script — Tyny PDF viewer (M1 a11y criterion, story 5.5)

Run against the physical Windows machine (Narrator or NVDA closed; this script
verifies keyboard reachability with the OS focus rectangle only). The viewer has
no page content yet (story 1.5 baseline, debt item 1), so the script fixes the
document state that the UI owns and asserts the announced value through the
window's UIA ValuePattern.

Expected: every control reachable with Tab/arrows; the window announces its
document state ("Page 1 of 5, zoom 150%") when focus lands on it. The exact
announced text is diffable — it is the output of `pc_uia_format_announcement`
(pinned by `tests/unit/test_uia.cc`), not a free-form description.

## State fixed by this script

- Document: 5 pages, current page 1, zoom 150% (set through the viewer's
  document state; the a11y selftest drives the same values).

## Steps

1. Launch `build/win-cross-x64/Release/tynypdf.exe`.
2. Press `Tab` until the window content has focus (the window is the only
   top-level element; one Tab lands on it).
3. Assert the focus rectangle is visible around the client area (no off-screen
   region, not clipped by the taskbar).
4. Press `Tab` again — focus stays on the window (the viewer has no secondary
   chrome to tab into; this is the expected single-stop tree).
5. Press `Shift+Tab` — focus returns to the window.
6. Press `F6` — no change (the viewer has one pane; documented, not a defect).
7. The window's UIA ValuePattern announces exactly:

   ```
   Page 1 of 5, zoom 150%
   ```

## Expected result (paste into epic-5-dod.md §5.5)

```
keyboard: Tab lands on window, focus rectangle visible, Shift+Tab returns
keyboard: no control is reached twice or skipped (single-stop tree)
uia value: "Page 1 of 5, zoom 150%"
```

## Reference

- Narrative contract: `include/pdfcore/uia.h` (`pc_uia_format_announcement`, the
  exact string shape), `src/os/win32/uia/uia_state.cc` (the formatter).
- Pinned test: `tests/unit/test_uia.cc` asserts the identical string on Linux.
- The announcement style ("Page N of M, zoom Z%") matches `docs/kickoff.md` §M5
  and the a11y design note in `docs/a11y/narrator-nvda.md`.
