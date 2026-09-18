# Git workflow (operational)

This is the how-to version of [ADR-0009](../adr/0009-git-workflow.md). The ADR holds the reasoning;
this page holds the commands and the settings. If the two disagree, fix this page.

## The loop, every story

```sh
git switch main && git pull --ff-only
git switch -c feat/d6-undo-command-log
# ... work in small increments, one logical change each ...
sh tools/check.sh                       # the eight gates, before anything is pushed
git add -A
git commit                              # one commit per PR is fine; the merge squashes anyway
git push -u origin feat/d6-undo-command-log
gh pr create --fill --title "feat(core): undo through the IR command log" --body-file
.github/PULL_REQUEST_TEMPLATE.md
gh pr merge --squash --delete-branch    # only after checks are green; or turn on auto-merge
```

Rules that the loop encodes:

- `main` is the only long-lived branch and is always releasable. Releases are tags.
- **One PR is one logical change**, at most ~400 changed lines, at most one day of work.
- Commit subject uses [Conventional Commits](https://www.conventionalcommits.org):
  `feat|fix|refactor|perf|test|docs|build|ci|chore|revert[(scope)]: imperative`. The changelog is
  generated from these subjects, so a lazy subject is a bug in the release notes.
- Never merge your own red build, never `--no-verify`, never force-push shared branches.
- Rebase on your own branch before pushing (`git rebase main`), then `git push --force-with-lease`.
  Never rebase `main`.

## Stacked PRs (how a two-week delta becomes ten half-day PRs)

```sh
git switch -c d2/field-model            && # ... work, PR #101 against main
git switch -c d2/validation             && # ... work, PR #102 against d2/field-model
git switch -c d2/flatten                   # ... work, PR #103 against d2/validation
gh pr create --base d2/field-model ...     # open #102 against its parent, #103 against #102
```

As each parent merges, retarget the child: `gh pr edit 102 --base main`. GitHub auto-merge makes the
chain drain by itself once the checks pass, which is what keeps small PRs affordable solo. If a
stack is two deep and you are retargeting it more than once, that is a signal the delta should have
been split differently. One approved ticket at a time, and the regression test lands before the fix
it covers; a session that closes more than three of them re-runs the full gate set over the whole
interval before the push, because a green gate per commit does not add up to a green gate over the
stack and the accumulated diff is the thing a reviewer reads (`python3 tools/diff-scan.py --diff`).

## Hotfix to a published line

```sh
git switch -c fix/0.3.1-crash-on-open v0.3.0
# ... fix, tests ...
gh pr create --base release/0.3 --title "fix(core): ..."   # the release line, if it still exists
git cherry-pick <squash-commit-sha> && git push            # and forward-port to main
```

`release/0.3` is created only when a shipped line genuinely needs maintenance; otherwise a tag is
enough and a branch would be a liability.

## Branch protection - to be applied once, in the repository settings

Owner-only (a PR cannot tick these), and required before PR #2 merges:

1. Branch: `main`. "Restrict pushing" on.
2. "Require a pull request before merging", required reviewers `0` (solo project; the gate is CI,
   not a headcount), "Require branches to be up to date before merging" on.
3. "Require status checks to pass before merging" with the CI jobs listed: `gates` now, plus
   `core-linux`, `windows-msvc`, `windows-mingw-cross` when they arrive with PR #2.
4. "Do not allow bypassing the above settings" on - including administrators.
5. "Allow auto-merge" on, "Restrict deletions" on, "Require signed commits" off until there is a
   signing key for it to mean something.
6. "Delete branches automatically on merge" on.

This is free on a public repository. If the repository ever becomes private, items 2 to 6 move
behind a paid plan and the fallback is a CI job that rejects anything merged without a PR.

## Hooks

```sh
git config core.hooksPath .githooks # run this once per clone, in both the WSL and the Windows side
```

`.githooks/pre-commit` runs language, naming, sidecar canonical bytes, byte stability and spec
shape.
CI runs the same scripts, so the hook is a convenience and never the only gate. There is no skip
flag on purpose.

## Line endings and modes (a correctness section, not a style one)

ADR-0007 makes the sidecar byte-canonical, so a silent CRLF conversion is a broken build, not a
formatting preference. `.gitattributes` and `.editorconfig` are committed; do the rest per machine:

```sh
git config core.autocrlf false && git config core.fileMode false
git ls-files --eol | grep -v 'i/lf.*w/lf' || echo "worktree matches the index"
```

If `git ls-files --eol` shows a `w/crlf` on a text file, run `sh tools/canonical-check.sh` to find
which files, `python3 tools/sidecar-fmt.py fix <path>` for a sidecar, and re-add. Do not "fix" it by
editing the file twice.

## What a PR body has to contain

The template in `.github/PULL_REQUEST_TEMPLATE.md` asks for four lines of trailer. They are not
ceremony: `Requirement:` ids are cross-checked by `tools/spec-check.py` against the capability
`SPEC.md`, and `Evidence:` is what a reviewer (or you, in three weeks) reads instead of re-deriving
whether the behaviour is actually tested. An agent-written PR that says "verified" with no artefact
named is rejected by the check, not by taste - see `docs/lessons.md`.

## Retiring parts of this document

When a second contributor appears, add `CODEOWNERS` and set required reviewers to `1`; nothing else
changes. If squashing ever hurts bisection on a real commit series, the answer is to split the PR,
not to switch to merge commits - that is the whole reason ADR-0009 exists.
