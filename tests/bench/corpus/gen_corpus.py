#!/usr/bin/env python3
"""Deterministic generator for the 1000-page bench corpus (story 1.5 content viewer).

Output: tests/bench/corpus/corpus-1000p.pdf

- Page 1 is 4000x3000 pt. At 72 dpi that is the 4000x3000 region R15.1
  (`src/features/render/SPEC.md`) blits — a 16x12 grid of 256 px tiles (192
  tiles) — but the strip the viewer shows holds three tiles, so the generator
  draws one distinct gray rect per 256-point tile to make tile misalignment
  visible in any crop.
- Pages 2-1000 are 612x792 pt and share one content stream so the file stays
  small.
- Byte-for-byte deterministic: no /CreationDate, no random sources, fixed
  object order, fixed xref — re-running this script reproduces the same sha256.

Stable digest for R30.2 (the corpus contract test pins it):
    sha256 = <filled by gen on first run; pinned in tests/unit/test_bench_corpus.cc>
"""

import hashlib
import sys
from pathlib import Path

PAGE_COUNT = 1000
BIG_W, BIG_H = 4000, 3000
SMALL_W, SMALL_H = 612, 792
TILE = 256
COLUMNS = (BIG_W + TILE - 1) // TILE  # 16 (last column partial)
ROWS = (BIG_H + TILE - 1) // TILE      # 12 (last row partial)


def big_page_stream() -> bytes:
    ops = ["q", "0 0 %d %d re W n" % (BIG_W, BIG_H),
           "0.98 0.98 0.98 rg 0 0 %d %d re f" % (BIG_W, BIG_H)]
    for row in range(ROWS):
        for col in range(COLUMNS):
            gray = 0.72 if (col + row) % 2 else 0.92
            x, y = col * TILE, row * TILE
            w, h = min(TILE, BIG_W - x), min(TILE, BIG_H - y)
            ops.append("%.2f %.2f %.2f rg %d %d %d %d re f"
                       % (gray, gray, gray, x, y, w, h))
    ops.append("Q")
    return ("\n".join(ops) + "\n").encode("ascii")


def small_page_stream() -> bytes:
    return b"q 1 1 1 rg 0 0 612 792 re f Q\n"


def build() -> bytes:
    chunks = [b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n"]

    def emit(data: bytes) -> bytes:
        chunks.append(data)
        return data

    def snapshot() -> int:
        return sum(len(c) for c in chunks)

    page1 = ("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %d %d] "
             "/Contents %d 0 R >>" % (BIG_W, BIG_H, 1003)).encode("ascii")
    small = ("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %d %d] "
             "/Contents %d 0 R >>" % (SMALL_W, SMALL_H, 1004)).encode("ascii")

    # Object 1: catalog, object 2: pages tree with a 1000-entry kid array.
    emit(b"1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n")
    kids = " ".join("%d 0 R" % n for n in range(3, 3 + PAGE_COUNT))
    emit(("2 0 obj\n<< /Type /Pages /Kids [ %s ] /Count %d >>\nendobj\n"
          % (kids, PAGE_COUNT)).encode("ascii"))
    emit(b"3 0 obj\n" + page1 + b"\nendobj\n")
    for n in range(4, 3 + PAGE_COUNT):
        emit(("%d 0 obj\n" % n).encode("ascii") + small + b"\nendobj\n")

    # Shared content streams: 1003 for page 1, 1004 for pages 2-1000.
    big = big_page_stream()
    tiny = small_page_stream()
    emit(b"1003 0 obj\n<< /Length %d >>\nstream\n" % len(big) + big + b"endstream\nendobj\n")
    emit(b"1004 0 obj\n<< /Length %d >>\nstream\n" % len(tiny) + tiny + b"endstream\nendobj\n")

    # xref + trailer.
    offsets = []
    total = len(chunks[0])
    for i in range(1, len(chunks)):
        offsets.append(total)
        total += len(chunks[i])
    xref = ["xref\n0 %d\n" % (PAGE_COUNT + 5),
            "0000000000 65535 f \n"]
    for off in offsets:
        xref.append("%010d 00000 n \n" % off)
    emit(("".join(xref) + "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n"
          % (PAGE_COUNT + 5, total)).encode("ascii"))
    return b"".join(chunks)


def main() -> int:
    assert COLUMNS * ROWS == 192, "page 1 must be a 16x12 -> 192 tile grid"
    out = Path(__file__).resolve().parent / "corpus-1000p.pdf"
    data = build()
    out.write_bytes(data)
    digest = hashlib.sha256(data).hexdigest()
    print("wrote %s (%d bytes) sha256=%s" % (out, len(data), digest))
    return 0


if __name__ == "__main__":
    sys.exit(main())
