# Epic 4 – Testing Strategy [grounded]

Levels from `docs/testing-playbook.md` §2.
Everything names command and artefact.

> AI-Guard: each level has guard for AI mistake.

## Gates (every PR)

- Repo: `sh tools/check.sh` 14/14;
  `gates-selftest 13/13`. Guard: AI rewraps count 11->14
  → `docs-check` catches.
- Style: `sh tools/format-check.sh`. Guard: engine
  include in `src/core` → `layering-check` red.
- Language: `python3 tools/lang-check.py` 0.
- Naming: `python3 tools/naming-sync.py check` 0.
- Spec: `python3 tools/spec-check.py` 0 orphans;
  `Verification:` must exist. Guard: mark R done without
  artefact → pending stays.

## 4.1 Transaction log core

- **Goal:** undo/redo correct and budget-gated on IR.
- **Unit:** `test_txn_core.cc` — apply/undo/redo hash
  equality over null doc 5 pages; budget ceiling 1 tile
  + 2 cmds → `PC_ERR_LIMIT`. Headless on Linux.
- **Contract:** `txn_contract.cc` `TEST_P` over
  null/mupdf — same IR hash after cycle for both.
- **Negative:** `grep windows.h src/core` 0; move
  include to core on throwaway → `layering` red.
- **Cmd:** `ctest -R txn_core` + `layering-check
  --strict`

## 4.2 Replay + CLI

- **Goal:** log replay byte-identical via core and CLI.
- **Unit:** `test_txn_replay.cc` — 5 cmds -> json ->
  from_json -> redo 5 sha256 equal; CLI replay output
  equals core json bytes.
- **Property:** `from_json(to_json(x))==x` over 20
  random logs.
- **CLI:** `tynypdf-cli txn replay log.json --out out
  .json` exit 0/1/2 per ADR-0003 §6.
- **Cmd:** `ctest -R txn_replay` + `diff` of bytes.

## 4.3 Font fallback per run

- **Goal:** no tofu, per-run face pick.
- **Unit:** `test_text_fallback.cc` over
  `fallback-ptbr.txt` with U+0301/0327/0303/U+2014 —
  asserts face ids; missing glyph → `PC_ERR_LIMIT`.
- **Guard:** `grep harfbuzz|ICU src/core` 0; `ldd`
  pdfcore no mupdf.
- **Cmd:** `ctest -R text_fallback`

## 4.4 Break and caret pt-BR

- **Goal:** grapheme step correct headless.
- **Unit:** `test_text_break.cc` approval
  `text-break-positions.txt` V2; `test_caret.cc`
  `e+U+0301` one step, `a+U+0303` break;
  `abnt2-golden.txt`.
- **Windows-specific:** none headless — IME in Epic 5.
- **Cmd:** `ctest -R "break|caret"`

## 4.5 Freeze, golden and ratio

- **Goal:** ABI append-only, contract, ratio healthy,
  baseline honest.
- **Unit:** `test_txn_abi.cc` vs
  `tests/golden/txn_enum.txt` — renumber fails;
  throwaway proof pasted.
- **Contract:** `txn_contract.cc` both backends
  `undo;redo` same hash.
- **Layering:** `layering-check --strict` ≤0.07;
  `grep fz_try | grep -v bridge` 0.
- **Gates final:** `check.sh 14/14`, `gates-selftest
  13/13`, `ctest 15+` (threaded <6x),
  `spec-check 0 orphans` pending 4 (R15.x).
- **Guard:** new `conan` dep without row →
  `docs-check` fails.

## Regression (per story + end)

- [ ] `check.sh` after every story, on clean clone
  before merge
- [ ] `docs-check` 0 with 5 new epic docs
- [ ] `spec-check` 0 orphans, pending 4 after 4.5
- [ ] `naming-sync` 0, `lang-check` 0
- [ ] `layering --strict` ≤0.07
- [ ] `ctest 0 failed`, contract both backends

**Checklist:**

- [ ] 4.1 txn core + budget
- [ ] 4.2 replay byte-identical
- [ ] 4.3 fallback no tofu
- [ ] 4.4 break/caret golden
- [ ] 4.5 freeze + golden + ratio
- [ ] Final gates green on `main`, pasted per level

---

*Run as each story executes, paste into `epic-4-dod.md`.*
