# ADR-0005: Repository language is English

- Status: Accepted (ratified by the project owner, 2026-09-16, as a non-negotiable)
- Date: 2026-09-16
- Deciders: project owner
- Related: ADR-0001, ADR-0002, ADR-0003; `AGENTS.md`; `tools/lang-check.sh`

## Context

The project is authored in Portuguese and English: the owner's working language is
Brazilian Portuguese, while the ecosystem the software belongs to (upstream engines,
conformance bodies, enterprise IT, potential contributors and upstream patches) reads and
reviews English. The owner has set the following as non-negotiable:

> Everything that goes into the repository is English: documentation, source code and
> configuration. Conversations may continue in Portuguese.

There is a concrete precedent for why this needs a gate and not an intention. The related project
in this family already declared an English-only rule for identifiers, comments, commit messages,
documentation and error strings, and still accumulated documents describing work that had not
been done. Language drift and claim drift are the same failure mode: unenforced prose.

Two further facts made this an architecture decision rather than a style note:

- The living-specification mechanism adopted from the SDD4J workflow deliberately supports
  writing EARS requirement prose in a configured local language. **We are declining that
  option.** Specs, requirement statements and their identifiers are English; the local-language
  affordance would split the corpus of readable requirements in two.
- The product itself is intended for Brazilian users, so the *application UI* needs Portuguese
  strings while the *repository* must not contain Portuguese as its working language. Those are
  different things and the rule must state the boundary, or the gate will fight the product.

## Decision

### 1. Scope of the rule

English is required, without exception, in: source code and comments; identifiers, file and
directory names; build and configuration files; `SPEC.md` capability specifications and EARS
requirement statements; ADRs; `README.md`, `CHANGELOG.md`, `AGENTS.md` and all other
documentation; commit messages and tags; issue and pull-request titles; user-visible string
**keys** and English source strings; log and error message templates; test names and test
case ids.

### 2. Explicit exemptions

- **Test data that is the subject under test.** Files under `tests/conformance/**`,
  `tests/fixtures/**` and `tests/approvals/**` may contain Portuguese, accented text, and
  domain terms, because the diacritics and font-fallback delta (D-4) is *about* that text. The
  gate allows that content but still requires the surrounding assertion names to be English.
- **Proper nouns and standards vocabulary**: `ICP-Brasil`, `MED-BR`, `Nota Fiscal`, `ISO 32000-2`.
  Translating them would make documents harder to search for. They are quoted, never declined.
- **Non-ASCII is allowed where it is data or mathematics**: `->` in a diagram, `<=`, `0.15`,
  `en-US`, copyright marks. The gate checks for *Portuguese language*, not for the ASCII range,
  with one exception: source files (the C and C++ family - `*.h`, `*.cc`, `*.cpp` - plus `*.cmake`,
  `*.py`, `*.sh`) must be ASCII, so
  that a mojibake incident is visible immediately instead of silently.
- **Product UI strings**: English is the source locale. Portuguese ships as a resource artifact
  (`locales/pt-BR.*` or equivalent), generated from the catalog, never hand-written into code.

### 3. The gate

`tools/lang-check.sh` (logic in `tools/lang-check.py`) runs in CI and locally from the pre-commit
hook, and fails the build when:

1. a source file contains a non-ASCII byte;
2. any tracked text file contains a token from a curated Portuguese blocklist
   (`nao`, `voce`, `tambem`, `assim`, `entao`, `quando`, `onde`, `feito`, `dando`, `usuario`,
   `codigo`, `linguagem`, `documento`, `arquivo`, `projeto`, `requisito`, and related forms)
   outside the allowlisted paths;
3. an allowlist marker is used without a reason.

A line may be exempted with `lang-check:allow reason=<text>` for an unavoidable quotation
(a translated error from an upstream document, a fixture filename). A whole-file exemption
`lang-check:allow-file reason=<text>` requires the reason to be non-empty, so the escape hatch
stays auditable.

The blocklist is short and deliberately low-noise: it catches Portuguese that was typed by
mistake, and it does not try to be a language detector. Review remains the backstop for fluent
but wrong-language prose, which is rare because the writing surface is small.

### 4. Consequences for the workflow

- Deliberation continues in Portuguese; `analysis/**` stays outside the repository, so
  translation is not a recurring cost. When a decision needs to be kept, it is written as an ADR
  in English at the moment it is accepted, as in this file and ADR-0001 to ADR-0004.
- Requirement ids and test names stay ASCII, so `--gtest_filter`, `ctest -R` and CI annotations
  keep working without quoting rules.
- Upstream contributions (the engine, the viewer family) need no translation step, which is the
  practical reason the rule is worth its friction.

## Enforcement

| Check | Command | Where |
| --- | --- | --- |
| Repository language | `tools/lang-check.sh` delegates to `tools/lang-check.py`; self-test via `--self-test` | CI plus pre-commit |
| Source files are ASCII | same script, mode `sources` | CI |
| Allowlist markers carry a reason | same script, mode `markers` | CI |
| English source locale exists and is the default | `tests/unit/test_locale_defaults.cc` | CI |

## Consequences

- Positive: one language for every artifact that outlives a conversation, including the ones an
  agent reads when it opens a task.
- Positive: the gate is a script in the repository, so the rule cannot silently rot the way an
  `AGENTS.md` bullet does.
- Negative: writing English prose costs the owner more time than Portuguese. Mitigated by the
  rule that an ADR is a short structured document (context, decision, consequences) and by
  keeping discussion out of the repository.
- Negative: a fixture legitimately containing Portuguese needs the allowlist, and allowlists
  attract abuse; the required reason plus the small allowlisted path set keeps that bounded.
