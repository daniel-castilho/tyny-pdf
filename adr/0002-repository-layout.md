# ADR-0002: Repository layout

- Status: Accepted (ratified by the project owner, 2026-09-16)
- Date: 2026-09-16
- Deciders: project owner
- Related: ADR-0001, ADR-0003, ADR-0005

## Context

v1 is six independently shippable deltas (undo command log, font and diacritic policy, form
validation and flatten, editor accessibility, annotation sidecar, redaction proof). Each delta
touches several layers at once. A layout organised by technical layer would scatter every delta
across four trees and make the per-capacity `SPEC.md` of the living-specification decision have
no obvious home.

## Decision

The repository is organised in **vertical feature slices** under `src/core/`, with **horizontal
seams** only where a second implementation is expected or required.

```
CMakeLists.txt  CMakePresets.json
conanfile.py    conan.lock            # ADR-0004
third_party/mupdf/                    # pinned submodule plus patches/series
include/pdfcore/                      # the only stable public surface (C headers)
  pdfcore.h  backend.h  sealer.h  print.h  sidecar.h
  gen/req_traces.h                     # generated from SPEC.md, committed, guarded
src/core/          # engine-agnostic, no OS widgets, no networking
  doc/    annot/    form/    text/    search/    sig/    sidecar/    budget/
  <each directory holds its own SPEC.md and unit tests>
src/backends/      mupdf/   null/      # implements include/pdfcore/backend.h vtable
src/sealer/        pkcs7_local/  stub/  # implements sealer.h
src/print/         win32_gdi/    preview/
src/os/win32/      window/  dpi/  uia/  clipboard/  policy/
src/render/        swapchain/  cachemap/  tiles/
src/app/           viewer executable: thin composition root only
src/cli/           tynypdf-cli: headless, exit codes 0,1,2,3, JSON plus JUnit reporters
tools/             check.sh, lang-check.py, sidecar-fmt.py, naming-sync.py, spec-check.py,
                   canonical-check.sh, docs-check.py; layering-check.sh (planned, PR #4)
tests/
  unit/  contract/            # contract suite runs against every backend
  conformance/<iso-clause>/  # clause-keyed corpus, pinned as a submodule (ADR-0010)
  approvals/                 # golden files for structured dumps
adr/               one file per decision, numbered, never edited after acceptance except for status
docs/
  kickoff.md  naming.md  dev-environment.md  references.md  lessons.md
  conformance/  security/  a11y/  data-model-decisions.md  flags.md  SWAP-CHECKLIST.md
AGENTS.md  README.md  CHANGELOG.md
```

### Rules that this layout carries

1. One `SPEC.md` per capability directory. Capability means one of: the five seams plus the
   six v1 deltas plus `text`, `search`, `budget`. Not per class, not per file.
2. `src/core/**` may include only `include/pdfcore/**` and its own headers. It may not include
   engine headers, `windows.h`, or any networking header.
3. `src/app/**` may not contain logic that a CLI consumer needs; anything shared moves to
   `src/core` or a seam. The composition root stays under one file per executable.
4. Backends are named by what they implement, never by what they are not
   (`mupdf`, `null`), and no backend may `#include` another backend.
5. Tests live next to the code they characterise for unit tests, under `tests/` for contract,
   conformance and approvals; a capability's contract tests are not optional.

## Enforcement (all commands, no conventions)

| Check | Command | Where |
| --- | --- | --- |
| No engine symbols leak into the app | `dumpbin /SYMBOLS` / `nm -u` on `pdfviewer.exe` must not list `fz_` or `FPDF_` | CI, all builds |
| `pdfcore` links no UI, no network | `dumpbin /DEPENDENTS` of `pdfcore.lib` and backends must not list `user32.lib`, `d3d11.lib`, `winhttp.lib`, `ws2_32.lib` | CI |
| Core includes stay narrow | `grep -rn -E "include <(fz_\|fpdf\|windows)" src/core` returns nothing | CI |
| Backend size budget | `cloc src/backends` divided by total lines stays at or below 0.15 | CI, reported as `backend_line_ratio` |
| Contract suite covers every backend | `ctest -R contract` must run once per backend pair (job matrix) | CI |
| Generated API and traces are current | regeneration must leave `git status --porcelain` empty for `include/pdfcore/gen` | CI |

## Consequences

- Positive: a delta is one tree of changes, which keeps AI-assisted diffs reviewable and keeps
  `git blame` readable per capability.
- Positive: the swap checklist can be verified mechanically because everything outside
  `src/backends/` is engine-free by construction, not by discipline.
- Negative: feature-slice layouts duplicate small helpers between slices; the budget for that
  duplication is a shared `src/core/doc` or an explicit ADR, and the rule is that duplication
  is cheaper than a speculative shared abstraction.
- Negative: reviewers used to layer folders must be told that `src/core/form` containing both
  model and rules is intentional.

## Alternatives rejected

- Layer folders (`domain/ application/ infrastructure/ presentation/`). Rejected: scatters
  every delta, and folder names give false comfort about boundaries while link-level checks are
  what actually hold.
- Hybrid `core` versus `app` only. Rejected: leaves the seam placement implicit.
