# ADR-0011: Modularity rules (reversibility as a first-class property)

- Status: Accepted (ratified by the project owner, 2026-09-16)
- Date: 2026-09-16
- Related: ADR-0001 (canon), ADR-0002 (layout), ADR-0004 (dependencies), ADR-0010 (toolchains)

## Context

The owner's stated foundation: swapping the engine or a library must hurt as little as possible, and
modularity is a pillar rather than a virtue. Two measured facts shape how that is honoured here:
MuPDF's `fz_context` is per-thread with a
shared store, so a swap is not a header edit but a concurrency-model edit, and the cost of getting
that wrong is measured rather than intuited: in the most-used Rust binding (`messense/mupdf-rs`,
issue #260, 2026-08) every context took its locks from one process-wide mutex array, so on a 6-core
machine ten threads each reading the text of a 160-page PDF took 13.3x the single-thread time, while
ten separate processes took 3.3x. The bottleneck was the binding's lock set, not the hardware; the
fix (PR #263) gives each context its own lock set and found a use-after-free and a wrong mutex
teardown on the way. A "swappable engine" that cannot be
swapped without re-deriving the threading model is a comment, not a property. Modularity therefore
gets rules that a script can check, not an aspiration in a manifesto.

## Decision

Rules R-M1..R-M13. Each is written so that a violation is visible in a diff or in a check, because
an unenforceable rule is decoration.

- **R-M1 Seams are typed and small.** One header per seam (`pdfcore.h`, `backend.h`), the public
  surface of `src/core` is the only API the viewer and the CLI may use.
- **R-M2 The engine is a backend, not a dependency of the app.** `src/backends/<name>/` implements
  the versioned C vtable declared in `include/pdfcore/backend.h`; nothing under `src/core` includes
  an engine header.
  CI must build with the `null` backend to prove the boundary holds (ADR-0002; wired with the first
  backend in PR #5).
- **R-M3 The vtable is a stable ABI, versioned explicitly.** A `pc_backend_caps` struct is the first
  argument of every capability query; the vtable carries `abi_major`/`abi_minor` and a struct size
  field, and an entry may only be appended. A backend built against a newer major is refused with a
  message that names both versions.
- **R-M4 No engine object crosses a seam.** Everything returned to the app is our own value type
  (page geometry, text runs, annotation records) copied out, so a document can be closed while the
  caller still holds the data, and so `src/render` never needs an engine header.
- **R-M5 Capabilities are declared, not assumed.** `pc_doc_find_tables` exists only if the backend
  reports `PC_CAP_TABLES`; the UI shows "not supported by this backend" rather than a stub that
  returns an empty list, because a silent empty result is indistinguishable from "no tables".
- **R-M6 One allocation owner.** The backend that allocates frees; our code never calls `free()` on
  engine memory. The MuPDF allocator and store live in the context created by the backend.
- **R-M7 One concurrency model per process, documented at the seam.** Engine contexts are not shared
  across threads; parallel work is per-document, and the contract says so, so a backend swap cannot
  quietly serialise the renderer behind a global lock.
- **R-M8 The IR owns semantics.** Undo, redaction, flatten and the sidecar operate on
  `src/core/doc` (the intermediate representation), never on engine handles. This is what makes
  D-6 (undo via a command log over the IR) independent of the engine, and what makes `pdfium` or
  `hayro` a plausible second backend instead of a rewrite.
- **R-M9 Cascades are explicit.** A decision that forces a dependency, a build flag or a toolchain
  records its cost in a "Cost of swapping" section, and the record says what would be lost by
  keeping it. (Negative example, measured in the sibling project: `x86_64-pc-windows-gnu` is forced
  by one scripting dependency, which is a toolchain decision made by a library.)
- **R-M10 Layering is machine-checked.** `tools/layering-check.sh` fails if
  `src/render/**` or `src/os/**` contains parsing, geometry reconciliation or annotation mutation,
  and if `src/core/**` includes a Windows header.
- **R-M11 The swap budget is measured, not promised.** `backend_line_ratio` = lines of code inside
  `src/backends/**` that are not in a vtable implementation, divided by total engine-facing lines;
  `tools/layering-check.sh` reports it and fails above 0.15. That number is the operational meaning
  of "swap with little pain"; the correction of 2026-09-17 fixes both terms exactly.
- **R-M12 Reversibility decides the ceremony.** Cheap to undo: no document, just a comment and a
  test. Expensive: an ADR with a swap cost. Irreversible user-visible formats (the sidecar suffix,
  the registry path, the PDF mutation semantics of redaction) get the "frozen at first public
  release" marker, which is why ADR-0007 froze them deliberately late.
- **R-M13 Behaviour lives in a specification a tool can check.** Each capability ships a `SPEC.md`
  whose requirements are one-line EARS statements carrying an `### R<n>.<m>` id and a
  `Verification:` target. `tools/spec-check.py` fails on a malformed statement, on an id that no
  test or code path cites, and on a `done` claim whose artefact is missing; the same rule closes a
  PR's `Requirement:` trailer (ADR-0009 rule 5). It sits in this ADR rather than in ADR-0002
  because a specification no command reads is exactly the failure mode modularity prevents.

## Consequences

- Positive: "maximum modularity" becomes two numbers a reviewer can see in every PR now that
  `tools/layering-check.sh` wires them (the ratio, and the layering check), and the engine swap
  stops being a rumour about a future rewrite.
- Positive: R-M4 and R-M8 are what make the WSL2 workflow viable at all, because everything behind
  them is testable without Windows (ADR-0010).
- Negative: copy-out IR costs memory and a translation layer; a 4000x3000 page with full text run
  is the worst case, bounded by the RSS gate in D6b. Accepted: the alternative is a design in
  which the engine choice cannot be revisited.
- Negative: an appended-only vtable accumulates optional entries; the escape hatch is a major
  version bump plus a migration note, and it is deliberately annoying.

## Corrections

- 2026-09-16: R-M1 and R-M2 named the vtable header `pc_backend.h`. ADR-0002 puts it at
  `include/pdfcore/backend.h`, and `pc_` is the symbol prefix rather than part of a file name; the
  rule text now matches the layout it constrains. Substance unchanged, no re-decision needed.
- 2026-09-17: R-M11 named `backend_line_ratio` but left its two terms to the reader, and ADR-0002's
  enforcement table described a third measurement (`cloc src/backends` over total lines). The
  formula is now fixed and is the one `tools/layering-check.sh` implements: the **numerator** is
  every line under `src/backends/**` that is not inside a function assigned into that backend's
  `pc_backend_api` initializer (the vtable implementation, i.e. the code any engine has to carry),
  and the **denominator** is every line under `src/**` and `include/**`. Lines are raw `wc -l`
  counts over `.c`, `.cc`, `.cpp`, `.h` and `.hpp`; the gate reports the ratio and fails above 0.15.
  The rule text above now carries the same terms.
