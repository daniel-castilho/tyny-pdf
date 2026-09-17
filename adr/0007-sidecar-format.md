# ADR-0007: Annotation sidecar format

- Status: Accepted (ratified by the project owner, 2026-09-16)
- Date: 2026-09-16
- Deciders: project owner
- Related: ADR-0002 (layout), ADR-0005 (language), ADR-0006 (naming rules), ADR-0008 (name); deltas
D-1, D-4, D-5, D-6, D-3; decision D8

## Context

Delta D-1 is the one v1 feature the surrounding ecosystem does not have: an annotation set that
lives in a file the user owns, can diff, can put in git, and survives "someone re-exported the
PDF and now the hash changed". Making that work is a serialization decision more than a UI
decision, and it constrains several other parts of the project:

- the undo model (D-6) needs an identity for an annotation that survives a save, a merge and a
  re-anchoring;
- the projection into the PDF needs a way to tell "our write is still there" from "a third party
  edited this document";
- diffs are the product, so byte stability is a functional requirement, not cosmetics;
- text is the user's content and can be written in any language (the pt-BR case matters), while
  the repository itself is English-only (ADR-0005);
- re-anchoring has to be deterministic and explainable, or the UI will silently move a
  highlight into a different clause.

## Decision

### 1. File, discovery, identity

One file per document, next to it: `<document>.tynypdf.json`, where the suffix comes from
`docs/naming.md`. The sidecar references the document by content hash, never by path (a path
would leak usernames into a file people commit and would break the moment a document moves).

Discovery on open: read the adjacent sidecar. If it is missing and the document is known from
recent history, offer "attach a sidecar" (a file picker) instead of maintaining a private
database of who-was-where. No app-managed annotation store, by decision.

### 2. Format: JSON, canonical bytes

JSON over YAML (parser corner cases, indentation-sensitive diffs), over XML/XFDF (unreadable
diffs), over SQLite (undiffable, and the whole point is user ownership).

The byte layout is normative and machine-enforced by `tools/sidecar-fmt.py`:

| Rule | Why |
| --- | --- |
| UTF-8, LF, no BOM, one trailing newline | cross-platform diffs |
| object keys sorted lexicographically, two-space indent | the only stable ordering without a schema-declared order to maintain |
| `annotations` sorted by `(page asc, -y1, x0, id)` | reading order: page, then top-to-bottom, then left-to-right, then id, so appending an annotation does not shuffle the file |
| `replies` sorted by `(created, id)` | a thread must not reorder because a save touched it |
| geometry rounded to at most 3 decimals; integral floats printed as integers; scientific notation banned | otherwise every save changes `712.5000000000001` and the diff lies about what changed |
| timestamps are RFC 3339 UTC at second precision | milliseconds churn on every write and make two identical files differ |
| unknown keys are preserved verbatim | a sidecar written by a newer version, or annotated by hand, must not be silently damaged |

`sidecar-fmt.py check` runs over every fixture in the committed gate
(`.github/workflows/gates.yml`), and over every file a test writes once the test harness exists (PR
#5); a test
that produces a non-canonical file fails. The tool only treats `*.tynypdf.json` as a sidecar, which
is the same suffix rule `docs/naming.md` publishes.

### 3. Identifiers

`id` is 10 characters of RFC 4648 base32 (lowercase, alphabet `[a-z2-7]`), random, allocated at
creation and immutable. Random and short rather than sequential, because sequence numbers collide
the moment two people's sidecars are merged by git, and merging is the reason the file exists.
Replies carry their own `id` plus an optional `in_reply_to`, so threads survive a merge;
`tools/sidecar-fmt.py` rejects a reply that points at an unknown id.

Deletion is a tombstone: `deleted: true` with `modified`, so git shows the removal and a merge
does not resurrect the annotation. A cleanup command (`tynypdf-cli sidecar gc`, not in v1) drops
tombstones older than 30 days.

### 4. Geometry and anchoring

```
"page": 1,
"rect": [x0, y0, x1, y1],          // PDF user space, origin bottom-left, before rotation
"quads": [[tlx,tly,trx,try,blx,bly,brx,bry], ...],   // markup annotations, PDF QuadPoints order
"rotation_at_capture": 0,
"anchor": { "quote", "prefix", "suffix", "text_sha256", "page_label", "score", "state" }
```

The rect and quads are what gets drawn. The `anchor` block is reconciliation data only, and the
split is deliberate: a highlight anchored by text alone would render wrong whenever the extraction
is imperfect, and a rect alone dies on any geometry change.

Re-anchoring when `document.sha256` differs is a deterministic ladder, evaluated in order, with
the outcome recorded in the file (never silently applied):

1. **Same page geometry** (`page_size` matches) and same `text_sha256` for the anchored page:
   keep the rect. `state: "anchored"`.
2. **Same geometry, text changed:** search `quote` (normalized: NFKC, case folded, whitespace
   collapsed) within the page, weighted by `prefix`/`suffix` agreement, take the best match, and
   rebuild quads from the matched character spans. `state: "moved"`, `score` = match score.
3. **Geometry changed** (crop, resize, rotation): scale the rect by the page-size ratio, then run
   step 2 to refine. `state: "moved"`.
4. **Score below 0.5, or no match:** keep the original rect, mark `state: "unanchored"`, and show
   the annotation in a review list. The UI asks; it never guesses.

Rule: a re-anchoring pass writes `anchor.state` and `anchor.score` back into the file. That makes
"what did the tool move?" a diffable fact, which is also what makes it auditable.

Normalization details that must be shared with the viewer's search index: the quote is compared
after NFKC and case folding, but *rendered* from the document's own extraction, never from
`anchor.quote`. The user's text is data and may be in any script; nothing may reject non-ASCII
content (this is content, not repository language, per ADR-0005).

### 5. The PDF is a projection, and that is recorded

```
"document": { "sha256": "...", "pages": 4, "projection": { "digest": "...", "written_at": "..." } }
```

`projection.digest` is the digest of the annotation objects this sidecar last wrote into the PDF.
On open, three cases, each with a defined behaviour:

| Sidecar content changed? | `projection.digest` matches the PDF? | Behaviour |
| --- | --- | --- |
| no | yes | nothing to do |
| yes | yes | write the projection (normal edit path) |
| any | no | a third party edited the PDF: run a three-way reconcile by `id` (sidecar, our projection, current PDF), then present the conflict list in the UI. Never auto-merge silently. Never duplicate an annotation that is already in the PDF: import-by-id is what prevents the "every open added 12 highlights" bug class. |

The sidecar is the source of truth for the annotation set. Editing annotations directly in another
application is reconciled, not obeyed.

### 6. Explicitly out of scope for v1 (recorded so it is not re-litigated)

- **Form field values.** They live in the PDF (delta D-2). A form value in a sidecar that is not
  in the document prints differently from what the user sees: a trap.
- **Attached files and stamp images.** Binary payloads do not belong in a file whose purpose is to
  diff. Attachments stay in the PDF.
- **View state** (zoom, page, scroll, open tabs) in a separate `viewstate.json` beside the config
  directory, never in the sidecar: view state churns on interaction and would poison every diff.
- **Redactions** exist in the sidecar only while pending (`redaction` block). Applying one removes
  the entry and records it in the proof report (D-3); a pending redaction must not survive as a
  "note" that hides content.
- **Signatures.** Nothing cryptographic is stored here; the sidecar is not an integrity artifact.

### 7. Versioning

`format_version` is an integer, currently `1`, never reused with different semantics, never
lowered by a writer. Additive changes keep `1`; anything breaking bumps it, and the reader then
runs a migration that is *itself* covered by a fixture pair (before, after) in
`tests/fixtures/sidecar/`. The `generator` block is informational and excluded from the content
digest, so a save by a different tool version does not look like an edit by a person.

### 8. Budgets (to be measured in CI, not hoped for; wired with PR #5)

| Budget | Limit |
| --- | --- |
| Load and parse a sidecar with 5000 annotations | <= 50 ms |
| File size for that case | <= 4 MB |
| Canonicalize round-trip stability | byte-identical, checked on every fixture |
| Schema validity | every fixture and every emitted file validates against `docs/sidecar.schema.json` (JSON Schema draft 2020-12) |

## Consequences

- Positive: `git diff` over annotations is readable, which is the actual feature; the ordering and
  rounding rules are what make that true rather than a hope.
- Positive: re-anchoring is auditable because its result is written into the file.
- Positive: the identity model (random immutable ids, tombstones) makes a git merge of two
  sidecars a mechanical union, and the reconcile rule makes the PDF edit case explicit instead of
  silently duplicated.
- Negative: two sources of truth in the general sense (PDF plus sidecar), reconciled by a digest.
  This is the price of "annotations in a file you can diff"; the projection digest is what keeps
  it from becoming a corruption vector.
- Negative: geometry is stored in PDF user space with bottom-left origin, which is correct but
  foreign to UI code; the conversion lives in one module (`src/core/doc/geometry`) and is covered
  by unit tests for the four rotations and for `CropBox` differing from `MediaBox`.
- Negative: canonical bytes plus key sorting means the file is written by a code path that cannot
  stream: a 5000-annotation file is materialized in memory. Accepted given the budget above.

## Verified now, in this repository state

- `docs/sidecar.schema.json` passes `Draft202012Validator.check_schema`.
- `tests/fixtures/sidecar/example.tynypdf.json` validates against the schema and is byte-canonical;
  a parse and rewrite round-trip returns identical bytes (3158 bytes).
- `tools/sidecar-fmt.py self-test` reports `5/5 properties hold`: order independence, no
  scientific notation, integers stay integers, idempotence, and reading-order sort.
- `tools/lang-check.sh` reports `OK (13 files)`, so the schema, the fixture and the tool comply
  with ADR-0005 (user content in the fixture is data and is allowlisted by path).

## Alternatives rejected

- An application-managed database (`annotations.sqlite`) with export on demand. Rejected: it
  trades the one differentiator of the delta for convenience.
- Storing the document path in the sidecar. Rejected: leaks environment details and breaks on move.
- Millisecond timestamps and schema-declared key order. Rejected: they buy nothing and cost diff
  stability.
- Re-anchoring by text only, W3C-style. Rejected for PDF: no reflow, so the rect is authoritative
  for rendering, and broken `ToUnicode` maps are too common to trust for drawing.
