# Lessons

Append-only. An entry exists because something was believed, measured, and found wrong. Each entry
names the belief, the evidence, and the rule that came out of it, so that the next person - or the
next agent session - does not have to rediscover it. Nothing here is prose for its own sake: if an
entry has no rule, it is not a lesson, and if a rule is durable it belongs in an ADR, not here.

## 2026-09-16 - a roadmap asserted a capability that belonged to a binding, not the engine

The plan for "table extraction" credited the C engine. Measured against the engine's own feature
list, structured table finding lives in the Python binding layer, and the engine's C API exposes
text-page access, not tables. The task survived only because the wording changed to say what the
layer does.

**Rule:** a capability claim in a specification cites the API that provides it, at the version we
pin (R-M1 in [ADR-0001](../adr/0001-engineering-canon.md)). "The engine supports X" is not a
requirement; "`pc_doc_find_tables` returns N rectangles for corpus file Y" is.

## 2026-09-16 - a sibling project documented a release pipeline that its CI did not run

The old release runbook described an updater build, signing and notarisation. The workflow files
contained none of it, and two modules named in the documentation existed only as declarations in
`main.rs`. The documentation was not wrong once; it was wrong permanently, because nothing executed
it.

**Rule:** every documented command in this repository is either a script in `tools/` or a CI step,
and forward references to tools that do not exist yet are marked "(planned)" so that
`tools/docs-check.py` (PR #2) can distinguish a promise from a lie.

## 2026-09-16 - an agent marked a task verified without writing a test

Documented in the literature on spec-driven development and reproduced here on the first day: a task
list said "checked", no test existed.

**Rule:** "done" is written by CI, never by a person or an agent. `tools/spec-check.py` fails a
`Verification:` artefact that does not exist once the capability has code, and a test that does not
cite the requirement id it proves (R-M13, [ADR-0009](../adr/0009-git-workflow.md) rule 5).

## 2026-09-16 - an audit of this seed found three defects, and none of them were in the tools

The gate run is reproducible (every number in the README re-executed to the unit). What was
wrong was prose that *reported* measurements: a table column labelled "repos with the token plus
`pdf`" holding free-text counts (`recto` 22 free text vs 4 in name), a concurrency figure
described as a throughput ratio ("13.3x throughput" for what the source measured as 13.3x the
*time* at ten threads versus 3.3x at ten processes), and a specification written in present
tense about infrastructure that has never run ("green in CI today", when the remote repository
has one commit and no workflow history).

**Rule:** a measurement in a document carries its query, its unit and its date - `"<token> pdf"` is
not the same measurement as `"<token> pdf in:name"`, and "13.3x time" is the opposite of "13.3x
throughput". And a document describing a pipeline says whether the pipeline has executed: present
tense is a claim about the world. Where the fix is a label, record the correction in the document
(`adr/0006` does), so the reader of the old copy can see what changed.

## 2026-09-16 - the "nobody else does it" premise was checked against the wrong version

The gap analysis that justified the six deltas measured the incumbent against SumatraPDF 3.6.1, the
latest *stable* release (2026-04-06, verified via the releases API). Its own pre-release changelog
already lists an Edit PDF mode with an annotation browser, Apply Redactions, undo/redo built on
MuPDF's journal covering annotations and form fields, signing from the Windows certificate store,
Document Properties showing LTV/RFC 3161/PAdES/EU LOTL, about twenty CLI tools, and a local-agent AI
chat. "The incumbent cannot do it" was true of a version nobody should be compared to and false
of the one that matters.

**Rule:** a competitive claim names the version *and the channel* (stable, pre-release,
nightly), and is re-read at every milestone, not once at kick-off. A delta survives on mechanism
(where the data lives, whether the proof is an artefact, whether the API is public), not on the
competitor's gap. This is why `docs/kickoff.md` section 4 is written as mechanism, and why M0
measures 3.7 pre-release besides 3.6.1.

ADR-0007 defines canonical bytes for the sidecar; ADR-0010 puts the working copy in WSL2 while
Windows tools still reach it. A single CRLF conversion in a fixture, or a trailing space added by an
IDE, invalidates a hash-comparison test without touching meaning.

**Rule:** `.gitattributes` and `.editorconfig` own line endings and whitespace;
`tools/canonical-check.sh` proves both hold on every commit, and `core.autocrlf=false` is set in
both environments.

## The front page is bound by the same gates as the code

The README was written to match the visual model of a sibling project: emoji in the bullet list,
box-drawing characters in the directory tree, shields at the top. Our own `tools/lang-check.py` R1
allows a short list of typographic marks in prose and nothing else, so the emoji and the tree glyphs
failed the gate on the first run. The tempting fix was the file-level exemption marker on the
README - and it would have exempted the one file most likely to acquire a stray Portuguese
word or an unverifiable claim.

**Rule:** when a rule blocks the copy, either change the copy or change the rule *in writing*, in
the ADR that owns it; never waive it for the file that the rule is currently inconvenient for. The
same run produced a smaller twin of this mistake: a comment claiming the title emoji was "the
exception the repository grants itself", which no decision had granted. Both were fixed in the
text.

Related: `docs/kickoff.md` promised the CI job would be "badge-free"; a project with badges and no
CI history needs a sharper line than none-versus-all. The recorded rule is *no status badge until
the job that can fail it exists*; static stack badges are allowed because they state facts
(language, platform, engine, licence) rather than measurements.

The same run found the tool-side half of the lesson. Editing the README left a code fence with its
closing marker glued to the end of a text line, and `tools/docs-check.py` walked the rest of the
file as "inside a fence": three over-length lines and a broken block went unnoticed, and the check
printed OK. A checker that skips a region has to fail when it cannot tell where the region ends.

**Rule:** a document check may skip content, but never silently - an unbalanced construct is a
finding, not a waiver. `docs-check.py` now reports an unclosed fence, and its self-test set covers
both the unbalanced case and a legitimately indented fence inside a list item.

## 2026-09-17 - a published tree was assumed to be the tree we had

Story 1.1's baseline measured `origin/main` and found 24 files where the maintained seed had 48:
the push had dropped every dotfile - `.editorconfig`, `.gitattributes`, `.githooks/pre-commit`,
`.github/PULL_REQUEST_TEMPLATE.md`, `.github/workflows/gates.yml` - plus all eleven `adr/*.md`,
`src/features/sidecar/SPEC.md` and the sidecar fixture. The mechanism is a filesystem copy that
skips dotfiles: `cp -r` does, `rsync -a` does not, and `git add -A` from inside the tree cannot.
The published repository briefly advertised decisions and a gate that its own `main` did not
contain.

**Rule:** the published tree is never assumed from the working copy. The parity check is
`git ls-tree -r --name-only origin/main | wc -l` plus a byte diff against a clean clone of `main`,
and the tree is produced with `git init -b main`, `git config core.hooksPath .githooks`, `git add
-A`, and the staged list checked against the tracked list before the first commit. Story 1.1 keeps
the measured result in `epic-1-dod.md` section 1.0.

## 2026-09-17 - the seed and its two fixes were pushed straight to `main`

The first three commits of this repository (`9fc2d52`, `94f1950`, `0d64999`) landed directly on
`main`, so `pulls?state=all` was `[]` while ADR-0009 and `docs/git-workflow.md` said branch-per-PR.
The flow was bypassed exactly once, to get a coherent tree onto `main` before the protection that
would have blocked the direct push existed - which is also the moment the rule is meant for.

**Rule:** the repository flow is PR-only from Story 1.1's merge forward. `main` is protected so the
direct push cannot recur, and the deviation is written here so a later reader never treats three
history commits as precedent.

## 2026-09-17 - a ported gate passed its own self-test only where it was run before

`tools/diff-scan.py` arrived as the fruit of the AkitaOnRails "talking about my AI skills" article,
ported from the sibling workspace. Its 17-case `--self-test` reported 16/17 here: the `--staged`
case expected rc=3 "outside a git checkout", but `staged_diff()` ran `git rev-parse` in the
process cwd instead of the `--root` that the test pointed at, so inside a real checkout it read a
diff (rc != 3) and the property failed. The sibling's run had passed because its cwd was not a
git tree. The bug is that a gate's default path silently ignored its own `--root`.

**Rule:** a self-test must prove the property in the repository it ships in, not in the one it was
written in. Ported gates are re-run locally before the PR claims them, and `tools/gates-selftest.sh`
runs every tool's own self-test so a regression of exactly this kind fails the gate set, not a
review reading.

## 2026-09-17 - a vendored engine is third-party code, and a change gate reads the project's change

The first `tools/diff-scan.py --tree` run reported 250 findings, every one inside
`third_party/mupdf/`: the engine's own manifests and vendored test binaries fail D7/D8 because they
are upstream's files, not ours. But `tools/format-check.sh` had already decided this: it excludes
`third_party/` from the style gate, and the layering budget counts backend lines but never the
engine's. The gate as ported had no such carve-out, so it graded 60 thousand lines of someone
else's drop.

**Rule:** a change-hygiene gate scans the change a PR makes and the project files it lands in;
vendored code is excluded the same way the other gates exclude it, with the reason in the rule
(ADR-0004). A tool that adds findings it cannot fix is a tool the next PR will quietly bypass.

## 2026-09-19 - a count typed into a document is a defect with a due date

`tools/check.sh` ran fourteen sections and printed `1/11` through `9/11`, because each banner had
its denominator typed into it and the four sections added by later PRs updated only their own
lines. The prose had the same disease where no gate can see it: `README.md` promised "all eleven
sections" and "9/9 suites", `docs/coding-standards.md` said "ten sections" and "8/8",
`docs/testing-playbook.md` said "nine checks in eleven sections", `docs/dev-environment.md` said
"the same six gates", two tree comments said "the eight checks", while the runners printed 14
sections and 13 suites. No check could have caught any of it: a sentence that names a number has
no path for `docs-check.py` to resolve.

**Rule:** a count a command prints is quoted with that command, or not quoted at all. Instructions
say `sh tools/check.sh` and "every section it has"; a report of state keeps the number but pastes
it from the command in the same sitting, because a pasted output is evidence and a typed memory is
a claim. Inside a script, derive it: `tools/check.sh` counts its own `sec` lines the way
`tools/gates-selftest.sh` counts its suites, so a section added later is one line and not ten
edits plus a documentation sweep. Mirrored values follow the same rule - the domain and the app id
live in `docs/naming.md`, and a document reaches them through
`python3 tools/naming-sync.py get identifiers.app_id`, never by copying.


## 2026-09-20 - a merged PR is not evidence that the change landed

PR #30 was merged with corrections listed as complete that its own diff did not contain:
`docs/release-runbook.md` was not in the twelve changed files and still opened with "there is no
code, no build system, no CI history"; `docs/lessons.md` gained no rule and `CHANGELOG.md` gained
no entry, both of which the PR body had promised; three of the counted sentences survived in
`docs/git-workflow.md`, `docs/testing-playbook.md` and `docs/release-runbook.md`; and a
`conan.lock.bak` was added by the same commit, which is the one file type this repository had no
ignore rule for. The title claimed "milestone 1-5 implementation" over a twelve-file docs-and-gates
diff, while `spec-check.py` printed 17 requirements with no artefact.

**Rule:** after a merge, re-run the measurement against the merged tree and paste it - one command
per claim, `git show --stat` for what a PR touched, `grep -c` for what survived. A green CI run
says the gates did not object, not that the change was complete. And a PR title is an archaeological
record: it states what the diff does, never what the milestone needs.


## 2026-09-23 - a keep verdict with unmeasured rows hidden is a kill with its wins hidden

The M1 spike verdict (story 5.5) is "keep": a hand-written Win32/Direct2D surface holds the floor.
Five of the seven M1 criteria are measured on the reference machine - cold start median 184.997 ms,
DPI byte-for-byte at 150%/200%, wheel p99 <= 0.324 ms, caret headless over combining marks, and the
UIA provider announced below. Two criteria cannot be measured yet because story 1.5 does not paint
page content: the 4000x3000 blit at 60 fps and the RSS-at-1000-pages row (5.3 is still unchecked).
The temptation is to "protect" the keep by writing zeros or happy prose over those rows. Doing so
turns the spike's whole point - measured, not estimated (kickoff §M1) - into decoration, and the
next team that reads it cannot tell the surface held the floor from the surface was never asked.

**Rule:** a spike verdict quotes the kickoff table verbatim and marks each row measured / not
measured; a row blocked by missing machinery says so and names the story that unblocks it, rather
than passing it with a number nobody took. The kill criterion exists to fire on blit/RSS data
when the content viewer exists (kickoff §12: Skia revisited only then); a keep today is a
statement about the surface and its seams, not about rows the viewer cannot produce.
