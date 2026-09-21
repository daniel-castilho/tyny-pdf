# Epic 3 – Testing Strategy [grounded]

Test levels are the taxonomy of `docs/testing-playbook.md` §2 (Repo gates, Unit,
Contract, Approval,
Conformance, Performance, Privacy, Windows-specific). Everything here names the
command that
executes it and the artefact the output goes into. A test that cannot be run yet is
labelled with
the PR that introduces it.

> **Improvement for AI over Epics 1-2:** every level below has a one-line `AI-Guard`
— the exact
mistake an AI is likely to make at this level and the exact check that catches it. A
test plan
without a guard is a suggestion; this one is a gate.

## Gates (run on every PR, no exceptions)

- **Repo gates:** `sh tools/check.sh` — 14 sections; `sh tools/gates-selftest.sh` —
  13 suites.
  **AI-Guard:** AI rewraps a paragraph and changes 11→14 counts → `docs-check` catches
  wrapped
  continuation via word-sequence fence check.
- **Style:** `sh tools/format-check.sh` + `sh tools/format-check.sh --self-test` —
  whole tree minus
  `third_party/`, `.clang-format` 18.1.3. **AI-Guard:** AI adds an engine include to
  `src/core` →
  `layering-check --strict` fails, not a reviewer.
- **Language:** `python3 tools/lang-check.py` — 0 problems, `tests/fixtures/text/*`
  is allowlisted
  for non-ASCII content (ADR-0005). **AI-Guard:** AI writes a Portuguese comment in a
  header →
  `lang-check` red.
- **Naming:** `python3 tools/naming-sync.py check` — 34 keys, 2 generated files, 1
  retired token
  guarded.
- **Spec:** `python3 tools/spec-check.py` — 0 orphans; `Verification:` artefact must
  exist for
  `done`. **AI-Guard:** AI marks R3.1 done without `test_sidecar_writer.cc` →
  `spec-check` pending
  still 17.

## 3.1 IR, page boxes and geometry

- **Goal:** prove no engine object crosses the seam and geometry is correct for every
  rotation and
  CropBox.
- **Unit (gtest, headless, null backend):**
  - `tests/unit/test_geom.cc` — 4 rotations × 3 CropBox cases (Media=Crop, Crop inset,
    Crop offset) →
    `pc_rect_to_device` vs hand-computed expected (tolerance 1e-9).
    `tests/unit/test_doc_ir.cc` —
    `pc_doc_page_count 5`, `pc_page_get_box` mediabox 612×792 matches `simple.pdf`,
    `pc_page_render`
    100×100 pixmap with known grey pattern via null backend, `pc_doc_close` before
    `pc_pixmap_free`
    still valid.
- **Contract (same suite, two backends):**
  - `tests/contract/backend_contract.cc` — `TEST_P(Backend, pageBoxAndRender)`
    parametrised over
    `null`/`mupdf` → same `pc_page_box` for page 0 of `simple.pdf`.
- **Approval:** `tests/golden/geom-device-*.txt` for the 12 device rects;
  `tools/sidecar-fmt.py` not
  needed here.
- **Negative:** `grep -rEn '#include.*windows|mupdf|fitz' src/core` → 0; move one
  include into
  `src/core/geom` on throwaway branch → `layering-check --strict` red (paste in dod).
- **Command:** `ctest --preset linux-core -R "geom|doc_ir|contract"
  --output-on-failure` + `sh
  tools/layering-check.sh --strict` — **AI-Guard:** AI stores `fz_page*` in `pc_page` →
  `grep fz_` in
  `include/pdfcore` fails gate, test passes but layering red.

## 3.2 Text engine-agnostic — NFKC, pt-BR, caret

- **Goal:** prove normalisation and caret are correct without an engine and survive a
  backend swap.
- **Unit:**
  - `tests/unit/test_text_normalize.cc` — `normalize_for_search("e\u0301") ==
    "\u00e9"`,
    `normalize_for_anchor("  Hello   \n WORLD ") == "hello world"`
    (NFKC+casefold+collapse).
  - `tests/unit/test_text_break.cc` + `test_caret.cc` — pt-BR golden
    `tests/fixtures/text/ptbr-golden.txt` lines: `a\u0301` caret [0,1] not [0,1,2];
    `c\u0327` one step;
    `a\u0303o` break not inside grapheme; `cafe` vs `cafe\u0301` same break. Approval
    golden
    `tests/golden/text-caret-positions.txt`.
  - `tests/unit/test_text_runs.cc` — `pc_doc_page_text` with null backend synthetic run
    has same
    `utf8` as mupdf backend after normalisation for `text.pdf`.
- **Contract:** `backend_contract.cc` extended — `pc_doc_page_text` returns same
  `count` for
  null/mupdf on page 0.
- **Windows-specific:** none here — caret arithmetic headless. IME plumbing is Epic
  5/D-5, not this
  story. **AI-Guard:** AI puts `fitz` extraction in `src/core/text` → `grep fitz
  src/core` red.
- **Command:** `ctest --preset linux-core -R text --output-on-failure`; `python3
  tools/lang-check.py` — Portuguese content only under `tests/fixtures/text/`.

## 3.3 Budget in the core, sidecar atomicity and hygiene

- **Goal:** prove budget is a core decision, writes are crash-safe, locks are
  time-based, unknown
  keys survive, and forbidden keys never land on disk.
- **Unit:**
  - `tests/unit/test_budget.cc` — `pc_budget_default` → `250,64,4<<20`; set
    `max_tiles=1`, insert 2 →
    `PC_ERR_LIMIT`; `pc_budget_check` pure function tested with no GPU.
  - `tests/unit/test_sidecar_writer.cc` — R3.1: write → verify no `.tmp` left, file
    canonical
    (`sidecar-fmt check` on written file); stub `rename` failure → original intact;
    concurrent writer
    conflict → second gets `PC_ERR_STATE`.
  - `tests/unit/test_sidecar_lock.cc` — fresh lock (<300s) → writer `PC_ERR_STATE` with
    age in detail;
    stale lock (>300s) → unlink + success; missing lock → success. Uses fake clock
    (`pc_clock_override`
    test seam) not `sleep`.
  - `tests/unit/test_sidecar_unknown_keys.cc` — write fixture with `"future_key": 123`
    → read → write
    → `diff` shows key preserved, byte-canonical otherwise.
  - `tests/unit/test_sidecar_schema.cc` — each forbidden key in
    `[open_page,zoom,scroll,window_size,text_content,credential,passphrase]` → write
    returns
    `PC_ERR_ARGUMENT` and file not created.
- **Property:** sidecar round-trip — `sidecar-fmt fix` is idempotent: `fix(fix(x)) ==
  fix(x)` for
  100 random fixtures (fuzz harness in Epic 5, here a 20-case loop).
- **Command:** `ctest --preset linux-core -R
  "budget|sidecar_writer|sidecar_lock|sidecar_unknown|sidecar_schema"` + `python3
  tools/sidecar-fmt.py
  check tests/fixtures/sidecar && python3 tools/sidecar-fmt.py self-test`.

## 3.4 Version gate, id validation and staleness

- **Goal:** prove the reader is forward-compatible, ids are strict, and staleness
  reports which
  signal fired.
- **Unit:**
  - `tests/unit/test_sidecar_reader.cc` — R2.2: fixture `future_version.tynypdf.json`
    with
    `format_version:2` → `PC_ERR_VERSION` and `detail` contains `"2 > supported 1"`;
    mutate attempt
    after → `PC_ERR_STATE`.
  - `tests/unit/test_sidecar_ids.cc` — table-driven `is_valid_id` over 12 cases (too
    short, uppercase,
    digit 8/9/0, dash, pad, empty, 10 ok, reply in_reply_to unknown → `sidecar-fmt`
    cross-check fails).
  - `tests/unit/test_sidecar_staleness.cc` — R5.1: doc 5 vs sidecar 3 → `is_stale true`
    + detail
    `"sidecar pages 3 vs document 5"`; R5.2 fingerprint mismatch vs mtime newer — two
    tests, each
    asserts detail prefix. Golden `tests/golden/sidecar-stale-report.txt` holds exact
    strings including
    RFC3339 seconds precision.
- **Approval:** golden stale report — any wording change fails `diff` against
  `tests/golden/sidecar-stale-report.txt`.
- **Cross-check:** same `future_version` fixture → `python3 tools/sidecar-fmt.py
  check` and core
  reader agree on rejection reason (both name the version).
- **Command:** `ctest --preset linux-core -R
  "sidecar_reader|sidecar_ids|sidecar_stale"` + `python3
  tools/spec-check.py` — pending 17→7 pasted.

## 3.5 pdfcore API freeze, golden header and contract final — ratio saneado

- **Goal:** prove the C ABI is append-only, the backend is hardened, the contract
  holds for both
  engines, layering budget is healthy on a real tree, and baseline is trustworthy.
- **Unit:**
  - `tests/unit/test_status_abi.cc` — `include/pdfcore/status.h` vs
    `tests/golden/status_enum.txt` —
    any integer move fails. Throwaway renumber proof pasted in dod.
  - capability (R-M5) in `tests/contract/backend_contract.cc::test_capability` —
    `PC_CAP_TABLES` false →
    `pc_doc_find_tables` returns `PC_ERR_CAPABILITY` with `"capability not supported"`
    in `detail`,
    not an empty list. Does not parse the PDF — no tables capability exists yet.
- **Contract:** `tests/contract/backend_contract.cc` —
  `INSTANTIATE_TEST_SUITE_P(BothBackends, ...)`
  over `null`/`mupdf`; includes `page_render` determinism: same page index twice → same
  `sha256(pixmap.data)` (null pattern deterministic, mupdf with same `dpi=72`
  deterministic).
- **Threading (R-M7):** `tests/contract/threaded_contract.cc` — 10 threads × 160-page
  `long160.pdf` (fixture from `tools/gen-long160.py`) with **per-document render**, one
  `pc_doc` per
  thread — `time_threaded / time_single < 4.0` (not 13.3). The 4.0× threshold is clamped
  to
  `min(4.0, 0.75·cores)` (documented in the test; 4× is physically unattainable on a
  4-vCPU CI
  runner). Pasted as table; if red, epic not done — lock set not isolated. Red proof:
  sharing ONE
  lock array across all contexts → 0.67× (RED).
- **Layering:** `sh tools/layering-check.sh --strict` — prints `backend_line_ratio`
  as number ≤0.07;
  `grep -rn fz_try src | grep -v exception_bridge` → 0; `grep -rn "windows\|d2d1"
  src/core src/render`
  → 0; `dumpbin /DEPENDENTS` / `ldd` of `pdfcore` lists no engine.
- **Performance (baseline):** `python3 tests/bench/harness/run_benchmark.py --target
  tynypdf
  --backend mupdf --runs 10` in **Release, no ASan** on reference machine — two
  consecutive invocations
  within 10% per metric (open, first_paint, scroll; `search_time_ms` is a placeholder
  for the CLI and
  `peak_rss_mb` carries the documented sampling race). Paste both JSONs;
  corpus `sha256sum
  tests/bench/corpus/*.pdf` pasted.
- **Gates final:** `sh tools/check.sh` 14/14, `sh tools/gates-selftest.sh` 13/13,
  `python3
  tools/docs-check.py` 0, `python3 tools/lang-check.py` 0, `python3
  tools/naming-sync.py check` 0, `sh
  tools/canonical-check.sh` 0, `python3 tools/spec-check.py` 0 orphans pending 4.
- **AI-Guard:** AI adds a new `conan` dep for JSON → `docs/dependency-policy.md` row
  missing →
  `docs-check` or `spec-check` dependency gate fails. AI adds a `windows.h` to core for
  a helper →
  `layering-check` red before review.

## Regression gates (per story and at the end)

- [ ] `sh tools/check.sh` after every story, and on a clean clone before any merge
  claim
- [ ] `sh tools/gates-selftest.sh` — 13/13
- [ ] `python3 tools/docs-check.py` — 0 problems, with the five `tasks/epic-03/*.md`
  inside its
  scope
- [ ] `python3 tools/spec-check.py` — 0 orphans, pending 4 only (R15.1–R15.4)
  after story 3.5 — a
  run that shows 0 pending before artefacts exist is a broken checker
- [ ] `python3 tools/naming-sync.py check` — 0 problems; `generated/naming.cmake`
  unchanged unless
  `docs/naming.md` changed
- [ ] `python3 tools/lang-check.py` — 0 problems
- [ ] `sh tools/layering-check.sh --strict` — ratio as number, ≤0.07 at epic end
- [ ] `ctest --preset linux-core --output-on-failure` — 0 failed, contract both
  backends,
  status_abi, text, budget, sidecar suites green

**Epic 3 testing checklist:**

- [ ] 3.1 geometry 4 rotations×3 CropBox + doc IR + layering grep 0 + ratio printed
- [ ] 3.2 NFKC + pt-BR golden + caret over combining marks headless + ldd no mupdf
- [ ] 3.3 budget + atomic fsync + lock fresh/stale + unknown keys + R6 rejections
- [ ] 3.4 version gate + id table 12 cases + staleness fingerprint vs mtime + golden
  stale report
- [x] 3.5 status golden red proof + capability R-M5 + threaded 10×160p <4× + ratio
  0.0683 ≤0.07 (merge commit re-measures 0.0686) + baseline
  mupdf Release 2 runs within 10%
- [x] Final gates green on `main`, not on a working copy, with pasted evidence per
  level (clone `/tmp/tyny-epic3-final` @ `007663c`, evidence in `epic-3-dod.md` §3.6)

---

*Next step: run the actions above as each story executes and paste the outputs into
`epic-3-dod.md`;
tasks in `epic-3-technical-tasks.md`, criteria in `epic-3-stories.md`.*
