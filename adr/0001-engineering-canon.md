# ADR-0001: Engineering canon

- Status: Accepted (ratified by the project owner, 2026-09-16)
- Date: 2026-09-16
- Deciders: project owner
- Related: ADR-0002, ADR-0003, ADR-0005; `docs/lessons.md`; `AGENTS.md`

## Context

The project is a native desktop application with a performance-critical raster loop, an
adversarial input format, byte-faithful file writing, and a pluggable document engine. It is
built by one person with AI assistance, under a copyleft engine license, with a stated
non-negotiable of reversibility for engine and library choices.

The available design literature pulls in different directions for this shape of software.
Clean Architecture and hexagonal architecture describe boundaries well but are silent about
hot paths and hardware. Clean Code's style rules are easy to misapply to a renderer. SOLID was
written for object-oriented business code, not for a C ABI. The project needs one short list of
what is normative here, and needs the rules to be checkable rather than aspirational.

## Decision

1. **Deep modules are the design objective.** Small interfaces, powerful implementations;
   comments record invariants and rationale, not code narration.
2. **Hexagonal boundaries only at the five seams**: `backend` (document engine), `sealer`
   (signature emission), `print`, `sidecar` (external annotation store), `os` (window, DPI,
   clipboard, UIA, policy). Every seam has one production implementation and one test or
   `null` implementation, and both are built in CI. Nothing else is a "port".
3. **The dependency rule is enforced by linking, not by folder names**: `pdfcore` and its
   backends must not link or include UI or networking facilities; the viewer executable must
   not reference engine symbols.
4. **Value- and data-oriented internals in the raster and search loops**: contiguous storage,
   no per-frame allocation, no virtual dispatch in the paint loop, explicit memory budget.
5. **Characterization and property tests are the primary tools where an oracle is external**:
   golden images for rendering, approvals for structured dumps, algebraic properties for
   round-trips and undo. Unit assertions are used for rules, not for pixels.
6. **Local-first is the product's stated model** for data ownership and offline behaviour; the
   local copy is primary and any network use is an explicit, disclosed user action.
7. **SOLID is a review checklist, not scaffolding.** Interface segregation is expressed as a
   minimal capability-negotiated vtable; dependency inversion is expressed as a versioned C
   function table loaded per backend; the single-responsibility rule is read as "keep the
   interface narrow", not "keep the class small".
8. **Flexibility is bought, not sprayed.** Cost of indirection is accepted at the five seams;
   inside a component, a speculative abstraction without a second implementation is a defect.
9. **Language floor: C++20, adopt C++23 library pieces** (`std::expected`, `std::span`,
   `std::format`). Do not design against C++26 reflection or safety profiles, which are not
   available on the target toolchain, and do not treat contracts as a security mechanism.

## Reading list location

`AGENTS.md` contains only rules with a checking command. Books, papers and their critiques
live in `docs/references.md` (to be authored at kick-off) so that the agent-facing rules file
stays short and every rule stays machine-verifiable.

## Consequences

- Positive: every normative statement in this repository is either a narrow rule with a
  command, or an ADR with a cost of reversal. Reviewing an AI-generated change against the
  canon is mechanical instead of rhetorical.
- Positive: the hot path is explicitly exempted from abstraction pressure, which is the usual
  failure of layer-cake architecture in a renderer.
- Negative: five folders of `ports/` and `adapters/` ceremony, which this ADR rejects, are a
  familiar pattern that a contributor may expect; the rationale must be kept short and visible.
- Negative: "no speculative abstraction" will occasionally be read too broadly and block a
  refactor that would have paid off. The escape hatch is an ADR, not an exception without a
  record.
- Cost: the second implementation of each seam (test double or `null` backend) is real work
  that never ships to users. It is capped by requiring only the interface-level double, not a
  functional engine.

## Alternatives rejected

- Clean Architecture plus SOLID as the primary structure, with layer folders and one use-case
  class per user action. Rejected: layout cost with no gate value, and no vocabulary for the
  raster loop.
- A rules-free "pragmatic minimum" with no named canon. Rejected: shared vocabulary is needed
  to justify decisions to reviewers and to future readers of the repository.
