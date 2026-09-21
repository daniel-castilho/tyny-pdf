#!/usr/bin/env python3
"""Generate tests/fixtures/long160.pdf - a deterministic 160-page PDF.

Each page carries a content stream that draws a background fill, a filled square whose
size varies with the page index, and a diagonal line, so every page renders to distinct,
non-uniform pixels. Used by the R-M2 threaded contract to give rendering enough work per
document to expose cross-context lock contention.

Usage: python3 tools/gen-long160.py [output-path]
"""

import sys

PAGES = 160
W = 595
H = 842


def content_stream(page: int) -> bytes:
    r = 40 + (page % 7) * 18
    x = 60 + (page % 9) * 20
    y = 100 + (page % 5) * 24
    ops = (
        "q\n"
        "0.898 0.898 0.898 rg\n"
        f"0 0 {W} {H} re f\n"
        "Q\n"
        "q\n"
        "0.098 0.196 0.882 rg\n"
        f"{x} {y} {r} {r} re f\n"
        "Q\n"
        "q\n"
        "0.9 0.2 0.1 RG 6 w\n"
        f"0 0 {W} {H} m\n"
        f"{W} {H} l S\n"
        "Q\n"
        f"BT /F1 {12 + (page % 5)} Tf 30 800 Td (page {page + 1}) Tj ET\n"
    )
    return ops.encode("ascii")


def main() -> None:
    path = sys.argv[1] if len(sys.argv) > 1 else "tests/fixtures/long160.pdf"

    # First pass: record obj offsets
    header = b"%PDF-1.4\n"
    chunks = []
    offsets = {}
    pos = len(header)

    def obj(number: int, body: bytes) -> None:
        nonlocal pos
        offsets[number] = pos
        chunk = f"{number} 0 obj\n".encode("ascii") + body + b"\nendobj\n"
        chunks.append(chunk)
        pos += len(chunk)

    kids = " ".join(f"{3 + i} 0 R" for i in range(PAGES))
    obj(1, "<< /Type /Catalog /Pages 2 0 R >>".encode("ascii"))
    obj(2, f"<< /Type /Pages /Kids [{kids}] /Count {PAGES} >>".encode("ascii"))

    first_content = 3 + PAGES
    font_obj = first_content + PAGES
    for i in range(PAGES):
        page_body = (
            f"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 {W} {H}] "
            f"/Contents {first_content + i} 0 R /Resources "
            f"<< /ProcSet [/PDF /Text] /Font << /F1 {font_obj} 0 R >> >> >>"
        ).encode("ascii")
        obj(3 + i, page_body)

    obj(
        font_obj,
        ("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica "
         "/Encoding /WinAnsiEncoding >>").encode("ascii"),
    )

    for i in range(PAGES):
        stream = content_stream(i)
        body = f"<< /Length {len(stream)} >>\nstream\n".encode("ascii") + stream + b"\nendstream"
        obj(first_content + i, body)

    max_obj = font_obj
    body = bytearray(header)
    for c in chunks:
        body += c
    xref_at = len(body)
    body += b"xref\n"
    body += f"0 {max_obj + 1}\n".encode("ascii")
    body += b"0000000000 65535 f \n"
    for n in range(1, max_obj + 1):
        body += f"{offsets.get(n, 0):010d} 00000 n \n".encode("ascii")
    body += f"trailer\n<< /Size {max_obj + 1} /Root 1 0 R >>\nstartxref\n{xref_at}\n%%EOF\n".encode(
        "ascii"
    )

    with open(path, "wb") as f:
        f.write(bytes(body))
    print(f"wrote {path} ({len(body)} bytes, {PAGES} pages)")


if __name__ == "__main__":
    main()
