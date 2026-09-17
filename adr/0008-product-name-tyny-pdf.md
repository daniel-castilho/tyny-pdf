# ADR-0008: Product name is Tyny PDF (supersedes ADR-0006)

- Status: Accepted (owner decision, 2026-09-16)
- Date: 2026-09-16
- Supersedes: ADR-0006
- Related: ADR-0005, ADR-0007; `docs/naming.md`

## Context

ADR-0006 proposed `Recto` on search-uniqueness grounds and offered a cheap swap path: edit
`docs/naming.md`, run the sync, let the tests catch stragglers. The owner chose **Tyny PDF**
instead, and the repository `daniel-castilho/tyny-pdf` already exists (verified: public, default
branch `main`, one commit `05b62ec3` "first commit" containing only an 11-byte `README.md`).

The measured case for the new name is stronger than the case for mine, so this ADR records the
data rather than the preference:

Queries against the GitHub repositories search endpoint, re-run on 2026-09-16 during the audit; the
query string is part of the measurement:

| Query | `total_count` |
| --- | --- |
| `q=tyny-pdf` (free text) | 1 (this project) |
| `q=tyny pdf` (free text) | 1 |
| `q=tynypdf` (free text) | 0 |
| `q="tyny-pdf in:name"` | 1 |
| `q="tynypdf in:name"` | 0 |
| `q=recto pdf` (free text) | 22 |
| `q="recto pdf in:name"` | 4 |

A name inside the `tyny-*` family also makes the identifier scheme a copy of an already-published
pattern (`ca.tyny.pulse` in the sibling project), and it is pronounceable and spellable in
Portuguese without explanation, which matters for decision D1 (effect: users recommending it to
each other).

The cost ADR-0006 predicted for a "PDF"-bearing name is real but bounded: the word `pdf` occurs
constantly in the codebase, so `git grep pdf` is noise. The mitigation was already decided in
ADR-0006 rule 2 and ADR-0002: the library and its symbols are brand-free (`pdfcore`, `pc_`), and
internal modules are named after what they do (`doc`, `annot`, `form`), never `pdf*`. Grep noise
is then confined to file names and packaging, where the brand is wanted.

## Decision

1. Product name `Tyny PDF`; repository `tyny-pdf`; identifiers and artefacts per
   `docs/naming.md` (`ca.tyny.pdf`, `tynypdf`, `tynypdf-cli`, `tynypdf-worker`, sidecar suffix
   `.tynypdf.json`).
2. The sidecar suffix changes from the ADR-0007 draft (`.recto.json`) to `.tynypdf.json`. This is
   the last moment it is free: ADR-0007 freezes it at the first public release.
3. `docs/sidecar.schema.json` re-IDs to `urn:tynypdf:sidecar:1`. URNs are used instead of an
   `https://` URI because the domain does not serve anything yet (see below); a schema `$id` must
   not point at a URL that 404s or, worse, one day resolves to somebody else's content.
4. The tagline becomes: *"An offline PDF reader that keeps your annotations in a file you can
   diff."*
5. ADR-0006's open items update as follows:
   - **Domain: closed as to registration.** Evidence supplied by the owner (GoDaddy Domain Control
     Center for `tyny.ca`): renews **2027-03-19**, C$21.99/yr. The DNS gap found in ADR-0006 is
     confirmed and still open: the parent `.ca` zone delegates `tyny.ca` to an AWS Route 53
     nameserver that answers nothing, so there is no hosted zone with records.
   - **Trademark clearance: open.** "Tyny" is an existing brand in unrelated fields; the name is
     also deliberately close to "tiny". A clearance search for software (INPI/EUIPO/USPTO) has not
     been done and cannot be done from this environment.
   - **Name reservation: partially closed** by the existing repository. Remaining: confirm
     `Tyny.TynyPDF` is free in winget-pkgs, and decide whether to also claim
     `daniel-castilho/tyny-pdf-corpus` for the clause-keyed test corpus (ADR-0002, ADR-0007).

## Consequences

- Positive: the naming-family pattern, the update endpoint path and the eventual MSI/GPO rules for
  two products (`ca.tyny.pulse`, `ca.tyny.pdf`) now share one documented convention.
- Positive: executing this swap now is the verification of ADR-0006's claim that renaming is a
  scripted edit of `docs/naming.md`, not a refactor. It touched seven files plus one rename, and
  every gate still passes; `retired_tokens` in `docs/naming.md` now keeps a half-done rename from
  being mergeable.
- Negative: `Tyny PDF` reads as a family of utilities rather than a standalone brand, which is
  what a consumer-facing launch would want; accepted, because the launch audience (D1) arrives
  through people who already know `tyny.ca`.
- Negative: nothing in the project name signals "no account, no cloud", so that has to be said in
  the tagline and the README, where it can be falsified by a test.
