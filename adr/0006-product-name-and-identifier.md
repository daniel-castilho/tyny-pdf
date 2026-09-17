# ADR-0006: Product name and reverse-DNS identifier

- Status: Superseded by ADR-0008 for the name; the rules and the table mechanics below stay in force
- Date: 2026-09-16
- Deciders: project owner
- Related: ADR-0002 (layout), ADR-0004 (supply chain), ADR-0005 (language), decision D16, D7b

## Context

The name has to be fixed before the first installer exists, because it is baked into things that
are expensive to change later: the MSIX/package identity, the `AppUserModelID`, registry keys,
the GNOME desktop file name, the bundle identifier, the winget package identifier, and the
on-disk sidecar file suffix. A rename after the first release is a support burden for users and
for upgrade paths.

The name is also the first thing a trust-oriented product can get wrong, so the choice is
constrained rather than tasteful:

| Constraint | Source |
| --- | --- |
| ASCII only in every artifact it appears in | ADR-0005 |
| Must not over-promise: no `Secure`, `Signed`, `Verified`, `Certified`, `Pro`, `Legal` | the anti-claim rule; v1 emits `PAdES-B-B (local)` only |
| Must not be a PDF dictionary key or a keyword in the vendored engine | grep noise: `/CropBox`, `/TrimBox`, `/MediaBox` appear in every PDF codebase, so naming the product after one destroys `git grep`, "go to symbol" and issue search |
| Short enough for a binary name (at most 8 characters) and unambiguous to type | the product's audience includes non-developers (decision D1 = effect) |
| Collisions measured, not guessed | table below |
| Identifier must chain to a domain under the owner's control | reverse-DNS convention; `ca.tyny.pulse` is already in use in the sibling project |

### Measured data (2026-09-16)

GitHub search, `total_count` from the repositories endpoint, measured 2026-09-16 and re-measured
the same day during the audit (the `in:name` column drifted by 1 to 2; the free-text column
reproduced exactly). The query strings are part of the measurement, so they are in the headers:

| Candidate | `"<token> in:name"` | `"<token> pdf"` (free text) | `"<token> pdf in:name"` | note |
| --- | --- | --- | --- | --- |
| `recto` | 1335 | 22 | 4 | front side of a leaf; short; phonetic in both languages |
| `verso` | 6680 | 60 | 7 | back side; heavier collision |
| `quire` | 669 | 25 | 1 | a gathering of pages; spelling is not phonetic |
| `vellum` | 989 | 33 | 4 | taken in adjacent markets |
| `folio` | 52404 | 233 | 39 | unusable as a search term |
| `palimpsest` | 399 | 7 | 1 | most distinctive; hard to type |
| `marginalia` | 808 | 38 | 2 | descriptive of annotations; taken by an existing search engine |
| `cropbox` | 53 | 3 | 1 | rejected: PDF keyword |
| `trimbox` | 9 | 6 | 1 | rejected: PDF keyword |

**Correction recorded 2026-09-16 (audit).** The first draft of this table had two columns and
labelled the second one "repos with the token plus `pdf`", which read as a name search; the 22 for
`recto` came from a free-text query, and the name-restricted number is 4. The measurements were
real, the label was not. Re-measured all three columns above.

Domain facts, checked with DNS-over-HTTPS (`dns.google`) because no other registry source was
reachable from the working environment:

- `tyny.ca` returns **NXDOMAIN with "Name servers refused query (lame delegation?)"**, and the
  parent `.ca` zone delegates it to `ns-2005.awsdns-58.co.uk` (verified: AS16509 Amazon, an
  AWS Route 53 delegation host). A referral in the parent zone means a registration with
  nameservers is set; the referenced zone itself answers **no records at all**. In short: the
  domain looks held, but it serves nothing.
- `releases.tyny.ca` does not resolve either. Any documentation that describes an update endpoint
  at that host is therefore describing infrastructure that does not exist today; per ADR-0004 the
  release notes must not claim it until it does.
- CIRA RDAP is access-restricted from here (`example.ca` returns the same restriction), so the
  registrant cannot be verified from this environment. **Open item, see below.**
- `cropbox.app` resolves to a live host, one more reason not to adopt that name.

## Decision

### 1. Names

| Thing | Value |
| --- | --- |
| Product display name | **Recto** |
| Expanded form, used only where search matters | `Recto PDF` |
| Repository | `recto` |
| Library target and public header | `pdfcore`, `include/pdfcore/pdfcore.h` (deliberately generic: the library is a component, not the brand) |
| Viewer executable | `recto` (`recto.exe`) |
| Headless CLI | `recto-cli` (matches the `tyny-cli` precedent in the sibling project) |
| Reverse-DNS identifier, all platforms | `ca.tyny.recto` |
| Publisher / vendor string | `Tyny` |
| Windows registry path | `Software\Tyny\Recto` |
| Linux desktop entry | `ca.tyny.recto.desktop` |
| macOS bundle identifier | `ca.tyny.recto` |
| winget package identifier | `Tyny.Recto` |
| Sidecar annotation file | `<document>.recto.json` |
| Environment variable prefix | `RECTO_` |
| Config directories | Windows `%APPDATA%\Tyny\Recto`; Linux `$XDG_CONFIG_HOME/tyny/recto`; macOS `~/Library/Application Support/Recto` |
| Log subsystem prefixes | `recto.core`, `recto.backend.mupdf`, `recto.ui`, `recto.cli` |

### 2. Rules the name carries

1. `docs/naming.md` is the single source of truth. Any new identifier, file suffix, registry key,
   environment variable or log prefix must be added there first; code and packaging read from it.
   This is the "one artifact, many consumers" pattern already used for the C ABI and generated
   requirement ids.
2. The engine's brand never appears in the product name, tagline or repository description; it
   appears in `README.md` under "Credits and licensing", as AGPL attribution requires.
3. The tagline must be a sentence the test suite can falsify. Accepted form:
   *"An offline PDF reader that keeps your annotations in a file you can diff."*
   Prohibited forms: any claim about security, legal validity, or "no telemetry" phrased as a
   promise rather than as a tested property (the CI gate is what makes it true).
4. `Recto` is a proper noun and therefore exempt from ADR-0005 in prose about the brand;
   surrounding text stays English.
5. The reverse-DNS identifier does not need DNS to work, so `ca.tyny.recto` is valid even while
   the zone is empty. Any *update* endpoint, however, needs a live host, and that is blocked on
   the open item below.

## Cost of swapping (deliberately low)

Per the reversibility principle, renaming is a scripted operation, not a refactor:

1. edit `docs/naming.md`;
2. run `tools/naming-sync.py` (generates `CMakePresets` product fields, the Tauri-free packaging
   manifests, `AppUserModelID`, desktop file name and sidecar suffix from that one file);
3. `tools/spec-check.py` and the packaging tests fail if a generated value was hand-edited.

The only values that cannot change without breaking users are the sidecar suffix and the registry
path, so both are frozen at the first public release, not at the first commit.

## Consequences

- Positive: one brand token, six characters, ASCII, pronounceable in Portuguese and English, and
  semantically about paper rather than about "PDF", which keeps search results about the product
  separate from search results about the format.
- Positive: `ca.tyny.recto` sits next to `ca.tyny.pulse`, so MSI, GPO and AppLocker rules for a
  second product follow an already-documented pattern.
- Negative: `recto` is a common word in Romance languages, so a bare GitHub search finds many
  unrelated repositories (measured above). Mitigation: always refer to the project as
  `daniel-castilho/recto` or `Recto PDF` in documentation and announcements.
- Negative: `Recto` is used by other companies in unrelated fields (agencies, print shops, a
  fintech in Latin America). A trademark clearance search in INPI, EUIPO and USPTO has **not**
  been performed and cannot be done from this environment. Treat it as a release gate for a public
  announcement, not as a blocker for development; if it fails, the swap cost above applies.
- Negative: the owner's apex domain currently serves nothing, so a product site and the update
  endpoint require configuration work outside this repository.

## Open items

1. Confirm the `tyny.ca` registration and add the DNS records, or pick a domain that is
   demonstrably under the owner's control before the first public announcement.
2. Trademark clearance for "Recto" in software (INPI/EUIPO/USPTO).
3. Reserve the GitHub repository name and, if the project will ever be announced on winget,
   confirm `Tyny.Recto` is free in the winget-pkgs repository.

## Alternatives rejected

- `CropBox` / `TrimBox` / `MediaBox`: the names are PDF dictionary keys present in every PDF
  codebase, including the vendored engine, so they make grep, symbol lookup and search
  permanently ambiguous.
- `Palimpsest`: the most on-theme option (writing scraped and rewritten, which is what the
  sidecar does to a page) and the most distinctive, but it is 10 characters, its spelling is not
  recoverable by ear, and the product's own audience has to recommend it to someone.
- `Quire`, `Folio`, `Marginalia`: measured collision or search noise, or an existing product in a
  nearby space.
- `Tyny Read` / `Tyny PDF`: maximal brand coherence, but the repository and binary names end up
  generic (`tyny-pdf`), which is the same searchability problem from the other direction.
