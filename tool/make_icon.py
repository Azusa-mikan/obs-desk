#!/usr/bin/env python3
"""Generate assets/icons/obs_desk.png (256x256 RGBA PNG).

Pure Python standard library only (no PIL / ImageMagick / rsvg). The icon is a
dark rounded square with a simple monitor/screen and a play triangle in the
centre. Anti-aliasing is done with 3x3 supersampling; pixels are tested against
analytic rounded-rectangle / triangle shapes.

Running the script is idempotent: it always writes the same bytes.
"""

import os
import struct
import zlib

SIZE = 256
SS = 3  # supersampling factor per axis

# --- Colours (R, G, B, A) -------------------------------------------------

BG = (23, 27, 34, 255)        # dark slate, rounded square backing
BEZEL = (203, 213, 225, 255)  # light grey monitor body / stand
SCREEN = (37, 99, 235, 255)   # blue screen
PLAY = (255, 255, 255, 255)   # white play triangle


def in_round_rect(px, py, x0, y0, x1, y1, r):
    """True if (px, py) lies inside a rounded rectangle."""
    if px < x0 or px > x1 or py < y0 or py > y1:
        return False
    cx = min(max(px, x0 + r), x1 - r)
    cy = min(max(py, y0 + r), y1 - r)
    dx = px - cx
    dy = py - cy
    return dx * dx + dy * dy <= r * r


def in_triangle(px, py, ax, ay, bx, by, cx, cy):
    """True if (px, py) lies inside triangle ABC (sign of cross products)."""
    d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by)
    d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy)
    d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay)
    has_neg = d1 < 0 or d2 < 0 or d3 < 0
    has_pos = d1 > 0 or d2 > 0 or d3 > 0
    return not (has_neg and has_pos)


def sample_color(x, y):
    """Colour of a single point, layering shapes front-to-back."""
    color = (0, 0, 0, 0)  # transparent outside the backing square

    if in_round_rect(x, y, 16, 16, 240, 240, 52):
        color = BG

    # Monitor stand (neck + base).
    if in_round_rect(x, y, 118, 172, 138, 196, 4):
        color = BEZEL
    if in_round_rect(x, y, 100, 196, 156, 204, 4):
        color = BEZEL

    # Monitor body and screen.
    if in_round_rect(x, y, 44, 60, 212, 176, 14):
        color = BEZEL
    if in_round_rect(x, y, 52, 68, 204, 168, 10):
        color = SCREEN

    # Play triangle, centred in the screen.
    if in_triangle(x, y, 116.0, 98.0, 116.0, 138.0, 152.0, 118.0):
        color = PLAY

    return color


def build_pixels():
    """Return raw image rows (bytes) with per-row filter byte 0."""
    inv = 1.0 / (SS * SS)
    rows = bytearray()
    for py in range(SIZE):
        rows.append(0)  # PNG filter type 0 (None) for each scanline
        for px in range(SIZE):
            r = g = b = a = 0.0
            for sy in range(SS):
                fy = py + (sy + 0.5) / SS
                for sx in range(SS):
                    fx = px + (sx + 0.5) / SS
                    cr, cg, cb, ca = sample_color(fx, fy)
                    r += cr
                    g += cg
                    b += cb
                    a += ca
            r = int(round(r * inv))
            g = int(round(g * inv))
            b = int(round(b * inv))
            a = int(round(a * inv))
            rows.extend((r, g, b, a))
    return bytes(rows)


def png_chunk(tag, data):
    return (
        struct.pack(">I", len(data))
        + tag
        + data
        + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    )


def write_png(path):
    raw = build_pixels()
    ihdr = struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)  # 8-bit RGBA
    png = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", ihdr)
        + png_chunk(b"IDAT", zlib.compress(raw, 9))
        + png_chunk(b"IEND", b"")
    )
    with open(path, "wb") as fh:
        fh.write(png)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.join(here, "..", "assets", "icons")
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "obs_desk.png")
    write_png(out_path)
    print("wrote %s" % os.path.normpath(out_path))


if __name__ == "__main__":
    main()
