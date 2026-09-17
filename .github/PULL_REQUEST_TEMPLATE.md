## What

One paragraph. What changes for a user, and which delta or ADR it serves.

## Why now

The measurement or the gate that made this the right time (a baseline number, a corpus failure, a
user-visible gap). "It seemed good" is not an answer.

## How it is checked

Name the command and its output. A requirement without an artefact is not done
(`tools/spec-check.py` enforces the artefact, ADR-0011 R-M13).

- [ ] `sh tools/check.sh` green locally
- [ ] new or changed behaviour covered by a test that cites its requirement id
- [ ] capability `SPEC.md` updated in the same PR (including `Out of scope` if something was
      dropped)
- [ ] no new dependency, build flag or toolchain without an ADR that prices it (R-M9)
- [ ] diff is ~400 lines or less, or this PR is one link in a stack

## Trailer

```
Requirement: R<n>.<m> ...
Check: <command> -> <what it reported>
Evidence: <test or fixture files added/changed>
Generated-by: <agent|human> (implementation); verified-by: <human> (criteria and merge)
```
