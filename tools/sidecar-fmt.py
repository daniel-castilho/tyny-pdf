#!/usr/bin/env python3
"""Canonical form and validation for the Tyny PDF annotation sidecar (ADR-0007).

Enforces requirements R1.1, R1.2, R1.3, R2.1 of src/features/sidecar/SPEC.md.

The sidecar is a user-visible file that must diff well, so its byte layout is part of the
format, not an accident of the serializer. This tool owns that canonical form.

  sidecar-fmt.py check FILE...     fail if a file is not canonical or not valid
  sidecar-fmt.py fix FILE...       rewrite files in canonical form
  sidecar-fmt.py self-test         prove the canonicalizer is deterministic

Guarantees checked here:
  - UTF-8, LF line endings, no BOM, exactly one trailing newline
  - object keys sorted lexicographically, two-space indent
  - annotations sorted by (page, -y1, x0, id) so reading order is stable
  - replies sorted by (created, id)
  - floats printed with the shortest round-trip form, never in scientific notation
  - integers stay integers (612, not 612.0)
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCHEMA_PATH = HERE.parent / "docs" / "sidecar.schema.json"

FLOAT_MAX_PLAIN = 1e12
FLOAT_MIN_PLAIN = 1e-6
ID_RE = re.compile(r"^[a-z2-7]{10}$")


class SidecarError(Exception):
    pass


# ---------------------------------------------------------------- canonical text

def fmt_number(value):
    if isinstance(value, bool):
        raise SidecarError("booleans must not be numbers")
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        if value != value or value in (float("inf"), float("-inf")):
            raise SidecarError("NaN and infinity are not representable")
        if value == int(value) and abs(value) < 1e15:
            return str(int(value))
        if abs(value) >= FLOAT_MAX_PLAIN or (value != 0 and abs(value) < FLOAT_MIN_PLAIN):
            raise SidecarError(
                "float outside plain notation: values must be rounded (rect uses 3 decimals)"
            )
        text = repr(value)
        if "." not in text and "e" not in text:
            text += ".0"
        return text
    raise SidecarError(f"not a number: {value!r}")


def dump(value, indent=0):
    pad, pad2 = "  " * indent, "  " * (indent + 1)
    if value is None:
        return "null"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (int, float)):
        return fmt_number(value)
    if isinstance(value, str):
        return json.dumps(value, ensure_ascii=False)
    if isinstance(value, list):
        if not value:
            return "[]"
        rows = ",\n".join(pad2 + dump(v, indent + 1) for v in value)
        return "[\n" + rows + "\n" + pad + "]"
    if isinstance(value, dict):
        if not value:
            return "{}"
        rows = ",\n".join(
            f'{pad2}{json.dumps(k, ensure_ascii=False)}: {dump(v, indent + 1)}'
            for k, v in sorted(value.items())
        )
        return "{\n" + rows + "\n" + pad + "}"
    raise SidecarError(f"unsupported type {type(value).__name__}")


# ----------------------------------------------------------------- normalisation

def annotation_sort_key(a):
    rect = a.get("rect") or [0, 0, 0, 0]
    return (a.get("page", 0), -float(rect[3]), float(rect[0]), a.get("id", ""))


def normalize(doc):
    if not isinstance(doc, dict):
        raise SidecarError("top level must be an object")
    anns = doc.get("annotations")
    if isinstance(anns, list):
        doc["annotations"] = sorted(anns, key=annotation_sort_key)
        for a in doc["annotations"]:
            if isinstance(a, dict):
                replies = a.get("replies")
                if isinstance(replies, list):
                    a["replies"] = sorted(replies, key=lambda r: (r.get("created", ""), r.get("id", "")))
    return doc


def round_numbers(value, depth=0):
    """Round geometry to 3 decimals so repeated saves do not drift in the low bits."""
    if isinstance(value, float):
        return round(value, 3)
    if isinstance(value, list):
        return [round_numbers(v, depth + 1) for v in value]
    if isinstance(value, dict):
        return {k: round_numbers(v, depth + 1) for k, v in value.items()}
    return value


def canonical_bytes(obj):
    return (dump(normalize(round_numbers(obj))) + "\n").encode("utf-8")


def load(path):
    raw = path.read_bytes()
    if raw.startswith(b"\xef\xbb\xbf"):
        raise SidecarError("BOM is not allowed")
    if b"\r" in raw:
        raise SidecarError("CR found: line endings must be LF")
    text = raw.decode("utf-8")  # a bad decode is a hard error, by design
    try:
        return json.loads(text)
    except json.JSONDecodeError as exc:
        raise SidecarError(f"invalid JSON: {exc}") from exc


# -------------------------------------------------------------------- validation

def schema_errors(doc):
    try:
        import jsonschema
    except ImportError:
        return ["jsonschema is not installed: pip install jsonschema"]
    schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
    validator = jsonschema.Draft202012Validator(schema)
    out = []
    for err in sorted(validator.iter_errors(doc), key=lambda e: list(e.absolute_path)):
        where = "/".join(str(p) for p in err.absolute_path) or "(root)"
        out.append(f"{where}: {err.message}")
    return out


def structure_errors(doc):
    """Rules the schema cannot express."""
    out = []
    seen = set()
    ids = {a.get("id") for a in doc.get("annotations", []) if isinstance(a, dict)}
    for i, a in enumerate(doc.get("annotations", [])):
        aid = a.get("id", "")
        if not ID_RE.match(aid or ""):
            out.append(f"annotations/{i}/id: must be 10 chars of RFC4648 base32, got {aid!r}")
        if aid in seen:
            out.append(f"annotations/{i}/id: duplicate id {aid}")
        seen.add(aid)
        if a.get("type") == "highlight" and not a.get("quads"):
            out.append(f"annotations/{i}: a highlight needs quads, not only a rect")
        rect = a.get("rect")
        if isinstance(rect, list) and len(rect) == 4:
            if rect[2] < rect[0] or rect[3] < rect[1]:
                out.append(f"annotations/{i}/rect: x1/y1 must be >= x0/y0")
        for r in a.get("replies", []) or []:
            target = r.get("in_reply_to")
            if target and target not in ids:
                out.append(f"annotations/{i}/replies: in_reply_to {target!r} is not a known id")
        anchor = a.get("anchor")
        if isinstance(anchor, dict) and anchor.get("quote") and not anchor.get("text_sha256"):
            out.append(
                f"annotations/{i}/anchor: a quote without text_sha256 cannot be reconciled"
                " deterministically"
            )
    return out


# ------------------------------------------------------------------------- CLI

def is_sidecar_name(path):
    # The suffix is defined once, in docs/naming.md: sidecar_suffix = ".tynypdf.json".
    return path.name.endswith(".tynypdf.json")


def check_file(path):
    problems = []
    try:
        doc = load(path)
    except (SidecarError, OSError) as exc:
        return [f"{path}: {exc}"]
    try:
        want = canonical_bytes(json.loads(json.dumps(doc)))
    except SidecarError as exc:
        return [f"{path}: {exc}"]
    if path.read_bytes() != want:
        problems.append(f"{path}: not canonical (run sidecar-fmt.py fix)")
    problems += [f"{path}: {m}" for m in schema_errors(doc) + structure_errors(doc)]
    return problems


def self_test():
    failures = []
    base = {
        "format_version": 1,
        "document": {"sha256": "0" * 64, "pages": 3, "filename": "doc.pdf"},
        "annotations": [
            {
                "id": "abc2d3e4f5",
                "type": "highlight",
                "page": 1,
                "rect": [72.0, 700.0, 300.0, 712.5],
                "quads": [[72.0, 712.5, 300.0, 712.5, 72.0, 700.0, 300.0, 700.0]],
                "color": "#f4d03f",
                "opacity": 0.4,
                "created": "2026-09-16T18:00:00Z",
                "contents": "note",
                "anchor": {"quote": "some text", "text_sha256": "a" * 64, "score": 0.999},
            },
            {"id": "aaaa2222bb", "type": "text_note", "page": 0, "rect": [10, 20, 30, 40]},
        ],
    }
    text = canonical_bytes(base).decode("utf-8")

    # 1. key and array order must not change the bytes
    shuffled = json.loads(text)
    shuffled["annotations"] = list(reversed(shuffled["annotations"]))
    for a in shuffled["annotations"]:
        items = list(a.items())
        a.clear()
        a.update(reversed(items))
    if canonical_bytes(shuffled).decode("utf-8") != text:
        failures.append("canonical form is not order-independent")

    # 2. no scientific notation, integers stay integers
    tiny = canonical_bytes({"format_version": 1, "annotations": [], "document": {"pages": 1, "sha256": "0" * 64, "x": 1e-7}}).decode()
    if "e-" in tiny:
        failures.append("scientific notation leaked into output")
    if "72.0," in text and '"72.0"' not in text:
        pass
    if "72," not in text:
        failures.append("integral float 72.0 should print as 72")

    # 3. idempotence
    if canonical_bytes(json.loads(text)).decode() != text:
        failures.append("canonical form is not idempotent")

    # 4. reading order applied: page then top-to-bottom
    anns = json.loads(text)["annotations"]
    if [a["id"] for a in anns] != ["aaaa2222bb", "abc2d3e4f5"]:
        failures.append(
            "annotations were not sorted by (page asc, -y1, x0, id); got "
            + ",".join(a["id"] for a in anns)
        )

    # 5. the shipped example must satisfy the schema
    example = HERE.parent / "tests" / "fixtures" / "sidecar" / "example.tynypdf.json"
    if example.exists():
        errs = schema_errors(load(example)) + structure_errors(load(example))
        if errs:
            failures.append("example fixture is invalid: " + "; ".join(errs[:2]))
    else:
        failures.append("example fixture missing")

    for f in failures:
        print(f"  FAIL {f}")
    print(f"self-test: {5 - len(failures)}/5 properties hold")
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(prog="sidecar-fmt.py", description=__doc__)
    ap.add_argument("mode", choices=["check", "fix", "self-test"])
    ap.add_argument("paths", nargs="*", type=Path)
    args = ap.parse_args(argv)

    if args.mode == "self-test":
        return self_test()
    if not args.paths:
        print("no files given", file=sys.stderr)
        return 2

    # A directory argument means "every sidecar-named file under it", so CI can pass the repo root.
    paths = []
    for arg in args.paths:
        if arg.is_dir():
            paths += sorted(q for q in arg.rglob("*") if q.is_file() and is_sidecar_name(q))
        else:
            paths.append(arg)

    bad = 0
    skipped = 0
    for p in paths:
        if args.mode == "check" and not is_sidecar_name(p):
            print(f"{p}: not a sidecar by name, skipped (see docs/naming.md)")
            skipped += 1
            continue
        if args.mode == "fix":
            out = canonical_bytes(load(p))
            p.write_bytes(out)
            print(f"{p}: rewritten ({len(out)} bytes)")
            continue
        problems = check_file(p)
        for line in problems:
            print(line)
        bad += len(problems)
    if args.mode == "check":
        print(
            f"sidecar-fmt: {'FAILED' if bad else 'OK'}"
            f" ({len(paths) - skipped} checked, {skipped} skipped, {bad} problems)"
        )
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
