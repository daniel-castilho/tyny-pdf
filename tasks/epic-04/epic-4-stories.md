# Epic 4 – Stories (Acceptance) [grounded]

Each AC cites the real file it comes from. Last bullets
are executable checks. `epic-4-dod.md` requires pasted
command output next to every figure.

| # | Story | Acceptance Criteria (grounded) | Real Reference |
|---|-------|--------------------------------|----------------|
| 4.1 | **Transaction log core — undo/redo over IR** | <br>- `src/core/doc/transaction.h` + `transaction.cc` exist: `pc_txn { pc_doc* doc; vector<Command> undo, redo; pc_budget budget; }` — `Command` holds `type, before, after` as IR value types, never engine handles (R-M4/R-M8)<br>- C API `include/pdfcore/transaction.h`: `pc_txn_create(doc, budget, &txn)`, `pc_txn_apply(txn, cmd)`, `pc_txn_undo(txn)`, `pc_txn_redo(txn)`, `pc_txn_free(txn)` — `detail` static, `size` first field (ADR-0003)<br>- Apply/undo/redo cycle over synthetic doc (null backend, 5 pages) is byte-identical: hash of `pc_doc` IR after `apply; undo; redo` equals hash after `apply`<br>- Budget ceiling enforced: `pc_budget { 1 tile, 1 MB }` + 2 commands -> `PC_ERR_LIMIT` with `"undo budget exceeded"` in `detail`<br>- `grep windows.h src/core` 0; `sh tools/layering-check.sh --strict` ≤0.07; `sh tools/check.sh` green | ADR-0011 R-M4/R-M8, kickoff D-6, docs/coding-standards §3.5, include/pdfcore/transaction.h |
| 4.2 | **Replay + CLI — byte-identical log** | <br>- `src/core/doc/transaction.cc` JSON serialize: `pc_txn_to_json(txn, &str)` and `pc_txn_from_json(str, doc, &txn)` — keys sorted, 2-space, LF (same canonical as sidecar), `unknown keys` preserved<br>- `src/cli/txn.cc` adds `tynypdf-cli txn replay <log.json> --out replayed.pdf.tynypdf.json` — exit 0 on success, 1 corrupt log, 2 usage per ADR-0003 §6<br>- `tests/unit/test_txn_replay.cc` does `apply 5 cmds -> to_json -> from_json -> redo 5` and compares `sha256(to_json)` equal; also `tynypdf-cli txn replay` output equals core `to_json` bytes<br>- Property: `from_json(to_json(x)) == x` for 20 random logs (fuzz seam, not yet libFuzzer)<br>- `sh tools/check.sh` green; `ctest -R txn_replay` green | kickoff D-6, ADR-0003 §6, ADR-0007 §2 canonical, src/cli/SPEC.md |
| 4.3 | **Font fallback per run — no tofu** | <br>- `src/core/text/fallback.cc` exists: `pc_text_run` now carries `font_face` picked per run by scanning `third_party/mupdf` face list; if no face has glyph, returns `PC_ERR_LIMIT` with `detail "missing glyph U+XXXX face N"` and does not render tofu box<br>- Golden `tests/fixtures/text/fallback-ptbr.txt` with `a + U+0301`, `c + U+0327`, `a + U+0303`, `em dash` — fallback picks face per codepoint, `tests/unit/test_text_fallback.cc` asserts face ids<br>- No new dep: reuses `mupdf` face enum via backend vtable `get_face_coverage` (appended capability, R-M3), no harfbuzz/ICU include in `src/core`<br>- `ldd pdfcore` no mupdf; `grep -rn "harfbuzz\\|ICU" src/core` 0; `sh tools/check.sh` green | kickoff D-4, ADR-0011 R-M3/R-M4, docs/coding-standards §3.2 |
| 4.4 | **Break and caret pt-BR — golden** | <br>- `src/core/text/break.cc` completed: UAX #29 grapheme break + pt-BR hyphen exceptions from `tests/fixtures/text/ptbr-break-golden.txt` — `tests/unit/test_text_break.cc` approval golden `text-break-positions.txt` V2<br>- `src/core/text/caret.cc` completed: `caret_left/right` over `e + U+0301` is one visual step, not two codepoints; `a + U+0303` + `o` sequence tested; `tests/unit/test_caret.cc` headless on Linux<br>- ABNT2 dead keys: `tests/fixtures/text/abnt2-golden.txt` — `~ + a -> a with tilde` as one run; `tests/unit/test_caret.cc` covers<br>- No `fitz` text in `src/core`; `ctest -R "break\|caret"` green on `linux-core` | kickoff D-4, ADR-0007 §4, ADR-0010 |
| 4.5 | **Freeze, golden and ratio — sane** | <br>- `include/pdfcore/*.h` extended append-only: `transaction.h` added, `text.h` fallback report documented, `backend.h` unchanged — `pc_` brand-free<br>- Golden `tests/golden/txn_enum.txt` vs `transaction.h` + `tests/unit/test_txn_abi.cc` fails on renumber (R-M12); throwaway renumber proof pasted in `epic-4-dod.md` §4.5<br>- Contract `tests/contract/txn_contract.cc` green for both backends (null/mupdf share IR) — `undo; redo` same IR hash<br>- `sh tools/layering-check.sh --strict` stays `≤0.07` with ~900 new core lines (total ~2000 core, ~180 backend non-vtable)<br>- `sh tools/check.sh` 14/14 + `gates-selftest 13/13` + `ctest 15+` (threaded still <6×) green; `spec-check 0 orphans` — pending now `R18-R21` for txn/text (this epic closes them) leaving only `R15.x` for Epic 5<br>- `sh tools/check.sh` green | ADR-0003, ADR-0011 R-M3/R-M12, kickoff D-6/D-4 |

**Traceability:**

| Story | Ref |
|-------|-----|
| 4.1 | kickoff D-6, R-M8 |
| 4.2 | kickoff D-6, ADR-0003 |
| 4.3 | kickoff D-4, R-M3 |
| 4.4 | kickoff D-4, UAX #29 |
| 4.5 | ADR-0011 R-M3/R-M12 |

---

*Execute 4.1 first — log gates text completion.*
