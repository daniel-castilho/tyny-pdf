# Epic 4 – Technical Tasks [grounded]

This is the only up-to-date plan. `[x]` filled during
execution; evidence in `epic-4-dod.md`. All boxes
unchecked on purpose.

> AI-Guard: every task names allowlist, denylist,
> exact command that proves it, and diff ceiling.
> Follow the list — do not widen scope.

## 0. Pre-flight

- [ ] Read `AGENTS.md` R1-R12, `docs/coding-standards`
  §2/§3, `adr/0011` R-M1..R-M13, `epic-4-overview.md`
- [ ] Read `docs/lessons.md` 2026-09-20/21 (counts +
  stash anti-pattern)
- [ ] Measure baseline in `epic-4-dod.md` §4.0:
  `git rev-parse HEAD`, `git ls-tree -r --name-only HEAD
  | wc -l`, `sh tools/check.sh`, `python3
  tools/spec-check.py`, `sh tools/layering-check.sh
  --strict`, `ctest --preset linux-core --output-on-failure
  | tail -n 20`
- [ ] Branch `feat/epic4-semantics` from `main` (9563263),
  stack `feat/4.1-txn-core` etc, each ≤350 lines,
  squash-merge one at a time

---

## 4.1 Transaction log core — undo/redo over IR

**Allowlist:** `include/pdfcore/transaction.h` (new),
`src/core/doc/transaction.cc/h` (new),
`src/core/doc/CMakeLists.txt`,
`tests/unit/test_txn_*.cc`,
`tests/golden/txn_*.txt`
**Denylist:** `src/backends/*` (read only vtable),
`src/render/*`, `src/os/*`

- [ ] `include/pdfcore/transaction.h`:
  ```c
  typedef struct pc_txn pc_txn;
  pc_status pc_txn_create(pc_doc* doc,
    const pc_budget* budget, pc_txn** out);
  pc_status pc_txn_apply(pc_txn* txn,
    const pc_command* cmd);
  pc_status pc_txn_undo(pc_txn* txn);
  pc_status pc_txn_redo(pc_txn* txn);
  void pc_txn_free(pc_txn* txn);
  ```
  Comment names allocator for every out param.
- [ ] `src/core/doc/transaction.cc` — `Command { enum
  Type { ADD_ANNOT, MOVE, DELETE }; pc_rect before,
  after; char id[11]; }` stored as value types.
  `undo` pops `undo` stack, pushes `redo`, applies
  `before`; `redo` reverse. No engine handle held
  (R-M4). Budget check before push:
  `if (undo.size() > budget->max_tiles) PC_ERR_LIMIT`.
- [ ] IR hash helper `pc_doc_hash(doc)` (sha256 of IR
  boxes + text runs) for test comparison — value-type
  only, no engine.
- [ ] Tests `test_txn_core.cc`: `apply; undo; redo`
  hash equality over null doc (5 pages synthetic);
  budget ceiling fail then pass after raise.
- [ ] Guard: `grep -rEn
  '#include.*windows|mupdf|fitz' src/core` 0 pasted.
  `layering-check --strict` ≤0.07.
- [ ] Gate: `ctest -R txn_core` green; `check.sh` green.
- [ ] Diff ≤350.

## 4.2 Replay + CLI — byte-identical log

**Allowlist:** `src/core/doc/transaction.cc` (extend),
`src/cli/txn.cc` (new), `src/cli/CMakeLists.txt`,
`tests/unit/test_txn_replay.cc`
**Denylist:** `src/backends/*`, `third_party/*`

- [ ] `pc_txn_to_json` / `from_json` — canonical JSON
  (keys sorted, 2-space, LF, 3 decimals) same as
  sidecar. `unknown keys` preserved via
  `std::map<string, json>` round-trip. No new dep;
  reuse sidecar json helper extracted to
  `src/core/json/canonical.cc` (move, not copy).
- [ ] `src/cli/txn.cc` — `tynypdf-cli txn replay
  <log.json> --out <out.json>` — read log, recreate
  doc via null backend, replay, write out. Exit 0
  success, 1 corrupt, 2 usage (ADR-0003 §6).
- [ ] Test `test_txn_replay.cc`: `apply 5 -> to_json ->
  from_json -> redo 5` sha256 equal; CLI replay output
  equals core `to_json` bytes (`diff` 0). Property
  `from_json(to_json(x))==x` over 20 random logs.
- [ ] Gate: `ctest -R txn_replay` green; `check.sh` green.
- [ ] Diff ≤300.

## 4.3 Font fallback per run — no tofu

**Allowlist:** `src/core/text/fallback.cc/h` (new),
`include/pdfcore/text.h` (extend),
`tests/unit/test_text_fallback.cc`,
`tests/fixtures/text/fallback-ptbr.txt`,
`tests/golden/text-fallback-*.txt`
**Denylist:** no `harfbuzz`/`ICU` include, `src/backends/*`

- [ ] Extend `backend.h` vtable with
  `pc_backend_caps { bool has_face_coverage; }` +
  `get_face_coverage(codepoint, &has)` appended
  (R-M3). No renumber.
- [ ] `fallback.cc` — per run scan faces via
  `pc_backend_get_face_coverage`; pick first face
  with glyph. If none, return `PC_ERR_LIMIT` with
  `detail "missing glyph U+XXXX"` and caller skips
  run (no tofu). Test `test_text_fallback.cc` over
  `fallback-ptbr.txt` with `U+0301/0327/0303` +
  `U+2014` — asserts face ids not tofu.
- [ ] Guard: `grep -rn harfbuzz\|ICU\|freetype
  src/core` 0; `ldd pdfcore` no mupdf.
- [ ] Gate: `ctest -R text_fallback` green; `check.sh`
  green.
- [ ] Diff ≤300.

## 4.4 Break and caret pt-BR — golden

**Allowlist:** `src/core/text/break.cc/h`,
`caret.cc/h`, `tests/unit/test_text_break.cc`,
`test_caret.cc`, `tests/fixtures/text/*.txt`,
`tests/golden/text-*.txt`
**Denylist:** `src/backends/*`

- [ ] `break.cc` — UAX #29 grapheme + pt-BR hyphen
  exceptions from `ptbr-break-golden.txt`. Approval
  `tests/golden/text-break-positions.txt` V2
  (hash of positions).
- [ ] `caret.cc` — `caret_left/right(doc, pos)` moves
  one grapheme cluster over combining sequence
  (`e + U+0301` one step). Test `test_caret.cc`
  headless: `e + U+0301` [0,1] not [0,1,2],
  `a + U+0303` + `o` break not inside.
- [ ] ABNT2 `abnt2-golden.txt` — `~ + a -> a with
  tilde` as one run; `test_caret.cc` covers.
- [ ] Gate: `ctest -R "break|caret"` green; `check.sh`
  green.
- [ ] Diff ≤300.

## 4.5 Freeze, golden and ratio — sane

**Allowlist:** `include/pdfcore/*.h`,
`tests/golden/txn_*.txt`,
`tests/unit/test_txn_abi.cc`,
`tests/contract/txn_contract.cc`,
`tests/baseline.json` (read only)
**Denylist:** no new `conan` dep without row in
`docs/dependency-policy.md`

- [ ] Freeze headers: `transaction.h` added,
  `text.h` fallback report documented, `backend.h`
  caps appended — `pc_` brand-free.
- [ ] Golden `tests/golden/txn_enum.txt` vs
  `transaction.h` — `test_txn_abi.cc` fails on
  renumber; throwaway renumber proof pasted in
  `epic-4-dod.md` §4.5 then reverted.
- [ ] Contract `txn_contract.cc` — `TEST_P` over
  null/mupdf, `undo;redo` same IR hash for both.
- [ ] Ratio: `layering-check --strict` stays `≤0.07`
  with ~900 new core lines (total ~1990 core,
  ~180 backend non-vtable). If higher, move helper
  from backends to core.
- [ ] Final gates: `check.sh` 14/14,
  `gates-selftest 13/13`, `ctest 15+` (threaded
  <6x), `spec-check 0 orphans` pending now
  `R15.x` only (4) for Epic 5, plus `R18-R21`
  txn/text closed this epic — paste before/after
  pending counts.
- [ ] Evidence pack in `epic-4-dod.md` §4.5:
  `wc -l` per new file, `grep` 0 proofs, ratio,
  threaded table, golden red proof.

## 4.x Final epic gates

- [ ] `sh tools/check.sh` 14/14 on clean clone
  and PR head
- [ ] `gates-selftest 13/13`
- [ ] `ctest --preset linux-core` 0 failed,
  contract both backends
- [ ] `docs-check` 0, `lang-check` 0,
  `naming-sync` 0, `canonical` 0
- [ ] `spec-check` 0 orphans, pending 4
  (R15.1-R15.4) after 4.5
- [ ] `layering-check --strict` ≤0.07 printed
- [ ] `grep windows.h src/core` 0,
  `grep fz_try src | grep -v bridge` 0

**Checklist (for `epic-4-dod.md` §4.6):**

- [ ] 4.1: txn core undo/redo + budget ceiling
- [ ] 4.2: replay + CLI byte-identical
- [ ] 4.3: fallback per run no tofu
- [ ] 4.4: break + caret pt-BR golden
- [ ] 4.5: freeze + golden + ratio ≤0.07 +
  gates green

---

*Next: do 4.1 first — log gates text completion.*
