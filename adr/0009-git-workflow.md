# ADR-0009: Git workflow

- Status: Accepted (ratified by the project owner, 2026-09-16)
- Date: 2026-09-16
- Related: ADR-0001 (canon), ADR-0007 (canonical bytes), ADR-0008 (repo), `docs/lessons.md`

## Context

The owner's non-negotiable: every story gets its own branch, with commit, push, pull request and
merge into `main`. That shape is adopted. The word "GitFlow" is not: the 2010 model with
long-lived `develop`, `release/*` and `hotfix/*` branches plus cross-branch merges was built for a
team shipping coordinated versions with a QA gate, and it makes squash merging impossible. With
one person reviewing (and an agent producing much of the code), the scarce resource is review
capacity per sitting, not merge coordination.

Two environment facts constrain this: the working copy lives in WSL2 on the Linux filesystem
(ADR-0010), and ADR-0007 makes file byte layout normative. Both mean that line endings and
whitespace are correctness issues here, not style.

## Decision

1. **`main` is the only long-lived branch.** It is always releasable. No `develop`.
   `release/0.x` branches are created only when a shipped line genuinely needs fixes, and
   `hotfix/*` branches start from a tag and are cherry-picked back.
2. **The unit of work is the pull request, not the story.** Target: at most ~400 changed lines and
   at most one day of work. A delta such as D-2 that needs two weeks becomes a chain of stacked
   branches (`d2/field-model`, then `d2/validation`, then `d2/flatten`), each merged into `main` in
   order. Rationale: a three-week branch in a solo repository is where diffs stop being readable,
   and it is also where "the agent rewrote my other change" happens.
3. **Branch naming** `<type>/<delta>-<slug>`: `feat/d6-undo-command-log`, `fix/d2-tab-order`,
   `chore/ci-msvc-matrix`, `docs/adr-0011-...`. `type` from Conventional Commits
   (`feat fix refactor perf test docs build ci chore revert`), because the changelog is generated
   from it. Delete on merge.
4. **Merge method: squash.** One commit per PR on `main`, message = PR title plus the trailer
   block below. Bisectability comes from rule 2 (one logical change per PR), not from merge
   topology.
5. **Every PR body carries the machine-checkable record.** A trailer, so tooling and future
   readers can find the evidence:

   ```
   Requirement: R1.1 R3.1
   Check: python3 tools/spec-check.py -> 14 ids, 0 orphans, 10 pending
   Evidence: tests/unit/test_sidecar_writer.cc (3 new), sidecar-fmt check OK
   Generated-by: agent (implementation); verified-by: owner (tests and criteria)
   ```

   `Requirement:` ids are cross-checked by `tools/spec-check.py` (ADR-0011 R-M13): a PR may not
   close a requirement that has no test named after it, and the "green" state is written by CI,
   never by a person or an agent.
6. **Branch protection on `main`** (free on a public repository, which this one is): require a PR,
   require status checks to pass, forbid force-push and deletion, require the branch to be up to
   date before merge, delete head branches automatically. GitHub auto-merge is enabled so a stack of
   PRs queues and merges unattended, which is what makes rule 2 affordable solo.
7. **Line endings and modes are pinned in the repository**, not in personal config:
   `.gitattributes` with `* text=auto eol=lf` plus explicit `*.bat text eol=crlf`,
   `*.rc text eol=crlf`, binary declarations for PDFs and images; `core.autocrlf=false` in both
   the WSL and the Windows git config. `tools/canonical-check.sh` (in CI and in the pre-commit
   hook) re-verifies that every `*.tynypdf.json` fixture and every `docs/**` file is byte-canonical
   and CRLF-free. Without this, a Windows-side editor silently corrupts the format ADR-0007 just
   made normative.
8. **Hooks are in-repo**: `git config core.hooksPath .githooks`; `.githooks/pre-commit` runs the
   fast subset (`tools/lang-check.sh`, `tools/sidecar-fmt.py check`, `tools/spec-check.py --quick`,
   `clang-format --dry-run -Werror` on staged C++). CI runs the same scripts, so a hook can never
   be the only gate.
9. **Never `--no-verify`, never merge your own red build.** With an agent in the loop the failure
   mode is documented in the literature (a task marked "verified" with no test written); the
   response is that the check is a command, and the command is what merges.
10. **Local loop**: `gh pr create --fill --title ...` from WSL against the single working copy on
    the Linux filesystem; `git push --force-with-lease` for the rebase-before-merge, rebase onto
    `main` before pushing, and never rebase anything already shared and green (squash-merge makes
    the branch history disposable).

## Deliberately not adopted

- `develop` and the GitFlow merge topology; long-lived feature branches; direct pushes to `main`;
  merge-commits-per-PR (history full of noise for a solo project); PR templates longer than the
  trailer block above; a "review checklist" that no script enforces.

## Consequences

- Positive: review load per sitting is bounded, which is the only scarce resource in a solo + AI
  project; the changelog, the requirement trace and the bisect all come out of the same rule set.
- Positive: `main` being releasable means the release job is a tag, and the "explicit update"
  promise in README stays honest because it is exercised on every merge.
- Negative: stacked PRs on GitHub have no first-class support; each child PR must be opened against
  its parent branch and retargeted as the chain merges (or, for short chains, merged into `main` in
  sequence). That is a few minutes of tedium per stack, bought against the far more expensive habit
  of accumulating a week of unread diff.
- Negative: squash discards the internal commit structure of an AI session. The record therefore
  has to be in the PR body (rule 5), and `docs/lessons.md` is where the "how it actually went" text
  belongs.
