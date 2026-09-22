# Epic 4: Semantics — Undo Transaction Log and Text Engine (D-6 + D-4)

**Project:** tyny-pdf
**Context:** C++20 core behind a C ABI (`pdfcore`),
Win32 + Direct2D presentation, LLVM-MinGW cross build
from Linux, `tynypdf-cli` as CI transport
(ADR-0001, ADR-0002, ADR-0011), MuPDF 1.26.8 vendored.
**Goal:** make semantics undoable and text correct.
After this epic, every mutation of document, annotation
and text is a command in a log that replays
byte-identically on Linux via `tynypdf-cli`
(R-M8), and pt-BR text breaks and caret move over
combining marks without tofu.

---

## Repo state (measured 2026-09-21, execution time)

Measured on `main @ 9563263` — tree this epic starts
from. Every number pasted from command output in
`epic-4-dod.md` §4.0.

- **14/14 gates green.** `sh tools/check.sh` — 14 sections,
  `gates-selftest 13/13`, `spec-check 12 specs / 41 reqs /
  0 orphans / 4 pending (R15.1-R15.4)`, `layering 0.0686`
  on 27 files, 0 violations.
- **Core heart beats.** `src/core` now ~1090 lines (doc,
  geom, sha256, sidecar reader/writer/stale) with
  `pdfcore` API frozen at 1.0 and `backend_line_ratio`
  sanado (0.0683 → 0.0686 in merge). No `text/` or
  `budget/` module exists yet — 4.3 creates the text
  module; the RSS ceiling lives inside the transaction
  log (4.1/4.2).
- **Sidecar R1-R6 closed.** `writer.cc` atomic tmp->rename
  + fsync, `lock` 5-min, `R2.2` version gate, `R2.3`
  base32 ids, `R5.1/5.2` stale (fingerprint vs mtime),
  `R4.1` unknown keys, `R6` exclusions — all with
  golden `sidecar-stale-report.txt`.
- **Baseline honest.** `tests/baseline.json` re-measured
  with MuPDF native in Release (open 3.24 vs 3.34 ms
  Delta 3.1%, scroll 15.01 vs 15.68 ms Delta 4.5%, <10%),
  machine_spec from reference box.
- **Still missing for v1:** undo log (D-6), font fallback
  per run, pt-BR line break corpus, caret over
  grapheme clusters, `pc_txn_*` public API — all
  this epic. Render/os window (R14/R15) stays
  pending for Epic 5 (M1 spike).

## Why this epic now

- **IR without undo is append-only.** `epic-3-dod.md` §3.6
  left IR testable but not reversible. `kickoff.md` §4
  orders D-6 first for a reason: without `pc_txn_undo`,
  sidecar merge and redaction are irreversible and
  `git merge` of two sidecars must pick a side.
- **Text without fallback is tofu.** There is no
  `src/core/text` module yet — not NFKC, not break, not
  caret. Story 4.3 creates the module with
  `font fallback per run` and `missing glyph report`;
  4.4 adds break and caret. D-4 closes the pt-BR
  promise before forms (D-2) and search need it.
- **Undo gates the next two deltas.** D-2 (forms) and
  D-1 (sidecar re-anchoring ladder) both become
  transactions. Doing them before the log would put
  reconciliation in the UI (R-M10 violation).
- **Risk order.** `kickoff.md` §10 tripwire
  "performance unreachable" is mitigated by the tile
  budget from Epic 3; this epic's `pc_budget`
  + undo memory ceiling is the lever.

## What this epic is and is not

**Is:**
- `src/core/doc/transaction.cc` — command log, undo/redo
  stacks, `pc_txn_*` C API, JSON serialization for
  `tynypdf-cli txn replay` byte-identical.
- `src/core/text` creation — fallback face per run,
  tofu → `PC_ERR_LIMIT` with glyph report, pt-BR
  break golden, ABNT2, caret over combining marks.
- Public API extension by appending to vtable
  (`pc_backend` unchanged, `pc_txn` new seam —
  R-M3 append-only).

**Is not:**
- Not `src/render` nor `src/os/win32` window — Epic 5
  (M1 spike).
- Not D-2 form rules, D-5 UIA, D-1 re-anchoring ladder,
  D-3 redaction proof — Epics 5/6.
- Not a second backend or new dep — R-M9 holds;
  `font fallback` probes `third_party/mupdf` bundled
  fonts through a curated face table on the mupdf
  adapter side (the engine exposes no enumeration API),
  no ICU/harfbuzz dep without ADR.

## Architecture impact

```
include/pdfcore/      # append-only after 3.5, now extended
  transaction.h  — pc_txn_* (new, versioned)
  text.h         — pc_text_run + fallback report (new, 4.3)

src/core/
  doc/transaction.cc/h  — NEW, owns undo semantics (R-M8)
  text/fallback.cc/h    — NEW, per-run face pick
  text/break.cc/h       — NEW (4.4), pt-BR golden
  text/caret.cc/h       — NEW (4.4), grapheme step
  (RSS ceiling lives inside doc/transaction.h — no budget/)

src/cli/
  txn.cc — NEW, `tynypdf-cli txn replay <log.json>`

tests/
  unit/test_txn_*.cc + test_text_*.cc + approvals/
```

**Arrow after epic:** `app/cli -> pdfcore(transaction+text) -> backends`
— `grep windows.h src/core` 0, `grep fz_ src/core` 0,
`layering-check --strict` ≤0.07 still.

## Acceptance criteria (grounded)

1. **`pc_txn_*` exists and is undoable on IR.** Create,
   apply, undo, redo over `src/core/doc` IR; same
   sequence replays byte-identically via
   `tynypdf-cli txn replay` on Linux (R-M8).
2. **Budget gates undo.** Transaction log respects
   `pc_budget` RSS/tile ceiling; shrinking ceiling fails
   a unit test that then passes after fix.
3. **Text fallback correct.** Missing glyph never renders
   tofu; returns `PC_ERR_LIMIT` with face report and
   golden corpus passes pt-BR.
4. **Caret and break correct.** Grapheme step over
   combining sequence is one visual step, headless,
   with approval golden `text-caret-positions.txt` V2.
5. **Layering healthy.** `backend_line_ratio` stays
   ≤0.07 with ~900 new core lines, `grep` gates 0.
6. **Rule zero.** Every number in `epic-4-dod.md` pasted
   from command output.

**Traceability:**

| Story | Ref | Aspect |
|-------|-----|--------|
| 4.1 | ADR-0011 R-M8, kickoff D-6 | Transaction log core |
| 4.2 | kickoff D-6, docs/coding-standards §3.5 | Replay + CLI |
| 4.3 | kickoff D-4, ADR-0007 §4 | Fallback + tofu report |
| 4.4 | kickoff D-4, ADR-0010 | Break + caret pt-BR |
| 4.5 | ADR-0003, ADR-0011 R-M3/R-M12 | Freeze + golden + ratio |

---

*Next: stories 4.1-4.5 in order — log before text
completion. Tasks in `epic-4-technical-tasks.md`,
evidence in `epic-4-dod.md`.*
