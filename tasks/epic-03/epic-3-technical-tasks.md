# Epic 3 – Technical Tasks [grounded]

This document is the only complete and up-to-date version of the plan. `[x]` marks
are filled during
execution; evidence is pasted in `epic-3-dod.md`. All boxes are unchecked on purpose:
nothing in
this epic has been executed yet.

> **Improvement over Epics 1-2 for AI execution:** every task below names the exact
files it may
touch, the exact files it must not touch, the exact command that proves it, and the
exact diff
ceiling. An AI that follows the list cannot silently widen scope, invent a
dependency, or claim a
gate.

## 0. Pre-flight (before story 3.1)

- [ ] Read `AGENTS.md` Critical Rules 1-12, `docs/coding-standards.md` §2 boundaries
  and §3.1-§3.8
  do/don't examples, and `adr/0011-modularity-rules.md` R-M1..R-M13 — `AGENTS.md` wins
  on style, ADR
  wins on structure
- [ ] Read `docs/lessons.md` — the 2026-09-20 entry ("a count typed is a defect")
  applies to every
  number in `epic-3-dod.md`
- [ ] Run the baseline measurement of this epic on `main @ 73dec5f` and paste in
  `epic-3-dod.md`
  §3.0: `git rev-parse HEAD`, `git ls-tree -r --name-only main | wc -l`, `sh
  tools/check.sh`, `python3
  tools/spec-check.py`, `sh tools/layering-check.sh`, `cat tests/baseline.json |
  python3 -m json.tool
  | head -n 60`
- [ ] Create branch `feat/epic3-core-heart` from `main` — all 5 stories stack as PRs
  from this
  branch (`feat/3.1-ir`, `feat/3.2-text`, …) each ≤400 lines diff, squash-merge one at
  a time
  (AGENTS.md rule 9, `docs/kickoff.md` §9)

---

## 3.1 IR, page boxes and geometry — no engine crosses the seam

**Files this story may create or edit (allowlist):**
- `include/pdfcore/doc.h`, `include/pdfcore/page.h`, `include/pdfcore/geom.h` (new),
  `include/pdfcore/pdfcore.h` (aggregate), `include/pdfcore/backend.h` (read-only
  unless R-M3 bump
  needed)
- `src/core/doc/*`, `src/core/geom/*`, `src/core/CMakeLists.txt`
- `tests/unit/test_doc_ir.cc`, `tests/unit/test_geom.cc`,
  `tests/contract/backend_contract.cc`
  (extend, not fork)

**Files this story must not touch (denylist):**
- `src/backends/*` beyond reading the vtable type — no engine include, no new
  dependency (`grep -r
  "mupdf\|fitz" src/core` must stay 0)
- `src/render/*`, `src/os/*`, `src/app/*`, `src/features/*` — those are Epic 2/4/5

- [ ] `include/pdfcore/geom.h` — value types only:
  ```c
  typedef struct pc_rect { double x0,y0,x1,y1; } pc_rect;
  typedef struct pc_page_box { pc_rect mediabox, cropbox; int rotation; } pc_page_box;
  pc_status pc_page_get_box(const pc_page* page, pc_page_box* out);
  void pc_rect_to_device(const pc_page_box* box, pc_rect us, pc_rect* dev);
  ```
  Comment on every `pc_` function names its allocator: `// caller owns *out_runs,
  release with
  pc_text_run_free`
- [ ] `src/core/geom/geom.cc` — 4 rotations + CropBox!=MediaBox, bottom-left origin,
  no float
  truncation. Unit test `tests/unit/test_geom.cc` covers: `mediabox 612x792, crop
  100,100-500,700` at
  0,90,180,270 → device rects approval-tested with 1e-9 tolerance. Test runs on Linux
  with `null`
  backend, no window.
- [ ] `src/core/doc/doc.cc` — `pc_doc` IR: holds `pc_page_box` array, `page_count`,
  `sha256` of
  source bytes (for sidecar staleness). `pc_doc_open(path,password,&doc)` delegates to
  backend vtable,
  copies `fz_bound_page` into `pc_page_box`, closes engine doc handle copy (R-M4: no
  `fz_page*` held).
  `pc_doc_close` frees IR only, never engine memory.
- [ ] `pc_pixmap` copy-out: backend `page_render` writes engine pixmap → core
  allocates
  `pc_pixmap.data` via `malloc` and `memcpy` — backend's buffer freed before return
  (R-M6).
  `pc_pixmap_free` is core-owned and `free`s what core allocated. Test: open doc,
  render,
  `pc_doc_close`, pixmap still valid until `pc_pixmap_free`.
- [ ] Contract: run `tests/contract/backend_contract.cc` against `null` (synthetic
  612×792 white
  with CropBox) and `mupdf` (`tests/fixtures/simple.pdf`) — `page_count` and
  `pc_page_get_box` equal
  for same logical page 0.
- [ ] Guard: `grep -rEn '#include
  *[<\"]\(windows|windef|unknwn|d2d1|dwrite|fitz|mupdf)' src/core
  src/render` → 0 lines. Paste output in `epic-3-dod.md` §3.1.
- [ ] Gate: `sh tools/layering-check.sh --strict` — print ratio, exit 0. Paste. `sh
  tools/check.sh`
  — 14/14. `ctest --preset linux-core -R "geom|doc_ir"` — green.
- [ ] Diff ceiling: ≤350 lines excluding tests+golden. If larger, split `geom` into
  `feat/3.1a-geom`
  and `doc` into `feat/3.1b-doc`.

## 3.2 Text engine-agnostic — NFKC, pt-BR, caret over combining marks

**Allowlist:** `include/pdfcore/text.h` (new), `src/core/text/*`,
`tests/unit/test_text_*.cc`,
`tests/fixtures/text/*`, `tests/golden/text-*.txt`
**Denylist:** `src/backends/*` (except `null` synthesises a fake run for contract),
`third_party/*`

- [ ] `include/pdfcore/text.h`:
  ```c
  typedef struct pc_text_run { uint32_t size; float rect[4]; uint32_t font_face; const char* utf8; } pc_text_run;
  // pc_doc_page_text: on success *out_runs owned by caller, release with pc_text_run_free(*out_runs,*out_count)
  pc_status pc_doc_page_text(const pc_doc* doc, uint32_t page, pc_text_run** out_runs, uint32_t* out_count);
  void pc_text_run_free(pc_text_run* runs, uint32_t count);
  ```
- [ ] `src/core/text/normalize.cc` — UTF-8 → NFKC via direct tables (no ICU
  dependency — R-M9: a
  dependency that forces a flag needs an ADR). Casefold + whitespace collapse (same as
  ADR-0007 §4 for
  re-anchoring). Exposed as `std::string normalize_for_search(std::string_view)` and
  `std::string
  normalize_for_anchor`. Unit test feeds `e\u0301` (U+0065 U+0301) and expects `e`
  U+00E9 single rect.
- [ ] `src/core/text/break.cc` + `caret.cc` — grapheme-cluster break (UAX #29
  simplified + PT-BR
  exceptions), caret `left/right` is one visual step over combining sequence. Tests:
  `tests/unit/test_text_break.cc` with fixture `tests/fixtures/text/ptbr-golden.txt`:
  ```
  a\u0301  → caret positions [0,1] not [0,1,2]
  c + [U+0327]  (c + [U+25CC][U+0327]) → one step
  a[U+0303]o  → break inside word only at syllable boundary if pt-BR hyphen corpus says so
  ```
  Approval golden `tests/golden/text-caret-positions.txt` holds positions.
- [ ] `src/core/text/text.cc` — `pc_doc_page_text` over IR: backend `null` returns
  synthetic run
  `"Hello pt-BR: a\u0301 c + [U+0327] a[U+0303]o"` with rect `0,0,100,20`; backend
  `mupdf` path
  extracts via `fz_stext` in backend only, converts to `pc_text_run` via copy-out. No
  `fz_*` type in
  the header.
- [ ] Guard: `ldd build/linux-core/Debug/libpdfcore*.so 2>/dev/null | grep -i mupdf`
  → 0; `dumpbin
  /DEPENDENTS` of `pdfcore.lib` lists no `mupdf` (paste in dod). `grep -rn
  "fitz\|mupdf" src/core` →
  0.
- [ ] Gate: `ctest --preset linux-core -R text` green; `sh tools/check.sh` green;
  `python3
  tools/lang-check.py` ignores `tests/fixtures/text/*` content as allowed (ADR-0005:
  content under
  test may be non-ASCII).

## 3.3 Budget in the core, sidecar atomicity and hygiene

**Allowlist:** `include/pdfcore/budget.h` (new), `src/core/budget/*`,
`src/core/sidecar/writer.cc`
(new), `tests/unit/test_sidecar_*.cc`, `tests/golden/sidecar-*.txt`
**Denylist:** `src/render/*` (must only read budget, never write), `src/os/*`

- [ ] `include/pdfcore/budget.h`:
  ```c
  typedef struct pc_budget { uint32_t max_rss_mb; uint32_t max_tiles; uint32_t max_sidecar_bytes; } pc_budget;
  pc_budget pc_budget_default(void); // 250 MB, 64 tiles, 4 MB per kickoff/D6b
  pc_status pc_budget_check(const pc_budget* b, uint32_t tiles, size_t rss);
  ```
  `src/core/budget/budget.cc` + `tests/unit/test_budget.cc` — shrinking ceiling fails:
  test sets
  `max_tiles=1`, inserts 2 tiles, expects `PC_ERR_LIMIT`. Render will read this struct
  in Epic 4; here
  it is just a value type with tests on Linux, no GPU.
- [ ] `src/core/sidecar/writer.cc` — **R3.1 atomic:** `sidecar_write(path, json)` →
  `path.tmp.<rand8>` in same directory → `write` → `fsync(fileno)` → `rename` (POSIX
  atomic). Test
  `tests/unit/test_sidecar_writer.cc`: write 200 annotations, kill after `write` before
  `rename`
  (simulate by stubbing `rename` to `EINVAL`), verify original file unchanged. Also
  verify
  `sidecar-fmt.py fix` after write still canonical.
- [ ] **R3.2 lock:** `src/core/sidecar/lock.cc` — `pc_sidecar_try_lock(doc_path)`
  creates
  `*.tynypdf.lock` with PID + timestamp; `pc_sidecar_write` checks: if lock exists and
  `now - mtime <
  300s` → `PC_ERR_STATE` `"sidecar locked for <age>s (pid N)"`; if `>300s` treat as
  stale, unlink and
  proceed. Tests in `test_sidecar_lock.cc`: fresh lock refuses, stale lock proceeds,
  missing lock
  proceeds.
- [ ] **R4.1 unknown keys:** `src/core/sidecar/reader.cc` parses with
  `nlohmann/json`-free
  handwritten parser or `json` with `ordered_json` + key sort, stores `unknown_fields`
  map via
  `std::map<std::string, json>` preserved on write. Test round-trip: fixture with
  `"future_key": 123`
  survives write unchanged (approval golden: before/after byte-identical except
  whitespace
  canonicalisation).
- [ ] **R6 exclusions:** `writer.cc` rejects input containing any of
  `["open_page","zoom","scroll","window_size",
  "page_geometry","text_content","credential","passphrase"]`
  at top level — returns `PC_ERR_ARGUMENT` and never writes. Test
  `test_sidecar_schema.cc` iterates
  each key and expects rejection.
- [ ] Gate: `sh tools/sidecar-fmt.py check tests/fixtures/sidecar` + `self-test`
  green; `ctest -R
  sidecar` green; `--strict` layering green (render has no `pc_budget` write).

## 3.4 Version gate, id validation and staleness

**Allowlist:** `src/core/sidecar/reader.cc`, `src/core/sidecar/stale.cc` (new),
`src/core/sidecar/writer.cc`, `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`,
`include/pdfcore/sidecar.h`, `tests/unit/test_sidecar_reader.cc`,
`tests/unit/test_sidecar_ids.cc`,
`tests/unit/test_sidecar_staleness.cc`, `tests/unit/test_sidecar_schema.cc`,
`tests/unit/test_sidecar_unknown_keys.cc`, `tests/golden/sidecar-stale-report.txt`
**Denylist:** `src/app/*`, `src/render/*`

- [ ] **R2.2 version gate:** `reader.cc` reads `format_version` (int). If `>
  SUPPORTED=1` → return
  `pc_status{PC_ERR_VERSION, detail="format_version X > supported 1"}` and out param is
  valid
  read-only view (caller may render but not mutate). Writer never lowers: `write` with
  `format_version=1` always. Test: fixture `future_version.tynypdf.json` with `2`, open
  →
  `PC_ERR_VERSION`, mutate attempt → `PC_ERR_STATE`.
- [ ] **R2.3 id validation:** validator `bool is_valid_id(string_view)` =
  `^[a-z2-7]{10}$`. Tests
  cover: `"abc"` (short), `"ABCDEFGH12"` (upper), `"abcdefgh81"` (digit 8),
  `"abcdefgh90"` (9/0),
  `"abcd-efgh12"` (dash), `"abcdefgh12="` (pad). Each annotation `id` and reply
  `id`/`in_reply_to`
  validated. Tool `sidecar-fmt.py` already rejects unknown reply target — mirror that
  logic in core
  and keep them in sync (add cross-check test: same fixture rejected by both).
- [ ] **R5.1 page count stale:** `stale.cc` compares `sidecar.document.pages` vs
  `pc_doc_page_count(doc)`. Mismatch → `pc_status{PC_ERR_STATE, detail="stale: sidecar
  pages N vs
  document M"}` and `pc_sidecar_is_stale()` returns true. Writes disabled until caller
  passes
  `force=true` (explicit, not silent). Test: doc 5 pages, sidecar 3 → stale true, write
  without force
  → `PC_ERR_STATE`.
- [ ] **R5.2 fingerprint vs mtime:** `document.sha256` hex vs computed
  `sha256(file_bytes)` is
  strong signal; if equal but sidecar `modified` < doc `mtime`, weak signal. Detail
  string names which
  fired: `"stale: fingerprint mismatch (doc sha256 abc… vs sidecar def…)"` vs `"stale:
  mtime newer
  (doc 2026-09-20T10:00:00Z vs sidecar 2026-09-19…)"` — exact format golden-guarded in
  `tests/golden/sidecar-stale-report.txt`. Both paths tested with fake sha and `utime`
  stub.
- [ ] **Spec retirement:** after 3.3+3.4, `python3 tools/spec-check.py` pending drops
  17 → 7 (only
  `R4.2` if deferred, `R14.1-14.3`, `R15.1-15.4` remain). Paste output before/after in
  `epic-3-dod.md`
  §3.4. Do not hand-edit a SPEC to make the number drop — only a real `Verification:`
  artefact does.
- [ ] Gate: `ctest -R "sidecar_reader|sidecar_ids|sidecar_stale"` green; `sh
  tools/check.sh` green.

## 3.5 pdfcore API freeze, golden header and contract final — ratio saneado

**Allowlist:** `include/pdfcore/*.h`, `tests/golden/status_enum.txt`,
`tests/unit/test_status_abi.cc`, `src/backends/mupdf/exception_bridge.h`,
`src/backends/mupdf/mupdf_backend.cc`, `tests/contract/backend_contract.cc`,
`tests/baseline.json`,
`tools/bench-measure.sh`
**Denylist:** no new `.patch` without `Upstream-status` update, no new `conan` dep
without
`docs/dependency-policy.md` row

- [ ] Freeze `include/pdfcore/*.h`: `status.h` enum 0-15 append-only, `doc.h`,
  `page.h`, `geom.h`,
  `text.h`, `budget.h`, `sidecar.h`, `backend.h` vtable 1.0 (`abi_major=1, minor=0,
  struct_size`),
  `pdfcore.h` aggregate. Every public function documents its `pc_error` codes in header
  comment —
  `python3 tools/spec-check.py` section for error documentation must pass.
- [ ] Golden: `tests/golden/status_enum.txt` is the committed enum dump;
  `tests/unit/test_status_abi.cc` reads `status.h` and fails on any renumber. Do the
  throwaway proof:
  on branch `tmp/renumber-proof`, change one enum value, `ctest -R status_abi` red —
  paste log snippet
  in `epic-3-dod.md` §3.5, then revert.
- [ ] Backend harden: `src/backends/mupdf/exception_bridge.h` is the **only** file
  that contains
  `fz_try`/`fz_catch`/`fz_always` — enforce with `grep -rn "fz_try" src
  --include="*.cc"
  --include="*.h" | grep -v exception_bridge` → 0. Per-context isolated lock set:
  replace
  `fz_new_context(nullptr,nullptr,FZ_STORE_UNLIMITED)` with per-doc `fz_locks` array
  allocated per
  `MupdfDoc::ctx` (see `mupdf-rs` PR #263 pattern), no process-wide mutex array. Paste
  threaded
  benchmark: `10 threads × 160p text extraction` — threaded ≤4× single, not 13×.
- [ ] Contract final: `ctest --preset linux-core -R contract` runs
  `backend_contract.cc` twice (null
  + mupdf) in one job; both share the same `pc_doc` IR copy-out and `pc_text_run` value
  types. Add
  `PC_ERR_CAPABILITY` test: `pc_doc_has_capability(doc, PC_CAP_TABLES)` false →
  `pc_doc_find_tables`
  returns `PC_ERR_CAPABILITY` not empty list (R-M5).
- [ ] Ratio saned: `sh tools/layering-check.sh --strict` reports `backend_line_ratio
  ≤ 0.07`
  (expected ~180 backend non-vtable lines / ~2600 total src+include lines). If higher,
  this story is
  not done — move a helper from `src/backends/` into `src/core/`.
- [ ] Baseline re-measured: `sh tools/build-mupdf-windows.sh 2>&1 | tail -n 20` (if
  cross), then
  `python3 tests/bench/harness/run_benchmark.py --target tynypdf --backend mupdf --runs
  5 --output
  /tmp/mupdf-baseline.json` **in Release, no ASan** on the reference machine; `python3
  -m json.tool
  /tmp/mupdf-baseline.json > tests/baseline.json` replacing the `null` entry with
  `tynypdf (mupdf
  backend)` and keeping both Sumatra channels. Second run within 10% per metric — paste
  both JSON
  blocks. `tests/bench/corpus` sha256s pasted.
- [ ] Final gates: `sh tools/check.sh` 14/14, `sh tools/gates-selftest.sh` 13/13,
  `ctest --preset
  linux-core --output-on-failure` 0 failed, `python3 tools/docs-check.py` 0 problems
  over the 5 new
  epic docs, `python3 tools/lang-check.py` 0 problems, `python3 tools/naming-sync.py
  check` 0
  problems, `sh tools/canonical-check.sh` 0 problems.
- [ ] Evidence pack in `epic-3-dod.md` §3.5: `wc -l` per new src/core file, `grep -c`
  for
  `windows.h` 0 proof, threaded benchmark table, golden red proof, ratio line, two
  baseline JSONs side
  by side.

## 3.x Final epic gates

- [ ] `sh tools/check.sh` (exit 0) — all 14 sections, on a clean clone and on the PR
  head, logs
  pasted
- [ ] `sh tools/gates-selftest.sh` (13/13) — pasted
- [ ] `ctest --preset linux-core --output-on-failure` — 0 failed, contract 2
  backends, status_abi,
  text, budget, sidecar suites all listed
- [ ] `python3 tools/docs-check.py` — 0 problems (5 new epic docs plus every relative
  link
  `../../adr/...` resolving)
- [ ] `python3 tools/spec-check.py` — 0 orphans, pending 7 (R4.2 + R14/R15 only) —
  paste
  before/after counts
- [ ] `python3 tools/naming-sync.py check` — 0 problems
- [ ] `python3 tools/lang-check.py` — 0 problems
- [ ] `sh tools/layering-check.sh --strict` — `backend_line_ratio ≤ 0.07` printed as
  a number
- [ ] `sh tools/canonical-check.sh` — 0 problems, LF only
- [ ] `grep -rEn '#include *[<\"]\(windows|windef|d2d1|fitz|mupdf)' src/core
  src/render` → 0 —
  pasted
- [ ] `grep -rn "fz_try" src --include="*.cc" --include="*.h" | grep -v
  exception_bridge` → 0 —
  pasted
- [ ] `tests/baseline.json` contains `tynypdf (mupdf backend)` Release measurement
  and both Sumatra
  channels

**Epic 3 completion checklist (for `epic-3-dod.md` §3.6):**

- [ ] 3.1: IR + geometry copy-out, 4 rotations + CropBox, contract 2 backends, grep
  0, layering
  green
- [ ] 3.2: NFKC + pt-BR golden + caret over combining marks headless, `ldd` no mupdf
  in pdfcore
- [ ] 3.3: budget struct + R3.1 atomic + R3.2 lock + R4.1 preserve + R6 exclusions,
  sidecar-fmt
  green
- [ ] 3.4: R2.2 version gate + R2.3 id validation + R5.1/R5.2 staleness with golden,
  pending 17→7
- [ ] 3.5: API freeze + golden guard + backend harden (isolated locks) + contract 2
  backends + ratio
  ≤0.07 + baseline re-measured
- [ ] Final gates green on `main`, evidence pasted in `epic-3-dod.md`

---

*Next step: execute 3.1 first — IR gates everything else because a text or budget PR
without IR has
nowhere to store its result. Definitions in `epic-3-stories.md`, level-by-level
commands in
`epic-3-testing.md`, and the completion evidence in `epic-3-dod.md`.*
