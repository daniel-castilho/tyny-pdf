#!/usr/bin/env python3
"""Diff and tree hygiene scan: the classes of change a reviewer must see before merging.

Why this gate exists and what it is not: the other gates read the repository's own promises
(`docs-check.py`), its bytes (`canonical-check.sh`), its shape (`layering-check.sh`). None of them
reads a *change*. This tool reads a unified diff, or the working tree when there is no diff to
read, and reports the eight classes that a narrative PR body hides. It is a static scan: it runs no
code from the patch, resolves nothing over the network, and never prints a pass for a class it
could not see (`docs/lessons.md`, "a check that cannot detect its own violation").

Rules, with the source that owns each:

  D1  exec-bit surprise. A new or mode-changed file with mode 100755 outside the allowlist
      (`tools/**`, `*.sh`, `.githooks/**`) is a finding. An executable dropped into a tree that
      nobody runs on purpose is how a payload arrives (ADR-0004 section 1).
  D2  symlink. Mode 120000 anywhere, or a tree entry that is a symlink, is a finding: a link
      escapes the checkout boundary and the Windows side of this project resolves it differently.
  D3  bidi and zero-width controls. U+202A-U+202E, U+2066-U+2069, U+200B-U+200F, U+FEFF in any
      text file or added line. These are invisible in a diff view and change what a reader
      believes the line says (the GitHub-adopted defence against the 2021 bidi attack).
  D4  confusable codepoints. A non-ASCII letter that maps onto an ASCII identifier character
      (Cyrillic/Greek lookalikes) inside a path, an include line or a `#define` - the Trojansource
      family. Reported with the codepoint so the reviewer can judge it, never auto-fixed.
  D5  workflow event risk. `.github/workflows/**` may not combine a pull-request-shaped trigger
      (`pull_request_target`, `workflow_run`, `issue_comment`, `repository_dispatch`) with
      `secrets.`, with `permissions: write-all`, or with a checkout of the PR head. Any
      `pull_request_target` is a finding on its own: an engine project has no use for it.
  D6  unpinned actions. `uses: owner/repo@v1`, `@main`, `@master`, any ref that is not a full SHA.
      A tag is mutable; ADR-0004 section 1 pins by commit and integrity for the same reason the
      Conan graph is pinned in `conan.lock`.
  D7  dependency-name similarity. A requirement in a manifest within edit distance 1 of a name in
      the seed list below, or equal to a name on the known-squat list, is a finding: "close enough
      to be a mistake, plausible enough to pass review" is the typosquat and hallucinated-package
      pattern. Offline by design; the registry check belongs to `tools/deps-refresh.py`.
  D8  manifest and lock drift. A manifest changed in the diff without its lockfile (or the reverse)
      is a finding, and a `conan.lock` whose revisions are not 40-hex or whose integrity hash is
      not 64-hex is a finding, because a hand-written lock is the failure `docs/dependency-policy.md`
      section 1 rule 4 names.

Usage:
  python3 tools/diff-scan.py --tree                  scan the working tree (what CI cannot skip)
  python3 tools/diff-scan.py --diff PATCH            scan one unified diff
  python3 tools/diff-scan.py --diff -                read the diff from stdin
  python3 tools/diff-scan.py --self-test             prove every rule above can still fail

Exit codes: 0 clean, 1 findings, 2 usage error, 3 nothing scannable (which is not a pass).
"""

from __future__ import annotations

import argparse
import re
import sys
import unicodedata
from pathlib import Path

# third_party is the vendored engine (ADR-0004): the gate reads *our* change, not upstream's
# 60000-line drop, and the engine's own manifests are not ours to police.
SKIP_DIRS = {'.git', 'build', 'node_modules', '__pycache__', '.venv', 'dist', 'third_party'}

# Files this repository expects to be executable; anything else that arrives executable is a find.
EXEC_ALLOW = re.compile(r'^(tools/|\.githooks/)|\.sh$')

BIDI = {0x202A, 0x202B, 0x202C, 0x202D, 0x202E, 0x2066, 0x2067, 0x2068, 0x2069,
        0x200B, 0x200C, 0x200D, 0x200E, 0x200F, 0xFEFF}

# Non-ASCII characters that render like an ASCII identifier character - the Cyrillic and Greek
# lookalikes the Trojansource family uses. Not a general confusables table: only the ones that have
# appeared in real attacks, kept reviewable by a human. Written as code points, because a source
# file containing them looks exactly like the defect D4 reports (ADR-0005).
CONFUSABLE = {chr(c): a for c, a in [
    (0x0430, 'a'), (0x0435, 'e'), (0x043E, 'o'), (0x0440, 'p'), (0x0441, 'c'), (0x0443, 'y'),
    (0x0445, 'x'), (0x0456, 'i'), (0x04CF, 'l'), (0x0501, 'd'), (0x0391, 'A'), (0x0392, 'B'),
    (0x0395, 'E'), (0x0396, 'Z'), (0x0397, 'H'), (0x0399, 'I'), (0x039A, 'K'), (0x039C, 'M'),
    (0x039D, 'N'), (0x039F, 'O'), (0x03A1, 'P'), (0x03A4, 'T'), (0x03A7, 'X'), (0x03B1, 'a'),
    (0x03B9, 'i'), (0x03BF, 'o'), (0x03C1, 'p'), (0x0410, 'A'), (0x0412, 'B'), (0x0415, 'E'),
    (0x041A, 'K'), (0x041C, 'M'), (0x041D, 'H'), (0x0422, 'T'), (0x0425, 'X'),
]}

MANIFESTS = {
    'conanfile.py': ['conan.lock'],
    'package.json': ['package-lock.json', 'yarn.lock', 'pnpm-lock.yaml'],
    'pyproject.toml': ['uv.lock', 'poetry.lock', 'requirements.lock'],
    'requirements.txt': ['requirements.lock'],
    'Cargo.toml': ['Cargo.lock'],
    'go.mod': ['go.sum'],
}

# Names this project plausibly depends on. The seed list makes "one edit away" checkable; growing
# it is a documentation change, not a code change.
SEED_NAMES = ['mupdf', 'harfbuzz', 'freetype', 'zlib', 'libpng', 'libjpeg-turbo', 'openjpeg',
              'googletest', 'openssl', 'curl', 'pyyaml', 'jsonschema', 'conan', 'cmake', 'ninja',
              'clang-format', 'requests', 'pillow', 'pytest', 'hypothesis', 'zstd', 'lcms2']

# Known-squat or hallucinated package names seen in public advisories and in LLM-generated lists.
# A seed, not a database: it is here so an obviously bad name never merges.
SQUATS = ['mupdf-js', 'mupdflib', 'harfruzz', 'harfbrush', 'freeetype', 'zlib-ng2',
          'googltets', 'googletst', 'openaii', 'reqeusts', 'requests2', 'pyaml', 'json-scheme',
          'clangfromat', 'conann', 'ninja-build-system', 'pillow-py']

REQ_LINE = re.compile(
    r"""(?:requires\s*=|require\(|\b[a-z_]+\s*=\s*)?["']([a-z0-9][a-z0-9._+-]{1,60})["']?\s*[=<>~]?""",
    re.I)


def edit_distance_one(a: str, b: str) -> bool:
    """True when a and b are one insertion, deletion or substitution apart."""
    if a == b:
        return False
    if abs(len(a) - len(b)) > 1:
        return False
    if len(a) == len(b):
        return sum(1 for x, y in zip(a, b) if x != y) == 1
    long_, short_ = (a, b) if len(a) > len(b) else (b, a)
    for i in range(len(short_)):
        if long_[:i] + long_[i + 1:] == short_:
            return True
    return long_[1:] == short_ or long_[:-1] == short_


def scan_text(name: str, text: str, added_only: bool = False) -> list:
    """D3 and D4 over the lines that a change touches."""
    finds = []
    for i, line in enumerate(text.split('\n'), 1):
        if added_only and line.startswith('-') and not line.startswith('---'):
            continue
        if added_only and not line.startswith('+') and not line.startswith('+++'):
            continue
        payload = line[1:] if line.startswith('+') else line
        cps = {ord(c) for c in payload if ord(c) in BIDI}
        if cps:
            finds.append(f'{name}:{i}: D3 invisible control codepoint(s) '
                         + ', '.join(f'U+{c:04X}' for c in sorted(cps)))
        conf = {c for c in payload if c in CONFUSABLE}
        if conf:
            pairs = ', '.join(f'{c!r}=U+{ord(c):04X} looks like ASCII {CONFUSABLE[c]!r}'
                              for c in sorted(conf))
            finds.append(f'{name}:{i}: D4 confusable codepoint in text a reviewer reads as ASCII '
                         + pairs)
    return finds


def scan_workflows(root: Path, problems: list) -> None:
    """D5 and D6, on the workflow files as they stand or as the diff makes them stand."""
    files = sorted(list(root.glob('.github/workflows/*.yml'))
                   + list(root.glob('.github/workflows/*.yaml')))
    if not files:
        return
    for wf in files:
        text = wf.read_text(encoding='utf-8', errors='replace')
        rel = wf.relative_to(root).as_posix()
        pr_shaped = bool(re.search(r'^\s*(pull_request_target|workflow_run|issue_comment'
                                   r'|pull_request_review_comment|repository_dispatch)\s*:',
                                   text, re.M))
        head_checkout = bool(re.search(r'actions/checkout@[0-9a-f]{40}\s*(?:#[^\n]*)?\s*\n'
                                       r'(?:\s*\n)*\s*with:\s*\n\s*ref:\s*\$\{\{\s*github\.event'
                                       r'\.(?:pull_request|issue)\.(?:head|merge)_ref', text))
        if pr_shaped:
            problems.append(f'{rel}: D5 a pull-request-shaped trigger gives a fork control over '
                            'this job; there is no use for it in an engine-facing repository')
        if head_checkout:
            problems.append(f'{rel}: D5 checkout of the PR head ref inside a trigger that carries '
                            'repository secrets')
        if re.search(r'^\s*permissions:\s*write-all\s*$', text, re.M):
            problems.append(f'{rel}: D5 `permissions: write-all` is not a policy, it is an absence '
                            'of one')
        for i, line in enumerate(text.split('\n'), 1):
            m = re.search(r'uses:\s*([^\s#]+)@([^\s#]+)', line)
            if not m:
                continue
            ref = m.group(2)
            if not re.fullmatch(r'[0-9a-f]{40}', ref):
                problems.append(f'{rel}:{i}: D6 action ref {ref!r} is mutable; pin to a full '
                                'commit SHA and leave the tag in the comment')
        if re.search(r'secrets\.', text) and 'pull_request' in text:
            problems.append(f'{rel}: D5 the file reads `secrets.` and is triggered by pull_request; '
                            'say in a comment which job needs it and why, or drop it')


def scan_manifests(root: Path, tree_mode: bool, changed: set, problems: list,
                    added_text: dict | None = None) -> None:
    """D7 and D8, over manifests and lockfiles present in the tree or touched by the diff."""
    locks = {lock for names in MANIFESTS.values() for lock in names}

    def interesting(name: str) -> bool:
        return name in MANIFESTS or name in locks

    if tree_mode:
        rels = [q.relative_to(root).as_posix() for q in root.rglob('*')
                if q.is_file() and interesting(q.name) and not _skipped(q, root)]
    else:
        rels = [n for n in changed if interesting(Path(n).name)]
    for rel in sorted(set(rels)):
        base = Path(rel).name
        p = root / rel
        if p.is_file():
            text = p.read_text(encoding='utf-8', errors='replace')
        elif added_text and rel in added_text:
            # the tree has not been patched (a review checkout, a bare patch file): the added lines
            # are what the change actually says, so D7 and D8 read those rather than skipping
            text = added_text[rel]
        else:
            problems.append(f'{rel}: D8 the change touches this file but it can be read from '
                            f'neither {root} nor the patch, so the manifest rules here did not run')
            continue
        if base in MANIFESTS:
            for i, line in enumerate(text.split('\n'), 1):
                for m in REQ_LINE.finditer(line):
                    dep = m.group(1).strip('.,;"\'()').lower()
                    if len(dep) < 3 or dep in ('the', 'and', 'self'):
                        continue
                    if dep in SQUATS:
                        problems.append(f'{rel}:{i}: D7 {dep!r} is on the known-squat list, so '
                                        'the name needs a registry check by '
                                        'tools/deps-refresh.py before it merges')
                        continue
                    if dep in SEED_NAMES:
                        continue
                    near = [k for k in SEED_NAMES if edit_distance_one(dep, k)]
                    if near:
                        problems.append(f'{rel}:{i}: D7 {dep!r} is one edit from '
                                        f'{near[0]!r}: confirm the intended package, not a typo')
        if tree_mode and base in MANIFESTS:
            missing = [lock for lock in MANIFESTS[base] if not (p.parent / lock).exists()]
            if missing and _declares_deps(text):
                problems.append(f'{rel}: D8 the manifest declares requirements and none of '
                                + ', '.join(missing) + ' exists, so nothing pins what resolves')
        if not tree_mode and base in MANIFESTS:
            if not ({Path(n).name for n in changed} & set(MANIFESTS[base])):
                problems.append(f'{rel}: D8 the change edits a manifest without editing its '
                                'lockfile, so the tree and the lock disagree after the merge')
        if base in locks:
            for i, line in enumerate(text.split('\n'), 1):
                # A Conan revision is the 32-hex MD5 or 40-hex hash Conan printed; a timestamp
                # suffix (%1600000000.123) is fine. Anything else that looks like one is a lock a
                # resolver did not write.
                bad_rev = [r for r in re.findall(r'#([0-9A-Za-z]+)', line)
                           if len(r) not in (32, 40)]
                bad_hash = [h for h in re.findall(r'sha256:([0-9A-Za-z]+)', line)
                            if len(h) != 64]
                if bad_rev or bad_hash:
                    offender = (bad_rev + bad_hash)[0]
                    problems.append(
                        f'{rel}:{i}: D8 {offender[:16]} is neither a Conan recipe revision '
                        '(32- or 40-hex) nor a sha256 integrity hash (64 hex); a lockfile is '
                        'written by the resolver, never by hand (docs/dependency-policy.md '
                        'section 1 rule 4)')
                    break


def _declares_deps(text: str) -> bool:
    return bool(re.search(r'requires\s*=|dependencies\s*=|install_requires|"[a-z0-9._-]+"\s*:\s*"',
                          text))


def _skipped(p: Path, root: Path) -> bool:
    rel = p.relative_to(root).as_posix()
    return any(part in SKIP_DIRS for part in rel.split('/')[:-1])


def parse_diff(text: str):
    """Return (files, added_lines_by_file, changed_names) from a unified diff."""
    files, added, changed = [], {}, set()
    cur = None
    for line in text.split('\n'):
        m = re.match(r'^diff --git a/(\S+) b/(\S+)', line)
        if m:
            cur = m.group(2)
            files.append(cur)
            changed.add(cur)
            changed.add(m.group(1))
            added.setdefault(cur, [])
            continue
        if cur is None:
            continue
        m = re.match(r'^(new file|deleted file|old|new) mode (\d{6})', line)
        if m:
            added[cur].append(('MODE', m.group(0)))
            continue
        if line.startswith(('+++ b/', '--- a/')):
            changed.add(line.split(' ', 1)[1].lstrip('ab/').strip())
            continue
        if line.startswith('+'):
            added[cur].append(('LINE', line))
    return files, added, changed


def check_tree(root: Path) -> list:
    problems = []
    for p in sorted(root.rglob('*')):
        if p.is_dir() or _skipped(p, root):
            continue
        rel = p.relative_to(root).as_posix()
        try:
            st = p.lstat()
        except OSError:
            continue
        import stat as S
        if S.S_ISLNK(st.st_mode):
            problems.append(f'{rel}: D2 the entry is a symlink; this repository has none and a '
                            'link reads outside the checkout')
        elif p.exists() and (st.st_mode & S.S_IXUSR) and not EXEC_ALLOW.search(rel):
            problems.append(f'{rel}: D1 executable bit set outside the tools allowlist')
        if rel.endswith(('.py', '.sh', '.md', '.txt', '.json', '.yml', '.yaml', '.cmake',
                         '.c', '.cc', '.cpp', '.h', '.hpp')) and p.is_file():
            try:
                problems.extend(scan_text(rel, p.read_text(encoding='utf-8', errors='replace')))
            except (UnicodeDecodeError, OSError):
                pass
    scan_workflows(root, problems)
    scan_manifests(root, True, set(), problems)
    return problems


def staged_diff(root: Path) -> str | None:
    """The index content of the checkout at `root`, when there is one. None when git is not there."""
    import subprocess
    if subprocess.run(['git', 'rev-parse', '--git-dir'], capture_output=True,
                      cwd=root).returncode != 0:
        return None
    return subprocess.run(['git', 'diff', '--cached'], capture_output=True, cwd=root,
                          text=True).stdout


def check_diff_text(text: str, root: Path) -> list:
    files, added, changed = parse_diff(text)
    problems = []
    for f in files:
        for kind, payload in added.get(f, []):
            if kind != 'MODE':
                continue
            if re.search(r'mode 100755', payload):
                if not EXEC_ALLOW.search(f):
                    problems.append(f'{f}: D1 {payload.strip()} - a new executable outside tools/')
            if re.search(r'mode 120000', payload):
                problems.append(f'{f}: D2 {payload.strip()} - the change adds a symlink')
        body = '\n'.join(p for k, p in added.get(f, []) if k == 'LINE')
        if body:
            problems.extend(scan_text(f, body, added_only=True))
    if any(f.startswith('.github/workflows/') for f in changed):
        # the workflow rules read the file as it will stand after the patch: the same file the
        # reviewer would have to audit by eye, so the scan points at it rather than guessing
        scan_workflows(root, problems)
    added_text = {f: '\n'.join(payload for kind, payload in added.get(f, []) if kind == 'LINE')
                  for f in files}
    scan_manifests(root, False, changed, problems, added_text)
    return problems


def check_diff(path: str, root: Path) -> list:
    text = sys.stdin.read() if path == '-' else (root / path).read_text(encoding='utf-8',
                                                                        errors='replace')
    return check_diff_text(text, root)


def self_test() -> int:
    import os
    import tempfile

    cases = 0
    passed = 0

    def check(name, fn, expect: bool):
        nonlocal cases, passed
        cases += 1
        got = bool(fn())
        ok = (got == expect)
        print(f"  {'ok  ' if ok else 'FAIL'} {name}")
        if not ok:
            print(f'       expected {"a finding" if expect else "clean"}, got '
                  f'{"findings: " + "; ".join(fn()) if got else "clean"}')
        passed += 1 if ok else 0

    with tempfile.TemporaryDirectory() as tmp:
        t = Path(tmp)
        (t / 'tools').mkdir()
        clean = t / 'payload.cc'

        def write(p, text, mode=None):
            p.write_text(text, encoding='utf-8')
            if mode is not None:
                os.chmod(p, mode)

        # D1: exec bit outside the allowlist, and the allowlist case that must stay silent
        write(clean, 'int main() {}\n', 0o755)
        check('D1 finds an executable outside tools/',
              lambda: [x for x in check_tree(t) if 'D1' in x], True)
        (t / 'tools' / 'x.sh').write_text('#!/bin/sh\n', encoding='utf-8')
        os.chmod(t / 'tools' / 'x.sh', 0o755)
        write(clean, 'int main() {}\n', 0o644)
        check('D1 does not fire on tools/*.sh nor on a plain file',
              lambda: [x for x in check_tree(t) if 'D1' in x], False)

        # D2: symlink
        (t / 'link').symlink_to('payload.cc')
        check('D2 finds a symlink', lambda: [x for x in check_tree(t) if 'D2' in x], True)
        (t / 'link').unlink()

        # D3: bidi override inside an added line, invisible in a diff viewer
        d3 = ('diff --git a/ok.py b/ok.py\n--- a/ok.py\n+++ b/ok.py\n'
              '+value = \u202enope\u202c\n')
        (t / 'p3.diff').write_text(d3, encoding='utf-8')
        check('D3 finds a bidi override in an added line',
              lambda: [x for x in check_diff('p3.diff', t) if 'D3' in x], True)

        # D4: Cyrillic U+0435 where a reader sees ASCII 'e'
        d4 = ('diff --git a/e.cc b/e.cc\n--- a/e.cc\n+++ b/e.cc\n'
              '+\u043eligible = 1;\n')
        (t / 'p4.diff').write_text(d4, encoding='utf-8')
        check('D4 finds a confusable in an added line',
              lambda: [x for x in check_diff('p4.diff', t) if 'D4' in x], True)
        d4b = ('diff --git a/e.cc b/e.cc\n--- a/e.cc\n+++ b/e.cc\n'
               '+eligible = 1;\n')
        (t / 'p4b.diff').write_text(d4b, encoding='utf-8')
        check('D4 stays silent on plain ASCII',
              lambda: [x for x in check_diff('p4b.diff', t) if 'D4' in x], False)

        # D5 and D6: workflow file with pull_request_target and a mutable action ref
        wf = t / '.github' / 'workflows'
        wf.mkdir(parents=True)
        (wf / 'bad.yml').write_text(
            'on:\n  pull_request_target:\npermissions:\n  contents: read\njobs:\n  a:\n'
            '    steps:\n      - uses: actions/checkout@v4\n      - run: echo ${{ secrets.T }}\n',
            encoding='utf-8')
        probs = []
        scan_workflows(t, probs)
        check('D5 finds pull_request_target', lambda: [x for x in probs if 'D5' in x], True)
        check('D6 finds an unpinned action ref', lambda: [x for x in probs if 'D6' in x], True)
        (wf / 'bad.yml').write_text(
            'on:\n  pull_request:\npermissions:\n  contents: read\njobs:\n  a:\n    steps:\n'
            '      - uses: actions/checkout@' + 'a' * 40 + '\n', encoding='utf-8')
        probs2 = []
        scan_workflows(t, probs2)
        check('D5 and D6 are silent on a pull_request job with a SHA-pinned action',
              lambda: [x for x in probs2 if 'D5' in x or 'D6' in x], False)

        # D7: one edit from a name the project really uses
        (t / 'conanfile.py').write_text('requires = ["harfruzz/8.3.0"]\n', encoding='utf-8')
        probs3 = []
        scan_manifests(t, False, {'conanfile.py'}, probs3)
        check('D7 finds a name one edit from a seed dependency',
              lambda: [x for x in probs3 if 'D7' in x], True)
        (t / 'conanfile.py').write_text('requires = ["harfbuzz/8.3.0"]\n', encoding='utf-8')
        probs4 = []
        scan_manifests(t, False, {'conanfile.py'}, probs4)
        check('D7 is silent on the correct name', lambda: [x for x in probs4 if 'D7' in x], False)

        # D7 and D8 from the patch alone: the review checkout has not applied it yet
        dpatch = ('diff --git a/conanfile.py b/conanfile.py\n'
                  '--- a/conanfile.py\n+++ b/conanfile.py\n@@ -1 +1 @@\n'
                  '-requires = ["harfbuzz/8.3.0"]\n+requires = ["harfruzz/8.3.0"]\n')
        bare = t / 'bare'          # a review checkout that has not applied the patch yet
        bare.mkdir()
        (bare / 'p7.diff').write_text(dpatch, encoding='utf-8')
        probs_p = check_diff('p7.diff', bare)
        check('D7 reads a manifest change from the patch, not the tree',
              lambda: [x for x in probs_p if 'D7' in x], True)
        check('D8 finds a manifest changed with no lockfile changed',
              lambda: [x for x in probs_p if 'D8' in x], True)

        # D8: a manifest with requirements and no lockfile, and a hand-written revision length
        probs5 = []
        scan_manifests(t, True, set(), probs5)
        check('D8 finds a manifest whose lockfile is absent',
              lambda: [x for x in probs5 if 'D8' in x], True)
        (t / 'conan.lock').write_text('{"requires": ["harfbuzz/8.3.0#c0ffee"]}\n',
                                      encoding='utf-8')
        probs6 = []
        scan_manifests(t, True, set(), probs6)
        check('D8 finds a revision string that is not a resolver revision',
              lambda: [x for x in probs6 if 'D8' in x], True)
        (t / 'conan.lock').write_text('{"version": "0.1", "requires": '
                                      '["harfbuzz/8.3.0#' + 'b' * 40 + '"], '
                                      '"build_requires": [], "python_requires": []}\n',
                                      encoding='utf-8')
        probs7 = []
        scan_manifests(t, True, set(), probs7)
        check('D8 is silent on a lock with a 40-hex revision',
              lambda: [x for x in probs7 if 'D8' in x], False)

    with tempfile.TemporaryDirectory() as tmp2:  # a directory with no .git in it
        cases += 1
        rc = main(['--staged', '--root', tmp2])
        ok = rc == 3
        print(f"  {'ok  ' if ok else 'FAIL'} --staged refuses outside a git checkout (rc=3)")
        passed += 1 if ok else 0

    print(f'diff-scan self-test: {passed}/{cases} properties hold')
    return 0 if passed == cases else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog='tools/diff-scan.py',
                                 description='D1-D8 diff and tree hygiene scan')
    ap.add_argument('--tree', action='store_true', help='scan the working tree')
    ap.add_argument('--diff', metavar='PATH', help='scan a unified diff (use - for stdin)')
    ap.add_argument('--root', default='.', help='repository root (default: current directory)')
    ap.add_argument('--staged', action='store_true',
                    help='scan what is in the git index (used by .githooks/pre-commit)')
    ap.add_argument('--self-test', action='store_true', help='prove every rule can still fail')
    ap.add_argument('--list-rules', action='store_true')
    a = ap.parse_args(argv)
    if a.list_rules:
        for line in (__doc__ or '').split('\n'):
            if re.match(r'^  D\d', line):
                print(line.strip())
        return 0
    if a.self_test:
        return self_test()
    root = Path(a.root).resolve()
    if a.staged:
        text = staged_diff(root)
        if text is None:
            print('diff-scan: NOT A PASS - --staged needs a git checkout, and this is not one; '
                  '--tree is the mode that works anywhere', file=sys.stderr)
            return 3
        if not text.strip():
            print('diff-scan: nothing staged, so the change rules had nothing to read (the CI '
                  'gate runs --tree over the whole checkout, which is the one that cannot be '
                  'skipped)')
            return 0
        problems = check_diff_text(text, root)
        for pr in problems:
            print(f'diff-scan: {pr}', file=sys.stderr)
        if problems:
            print(f'diff-scan: {len(problems)} finding(s) in the staged change', file=sys.stderr)
            return 1
        print('diff-scan: OK (staged diff, D1-D8 quiet)')
        return 0
    if not a.diff and not a.tree:
        print('diff-scan: NOT A PASS - give --diff PATCH or --tree; scanning nothing is not a '
              'clean result', file=sys.stderr)
        return 3
    try:
        problems = check_diff(a.diff, root) if a.diff else check_tree(root)
    except FileNotFoundError as exc:
        print(f'diff-scan: NOT A PASS - {exc}', file=sys.stderr)
        return 3
    for p in problems:
        print(f'diff-scan: {p}', file=sys.stderr)
    if problems:
        print(f'diff-scan: {len(problems)} finding(s) across D1-D8', file=sys.stderr)
        return 1
    scope = f'diff {a.diff}' if a.diff else f'tree {root.name}'
    print(f'diff-scan: OK ({scope}, D1-D8 quiet)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
