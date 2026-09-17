#!/usr/bin/env python3
"""Traceability gate (ADR-0001 R-M13, ADR-0002, ADR-0009 rule 5).

Checks, all cheap enough to run on every commit:

1. Shape     every capability SPEC.md writes requirements as `### R<n>.<m> <text>` with an EARS
             keyword and a `Verification:` line, and has an `Out of scope` section.
2. Trace     an id cited in adr/**, docs/**, src/** or a test file must be defined in some SPEC.md;
             a duplicate id is an error.
3. Evidence  `Verification:` is one of `unit:<path>`, `golden:<path>`, `script:<path>`, `manual:<note>`,
             where `script:` points at a repo tool that enforces the property. While the
             capability directory has no source file the artefact may still be missing, and the id
             is reported as pending. Once the capability has code, a missing artefact or a
             manual-only requirement is an error, and the named test file must cite the id.
             "Done" is therefore a property of the build, not of a claim by a person or an agent.
4. Code gap  a source file under src/ whose capability directory has no SPEC.md is an error.

Usage: python3 tools/spec-check.py [--quick] [--root DIR] [--self-test]
  --quick skips step 2 (the tree-wide citation scan); used by the pre-commit hook.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
SPEC_NAME = "SPEC.md"
REQ_RE = re.compile(r"^###\s+R(\d+)\.(\d+)\s+(.+?)\s*$")
ID_RE = re.compile(r"\bR\d+\.\d+\b")
# EARS needs both halves: a binding context (When/While/Where/If, or none for a ubiquitous rule)
# and a modal verb. "The X emits Y" is a description, not a requirement, so "The" alone is not a
# keyword - a self-test proved this gate was too easy the first time it was written.
EARS_LEAD = re.compile(r"^(When|While|Where|If|The)\b")
EARS_MODAL = re.compile(r"\b(SHALL|MUST)\b")
NON_NORMATIVE = re.compile(r"\b(SHOULD|MAY|WILL|CAN|might|prefer)\b", re.I)
VERIF_RE = re.compile(r"^\s*(?:[-*]\s*)?[*]*Verification[*]*\s*:\s*(.+?)\s*$")
FIELD_RE = re.compile(r"^\s*[*_]*[A-Z][A-Za-z ]{2,24}[*_]*\s*:")
KIND_RE = re.compile(r"^(unit|golden|manual|script)\b[:\s]\s*(.*)$", re.I)
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hh"}


def find_specs(root: Path):
    return sorted(p for p in root.rglob(SPEC_NAME) if ".git/" not in str(p))


def source_files(root: Path):
    src = root / "src"
    if not src.is_dir():
        return []
    return sorted(p for p in src.rglob("*") if p.is_file() and p.suffix in SOURCE_SUFFIXES)


def capability_of(root: Path, path: Path) -> Path:
    """src/<group>/<capability>/... -> src/<group>/<capability>; otherwise the parent directory."""
    rel = path.relative_to(root)
    parts = rel.parts
    if len(parts) >= 4 and parts[0] == "src":
        return Path(*parts[:3])
    return rel.parent if rel.parent != Path(".") else Path(".")


def parse_spec(path: Path):
    reqs = []
    problems = []
    open_req = None
    in_requirements = False
    has_out_of_scope = False
    for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if line.startswith("## "):
            title = line[3:].strip().lower()
            in_requirements = "requirement" in title
            has_out_of_scope = has_out_of_scope or "out of scope" in title
            continue
        match = REQ_RE.match(line)
        if match:
            reqs.append({"id": "R{}.{}".format(match.group(1), match.group(2)),
                         "text": match.group(3), "line": lineno, "file": path,
                         "verification": None})
            open_req = reqs[-1]
            continue
        if open_req is not None:
            match = VERIF_RE.match(line)
            if match:
                if open_req["verification"] is None:
                    open_req["verification"] = match.group(1)
                    open_req = None
                    continue
                problems.append("{}:{}: a second Verification line for the previous requirement"
                                .format(path.name, lineno))
                continue
            if line.strip():
                if FIELD_RE.match(line):
                    problems.append("{}:{}: unknown field {!r} in a requirement block"
                                    .format(path.name, lineno, line.split(":")[0].strip(" -*")))
                else:  # a wrapped statement line
                    open_req["text"] = (open_req["text"] + " " + line.strip()).strip()
                continue
            continue  # a blank line does not close a requirement; Verification or a new heading do
        if in_requirements and line.strip() and not line.startswith(("#", ">", "|", "-")):
            problems.append("{}:{}: prose inside Requirements has no `### R<n>.<m>` id"
                            .format(path.name, lineno))
    if not has_out_of_scope:
        problems.append("{}: no 'Out of scope' section (a delta without a boundary is not "
                        "falsifiable)".format(path.name))
    for req in reqs:
        text = req["text"]
        if not EARS_MODAL.search(text):
            problems.append("{}:{} {}: no SHALL/MUST, so it is not a requirement".format(
                path.name, req["line"], req["id"]))
        elif not EARS_LEAD.match(text):
            problems.append("{}:{} {}: EARS wants a leading When/While/Where/If or 'The <system>'"
                            .format(path.name, req["line"], req["id"]))
        elif NON_NORMATIVE.search(text):
            problems.append("{}:{} {}: contains a non-normative verb; a requirement uses SHALL or "
                            "MUST only".format(path.name, req["line"], req["id"]))
        if not req["verification"]:
            problems.append("{}: {}: no Verification line".format(path.name, req["id"]))
    return reqs, problems


def check(root: Path, quick: bool = False):
    specs = find_specs(root)
    srcs = source_files(root)
    problems = []
    defined = {}
    for spec in specs:
        reqs, errs = parse_spec(spec)
        problems += errs
        for req in reqs:
            if req["id"] in defined:
                problems.append("{}: duplicate requirement id {}".format(spec.name, req["id"]))
            req["capability"] = spec.parent.relative_to(root)
            req["has_code"] = bool([p for p in srcs if p.parent == spec.parent
                                    or spec.parent in p.parents])
            defined[req["id"]] = req

    for req in defined.values():
        value = req["verification"]
        if not value:
            continue
        kind = KIND_RE.match(value)
        if not kind:
            problems.append("{}: {}: Verification must start with unit:, golden:, script: or manual:"
                            .format(req["file"].name, req["id"]))
            continue
        kind_name, target = kind.group(1).lower(), kind.group(2).strip().strip("`")
        req["kind"] = kind_name
        req["target"] = target
        if kind_name == "manual":
            req["pending"] = not req["has_code"]
            if req["has_code"]:
                problems.append("{}: {} is verified by hand only, and {} already contains code"
                                .format(req["file"].name, req["id"], req["capability"]))
            continue
        path = root / target
        if not path.exists():
            if req["has_code"] or kind_name == "script":
                problems.append("{}: {} names {} which does not exist".format(
                    req["file"].name, req["id"], target))
            else:
                req["pending"] = True
            continue
        req["pending"] = False
        cited = ID_RE.findall(path.read_text(encoding="utf-8", errors="replace"))
        if req["id"] not in cited:
            problems.append("{}: {} does not cite its requirement id (a test that is not traced is "
                            "not evidence)".format(req["file"].name, req["id"]))

    if not quick:
        cited = set()
        def is_excluded(path: Path) -> bool:
            return "third_party" in path.parts
        scan = list(root.glob("adr/**/*.md")) + list(root.glob("docs/**/*.md")) + \
            list(root.glob("src/**/*.md")) + [p for p in root.rglob("*")
                                              if p.is_file() and p.suffix in SOURCE_SUFFIXES
                                              and ".git/" not in str(p)
                                              and not is_excluded(p)]
        for path in scan:
            text = path.read_text(encoding="utf-8", errors="replace")
            if path.name == SPEC_NAME:
                text = "\n".join(l for l in text.splitlines() if not REQ_RE.match(l))
            cited.update(ID_RE.findall(text))
        for rid in sorted(cited - set(defined)):
            problems.append("{} is cited outside a SPEC.md but defined nowhere".format(rid))

    if srcs and not specs:
        problems.append("{} source file(s) under src/ and no {}: specs are not optional"
                        .format(len(srcs), SPEC_NAME))
    for src in srcs:
        cap = capability_of(root, src)
        if not (root / cap / SPEC_NAME).exists():
            problems.append("{}: {} has no {}".format(src.relative_to(root), cap, SPEC_NAME))
    pending = sorted(rid for rid, req in defined.items() if req.get("pending"))
    return defined, problems, len(specs), len(srcs), pending


def main(argv) -> int:
    if "--self-test" in argv:
        return self_test()
    root = ROOT
    if "--root" in argv:
        root = Path(argv[argv.index("--root") + 1])
    defined, problems, specs, srcs, pending = check(root, quick="--quick" in argv)
    for p in problems:
        print("spec-check: " + str(p), file=sys.stderr)
    if problems:
        return 1
    print("spec-check: OK ({} specs, {} requirements, {} source files, 0 orphans)".format(
        specs, len(defined), srcs))
    print("spec-check: {} requirement(s) still pending (no artefact yet): {}".format(
        len(pending), ", ".join(pending) if pending else "-"))
    return 0


GOOD = ("# Sidecar\n\n## Requirements\n\n"
        "### R1.1 The writer SHALL emit canonical bytes.\n\n"
        "Verification: unit:tests/unit/test_writer.cc\n\n"
        "## Out of scope\n\n- the binary wire format\n")


def self_test() -> int:
    """Each case builds a fresh tree, so state cannot leak between cases."""
    import tempfile

    def build(text=GOOD, with_code=True, with_target=True, cited=True, extra=None):
        root = Path(tempfile.mkdtemp())
        cap = root / "src" / "features" / "sidecar"
        cap.mkdir(parents=True)
        if with_code:
            (cap / "writer.cc").write_text("// R1.1\n", encoding="utf-8")
        if text is not None:
            (cap / SPEC_NAME).write_text(text, encoding="utf-8")
        target = root / "tests" / "unit" / "test_writer.cc"
        if with_target:
            target.parent.mkdir(parents=True, exist_ok=True)
            body = "R1.1" if cited else "nobody knows"
            target.write_text("TEST(sidecar, canonical) { /* " + body + " */ }\n", encoding="utf-8")
        if extra:
            path = root / extra[0]
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(extra[1], encoding="utf-8")
        return root

    dup = (GOOD.rsplit("## Out of scope", 1)[0]
           + "### R1.1 The writer SHALL repeat itself.\n"
           + "Verification: unit:tests/unit/test_writer.cc\n\n## Out of scope\n\n- y\n")
    cases = [
        ("code + traced test passes", build(), False),
        ("missing test artefact fails once code exists", build(with_target=False), True),
        ("test that does not cite its id fails", build(cited=False), True),
        ("planned requirement is pending, not an error", build(with_code=False, with_target=False), False),
        ("manual-only verification fails once code exists",
         build(GOOD.replace("unit:tests/unit/test_writer.cc", "manual: eyeballed once")), True),
        ("no modal verb fails", build(GOOD.replace("SHALL emit", "emits")), True),
        ("no EARS lead clause fails",
         build(GOOD.replace("### R1.1 The writer SHALL emit",
                            "### R1.1 Writing is atomic and the writer SHALL emit")), True),
        ("should is not a requirement", build(GOOD.replace("SHALL emit", "SHOULD emit")), True),
        ("wrapped statement is accepted",
         build(GOOD.replace("### R1.1 The writer SHALL emit canonical bytes.",
                            "### R1.1 The writer SHALL emit canonical\nbytes for any input.")), False),
        ("missing Out of scope fails", build(GOOD.split("## Out of scope")[0]), True),
        ("missing Verification line fails",
         build(GOOD.replace("Verification: unit:tests/unit/test_writer.cc\n", "")), True),
        ("duplicate requirement id fails", build(dup), True),
        ("id cited in an ADR but never defined fails",
         build(extra=("adr/0099-x.md", "covers R9.9\n")), True),
        ("source without a SPEC.md fails", build(text=None), True),
    ]

    failures = 0
    for name, root, want_problems in cases:
        problems = check(root)[1]
        ok = (len(problems) > 0) == want_problems
        print("  {} {:<52}{}".format("ok  " if ok else "FAIL", name,
                                     "" if ok else " -> " + str(problems)))
        failures += 0 if ok else 1
    print("spec-check self-test: {}/{} properties hold".format(len(cases) - failures, len(cases)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
