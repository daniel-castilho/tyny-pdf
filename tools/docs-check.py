#!/usr/bin/env python3
"""Documentation gate: a document may only promise what exists.

Checks, in order:
1. Markdown table rows have the same number of cells as their header row.
2. Relative links resolve, and every adr/*.md is listed in README.md exactly once.
3. A reference to `tools/<file>` must resolve, unless the same line says who writes it: a marker of
   "(planned", "PR #<n>", "new in this PR" or "in progress". An unmarked forward reference is how a
   document becomes permanently wrong (docs/lessons.md). A
   forward reference without a marker is how a document becomes permanently wrong (docs/lessons.md).
4. Prose lines stay within the .editorconfig limit for Markdown (100 columns).
5. An ADR header block carries Status, Date and Related, and a Status of Accepted names who
   ratified it.

Usage: python3 tools/docs-check.py [--root DIR] [--self-test]
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
MD_LIMIT = 100
LINK_RE = re.compile(r"\]\(([^)#\s]+)(?:#[^)]*)?\)")
TOOL_RE = re.compile(r"`tools/([A-Za-z0-9._/-]+\.(?:py|sh|cmd|bat))`")
PLANNED_RE = re.compile(r"\(planned[,)]|planned, PR|in progress|PR #\d+|new in this PR",
                         re.I)
REQ_HEADING_RE = re.compile(r"^#{2,4}\s+R\d+\.\d+\s")
ADR_RE = re.compile(r"^adr/(\d{4})-[a-z0-9-]+\.md$")
HEADER_KEYS = ("- Status:", "- Date:", "- Related:")
SELF_CASES = []


def self_case(name):
    def wrap(fn):
        SELF_CASES.append((name, fn))
        return fn
    return wrap


def split_row(line: str):
    return line.replace("\\|", "\0").split("|")


def paragraph_at(lines, index):
    """The blank-line-delimited block containing lines[index]: a marker on any line of the
    paragraph covers the references in it, which is how a list item is actually written."""
    start = index
    while start > 0 and lines[start - 1].strip():
        start -= 1
    end = index
    while end + 1 < len(lines) and lines[end + 1].strip():
        end += 1
    return "\n".join(lines[start:end + 1])


def check_markdown(path: Path, text: str):
    problems = []
    lines = text.splitlines()
    table = None
    fenced = False
    fence_open = 0
    for i, line in enumerate(lines, 1):
        if line.lstrip().startswith("```"):
            fenced = not fenced
            fence_open = i if fenced else 0
            table = None
            continue
        if fenced:
            continue
        # A requirement statement is one line by contract (REQ_RE), so it is not a wrap candidate.
        if REQ_HEADING_RE.match(line):
            table = None
            continue
        if line.lstrip().startswith("|"):
            cells = len(split_row(line)) - 2
            if table is None:
                table = (cells, i)
            elif re.fullmatch(r"[|\s:-]+", line):
                continue
            elif cells != table[0]:
                problems.append("{}:{}: table row has {} cells, header had {}".format(
                    path.name, i, cells, table[0]))
        else:
            table = None
            if len(line) > MD_LIMIT and "http" not in line and not line.lstrip().startswith("|"):
                problems.append("{}:{}: line is {} chars (> {})".format(
                    path.name, i, len(line), MD_LIMIT))
            for ref in LINK_RE.findall(line):
                if ref.startswith(("http", "mailto:", "#")):
                    continue
                if not (path.parent / ref).resolve().exists():
                    problems.append("{}:{}: broken link {}".format(path.name, i, ref))
            for m in TOOL_RE.finditer(line):
                if not (ROOT / "tools" / m.group(1)).exists() \
                        and not PLANNED_RE.search(paragraph_at(lines, i - 1)):
                    problems.append("{}:{}: `tools/{}` does not exist and is not marked planned"
                                    .format(path.name, i, m.group(1)))
    if fenced:
        # An unbalanced fence silently hides the tail of the file from every rule
        # below, which is how a mangled code block once passed as documentation.
        problems.append("{}:{}: unclosed code fence (the tail of the file would escape every other rule here)".format(path.name, fence_open))
    return problems


def check_index(root: Path, text: str):
    problems = []
    listed = set(re.findall(r"\(?(adr/\d{4}-[a-z0-9-]+\.md)\)?", text))
    present = {"adr/" + p.name for p in sorted((root / "adr").glob("*.md"))}
    for missing in sorted(present - listed):
        problems.append("README.md: {} is not listed in the ADR table".format(missing))
    for ghost in sorted(listed - present):
        problems.append("README.md: lists {} which does not exist".format(ghost))
    for adr in sorted(present):
        body = (root / adr).read_text(encoding="utf-8")
        for key in HEADER_KEYS:
            if key not in body:
                problems.append("{}: missing header line '{}'".format(adr, key))
        status = re.search(r"^- Status:\s*(.+)$", body, re.M)
        if status and status.group(1).lower().startswith("accepted") \
                and not re.search(r"\d{4}-\d{2}-\d{2}", status.group(1)):
            problems.append("{}: Accepted without a ratification date".format(adr))
    return problems


def check(root: Path = ROOT):
    problems = []
    def is_excluded(path: Path) -> bool:
        return "third_party" in path.parts
    for path in sorted(root.rglob("*.md")):
        if ".git/" in str(path) or is_excluded(path):
            continue
        problems += check_markdown(path, path.read_text(encoding="utf-8"))
    readme = root / "README.md"
    if readme.exists():
        problems += check_index(root, readme.read_text(encoding="utf-8"))
    for path in sorted(root.glob("src/**/SPEC.md")):
        body = path.read_text(encoding="utf-8")
        if "Out of scope" not in body:
            problems.append("{}: no Out of scope section".format(path))
    return problems


@self_case("clean tree passes")
def _t_clean(root):
    return [] == check(root)


@self_case("ragged table detected")
def _t_table(root):
    (root / "x.md").write_text("# T\n\n| a | b |\n| - | - |\n| 1 |\n\n", encoding="utf-8")
    return any("table row" in p for p in check(root))


@self_case("broken link detected")
def _t_link(root):
    (root / "x.md").write_text("see [adr](adr/nope.md)\n", encoding="utf-8")
    return any("broken link" in p for p in check(root))


@self_case("unmarked missing tool detected")
def _t_tool(root):
    (root / "x.md").write_text("run `tools/ghost.sh` now\n", encoding="utf-8")
    return any("does not exist" in p for p in check(root))


@self_case("unclosed fence detected")
def _t_fence_open(root):
    (root / "x.md").write_text("prose\n\n```\ncode\n```\n\n```\nnever closed\n", encoding="utf-8")
    if not any("unclosed code fence" in p for p in check(root)):
        return False
    (root / "x.md").write_text("prose\n\n```\ncode\n```\n\n- item\n", encoding="utf-8")
    return not any("unclosed code fence" in p for p in check(root))


@self_case("fence inside a list item still toggles")
def _t_fence_indent(root):
    text = "- **cmd:**\n  ```bash\n" + "x " * 40 + "\n  ```\n"
    (root / "x.md").write_text(text, encoding="utf-8")
    return not any("unclosed" in p or "line is" in p for p in check(root))


@self_case("planned tool reference allowed")
def _t_planned(root):
    (root / "x.md").write_text("run `tools/ghost.sh` (planned, PR #4)\n", encoding="utf-8")
    return not any("does not exist" in p for p in check(root))


@self_case("self-authored forward reference allowed")
def _t_new(root):
    (root / "x.md").write_text("this PR adds `tools/ghost.sh` (new in this PR)\n", encoding="utf-8")
    return not any("does not exist" in p for p in check(root))


@self_case("marker anywhere in the paragraph covers the paragraph")
def _t_paragraph(root):
    (root / "x.md").write_text("3. `chore/x` - new in this PR: `tools/ghost.sh`,\n"
                              "   `tools/other.sh`, and notes.\n", encoding="utf-8")
    hits = [p for p in check(root) if "does not exist" in p]
    return not hits


@self_case("long line detected")
def _t_long(root):
    (root / "x.md").write_text("x" * 120 + "\n", encoding="utf-8")
    return any("line is" in p for p in check(root))


@self_case("ADR missing from index detected")
def _t_index(root):
    (root / "adr").mkdir(exist_ok=True)
    (root / "adr" / "9999-orphan.md").write_text("# Orphan\n\n- Status: Accepted 2026-01-01\n"
                                                 "- Date: 2026-01-01\n- Related: none\n\n"
                                                 "text\n", encoding="utf-8")
    return any("not listed" in p for p in check(root))


def self_test() -> int:
    import tempfile
    failures = 0
    for name, fn in SELF_CASES:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "README.md").write_text("# t\n\n", encoding="utf-8")
            (root / "x.md").write_text("# x\n\n", encoding="utf-8")
            ok = fn(root)
            print("  {} {}".format("ok  " if ok else "FAIL", name))
            failures += 0 if ok else 1
    print("docs-check self-test: {}/{} properties hold".format(len(SELF_CASES) - failures,
                                                                len(SELF_CASES)))
    return 1 if failures else 0


def main(argv) -> int:
    if "--self-test" in argv:
        return self_test()
    root = ROOT
    if "--root" in argv:
        root = Path(argv[argv.index("--root") + 1])
    problems = [] if root != ROOT else check(root)
    if root != ROOT:
        problems = check(root)
    for p in problems:
        print("docs-check: " + p, file=sys.stderr)
    if problems:
        return 1
    n = len(list(root.rglob("*.md")))
    print("docs-check: OK ({} markdown files, 0 problems)".format(n))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
