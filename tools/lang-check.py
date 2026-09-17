#!/usr/bin/env python3
"""ADR-0005 enforcement: the repository is written in English.

Modes
  lang-check.py [path ...]          check the given paths (default: whole tree)
  lang-check.py --self-test         prove that the check still catches violations
  lang-check.py --list-allow        print the exemption markers in use

Rules
  R1  Every tracked text file must be ASCII, except for a small allowlist of
      typographic characters used in prose (see ALLOWED_UNICODE).
  R2  Source files must be pure ASCII with no exceptions (no typographic marks).
  R3  No token from the Portuguese blocklist may appear outside ALLOW_PATHS.
  R4  An exemption marker must carry a non-empty reason.
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SOURCE_SUFFIXES = {
    ".h", ".hpp", ".hh", ".inl", ".c", ".cc", ".cpp", ".cxx",
    ".py", ".sh", ".cmake", ".toml", ".yml", ".yaml", ".json", ".txt",
}
SOURCE_NAMES = {"CMakeLists.txt", "conanfile.py", "meson.build"}
TEXT_SUFFIXES = SOURCE_SUFFIXES | {".md", ".adoc", ".in", ".def", ".rc", ".csv", ".ini"}

# Content that is the subject under test may legitimately be Portuguese.
ALLOW_PATHS = (
    "tests/conformance/",
    "tests/fixtures/",
    "tests/approvals/",
    "third_party/",
)

# Prose may use typographic characters; accented letters are deliberately absent,
# which is what makes R1 catch stray Portuguese.
ALLOWED_UNICODE = set(
    "\u2014\u2013\u2192\u2190\u2191\u2193\u2264\u2265\u00b1\u00b0\u00d7"
    "\u00b7\u2019\u201c\u201d\u2022\u00a7\u2713\u2717\u2026"
)

# The scanner's own token table necessarily contains the words it looks for, so the
# file is exempt from R3 only. R1 and R2 still apply to it.
SELF_EXEMPT_PT = {"tools/lang-check.py"}

# Low-noise blocklist: words that are Portuguese and not English.
PT_TOKENS = (
    "nao", "voce", "tambem", "assim", "entao", "quando", "onde", "feito",
    "dando", "usuario", "codigo", "linguagem", "documento", "arquivo",
    "projeto", "requisito", "decisao", "conexao", "posso", "preciso",
    "ainda", "assunto", "porque", "pois", "toda", "todos", "assim",
)
PT_RE = re.compile(r"(?<![A-Za-z0-9_])(" + "|".join(PT_TOKENS) + r")(?![A-Za-z0-9_])", re.IGNORECASE)

MARKER_PREFIX = "lang-check" + ":allow"  # assembled so this file is not its own match
ALLOW_LINE = re.compile(re.escape(MARKER_PREFIX) + r"\s+reason=(\S.*)")
ALLOW_FILE = re.compile(re.escape(MARKER_PREFIX) + r"-file\s+reason=(\S.*)")


def is_source(rel: Path) -> bool:
    return rel.suffix in SOURCE_SUFFIXES or rel.name in SOURCE_NAMES


def is_allowed(rel: Path) -> bool:
    p = rel.as_posix()
    return p.startswith(ALLOW_PATHS)


def tracked_files(root: Path) -> list[Path]:
    skip = {".git", "build", "out", "node_modules", "dist"}
    out: list[Path] = []
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        rel = path.relative_to(root) if path.is_relative_to(root) else Path("..") / path.name
        if any(part in skip or part.startswith(".venv") for part in rel.parts):
            continue
        if rel.suffix not in TEXT_SUFFIXES:
            continue
        out.append(path)
    return out


def check_file(path: Path, root: Path) -> list[str]:
    rel = path.relative_to(root) if path.is_relative_to(root) else Path("..") / path.name
    try:
        raw = path.read_bytes()
    except OSError as exc:  # unreadable file is a hard failure
        return [f"{rel}: cannot read ({exc})"]
    if b"\0" in raw[:4096]:
        return []  # binary
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        return [f"{rel}: not valid UTF-8"]

    problems: list[str] = []
    lines = text.splitlines()
    file_marker = any(ALLOW_FILE.search(l) for l in lines)
    if file_marker and not ALLOW_FILE.search(text):
        problems.append(f"{rel}:1: allow-file marker without reason=")

    for lineno, line in enumerate(lines, 1):
        if ALLOW_LINE.search(line) or file_marker:
            continue
        non_ascii = sorted({ch for ch in line if ord(ch) > 127})
        if non_ascii:
            bad = [ch for ch in non_ascii if ch not in ALLOWED_UNICODE]
            if bad:
                where = "source file (ASCII only)" if is_source(rel) else "char outside allowlist"
                shown = " ".join(f"U+{ord(c):04X}" for c in bad[:6])
                problems.append(f"{rel}:{lineno}: non-ASCII {where}: {shown}")
        if not is_allowed(rel) and rel.as_posix() not in SELF_EXEMPT_PT and PT_RE.search(line):
            word = PT_RE.search(line).group(1).lower()
            problems.append(f"{rel}:{lineno}: Portuguese token '{word}' (ADR-0005)")

    for lineno, line in enumerate(lines, 1):
        for m in re.finditer(re.escape(MARKER_PREFIX) + r"(-file)?\b(?!.*reason=\S)", line):
            problems.append(f"{rel}:{lineno}: exemption marker at {m.start()} lacks reason=")
    return problems


def self_test(root: Path) -> int:
    """Verify the gate has teeth: each rule must fail on a crafted file."""
    cases = {
        "R1 stray accented prose": ("docs/x.md", "Resumo da sess\u00e3o\n"),
        "R2 non-ASCII in source": ("src/x.cpp", "int main() { return 0; } // caf\u00e9\n"),
        "R3 Portuguese token": ("src/y.cpp", "// codigo de erro\nint f(void);\n"),
        "R3 Portuguese in prose": ("README.md", "Este projeto esta em portugues\n"),
        "R4 marker without reason": ("docs/z.md", "text  # " + MARKER_PREFIX + "\n"),
    }
    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        sandbox = Path(tmp)
        for name, (rel, content) in cases.items():
            target = sandbox / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(content, encoding="utf-8")
            caught = check_file(target, sandbox)
            status = "detected" if caught else "MISSED"
            if not caught:
                failures += 1
            print(f"  self-test {name}: {status}")
    print(f"self-test: {len(cases) - failures}/{len(cases)} rules effective")
    return 1 if failures else 0


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("paths", nargs="*", type=Path)
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--list-allow", action="store_true")
    args = ap.parse_args(argv)

    if args.self_test:
        return self_test(ROOT)

    files = [p.resolve() for p in args.paths] or tracked_files(ROOT)
    if args.list_allow:
        for f in files:
            if not f.is_file():
                continue
            for lineno, line in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                if MARKER_PREFIX in line:
                    print(f"{f.relative_to(ROOT)}:{lineno}: {line.strip()}")
        return 0

    problems: list[str] = []
    checked = 0
    for f in files:
        if f.is_file():
            checked += 1
            problems.extend(check_file(f, ROOT))
    for p in problems:
        print(p)
    scope = f"{checked} files"
    if problems:
        print(f"lang-check: FAILED ({len(problems)} problems in {scope})")
        return 1
    print(f"lang-check: OK ({scope})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
