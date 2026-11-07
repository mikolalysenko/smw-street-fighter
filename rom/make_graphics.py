#!/usr/bin/env python3
"""Generate original SNES 4bpp fighter, projectile and HUD artwork.

No downloaded assets or third-party Python packages are used.  The generated
files are build products; the small vector/pixel drawing instructions here are
the canonical source.
"""
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "generated"
W = H = 32

# Transparent, outline, gi, gi shade, skin, skin shade, red, hair, glove,
# cyan, blue, light, dark grey, green, orange, yellow.
RGB = [
    (0, 0, 0), (30, 28, 40), (244, 240, 232), (174, 188, 202),
    (235, 174, 128), (181, 103, 78), (216, 48, 56), (54, 40, 44),
    (190, 38, 48), (88, 216, 240), (40, 120, 208), (255, 248, 216),
    (72, 72, 84), (88, 216, 72), (240, 128, 56), (248, 216, 88),
]


class Canvas:
    def __init__(self, w=W, h=H):
        self.w, self.h = w, h
        self.p = [[0] * w for _ in range(h)]

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.p[y][x] = c

    def rect(self, x0, y0, x1, y1, c, outline=None):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.set(x, y, c)
        if outline is not None:
            self.line(x0, y0, x1, y0, outline)
            self.line(x1, y0, x1, y1, outline)
            self.line(x1, y1, x0, y1, outline)
            self.line(x0, y1, x0, y0, outline)

    def line(self, x0, y0, x1, y1, c):
        dx, sx = abs(x1 - x0), 1 if x1 >= x0 else -1
        dy, sy = -abs(y1 - y0), 1 if y1 >= y0 else -1
        err = dx + dy
        while True:
            self.set(x0, y0, c)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def poly(self, points, c, outline=1):
        # Even/odd scanline fill, followed by a one-pixel outline.
        for y in range(self.h):
            xs = []
            for i, (x1, y1) in enumerate(points):
                x2, y2 = points[(i + 1) % len(points)]
                if (y1 <= y < y2) or (y2 <= y < y1):
                    xs.append(round(x1 + (y - y1) * (x2 - x1) / (y2 - y1)))
            xs.sort()
            for i in range(0, len(xs) - 1, 2):
                for x in range(xs[i], xs[i + 1] + 1):
                    self.set(x, y, c)
        for i, p in enumerate(points):
            self.line(*p, *points[(i + 1) % len(points)], outline)

    def rows(self):
        return ["".join(f"{v:X}" for v in row) for row in self.p]


def head(c, x=13, y=2, band_tail="left"):
    # Hair silhouette, face, headband and two unmistakable red tails.
    c.poly([(x+1,y),(x+6,y),(x+8,y+3),(x+7,y+9),(x+4,y+11),
            (x+1,y+9),(x,y+4)], 7)
    c.poly([(x+2,y+3),(x+7,y+3),(x+7,y+8),(x+5,y+10),
            (x+2,y+8)], 4)
    c.rect(x+1, y+3, x+7, y+4, 6)
    c.set(x+6, y+6, 1)
    c.line(x+3, y+9, x+5, y+9, 5)
    if band_tail == "left":
        c.line(x+1, y+4, x-4, y+2, 6)
        c.line(x+1, y+5, x-5, y+6, 6)
        c.set(x-5, y+5, 6)
    elif band_tail == "up":
        c.line(x+1, y+4, x-3, y-2, 6)
        c.line(x+2, y+4, x, y-3, 6)
    else:
        c.line(x+7, y+4, x+11, y+2, 6)
        c.line(x+7, y+5, x+12, y+6, 6)


def legs(c, left, right):
    # Each leg is (hip x, knee x, foot x, foot y).
    for hip, knee, foot, fy in (left, right):
        c.poly([(hip-3,20),(hip+3,20),(knee+3,26),(foot+3,fy-1),
                (foot+2,fy+1),(foot-3,fy+1),(foot-4,fy-1),(knee-3,26)], 2)
        c.line(hip-1, 22, knee-1, 26, 3)
        c.line(foot-3, fy, foot+3, fy, 1)
        c.line(foot-3, fy+1, foot+3, fy+1, 7)


def torso(c, lean=0, belt=20):
    c.poly([(10+lean,10),(15+lean,9),(21+lean,11),(23+lean,17),
            (21+lean,belt),(10+lean,belt),(8+lean,16)], 2)
    c.poly([(12+lean,11),(16+lean,14),(20+lean,11),(18+lean,18),
            (14+lean,18)], 3, 3)
    c.line(11+lean, belt-1, 21+lean, belt-1, 1)
    c.rect(13+lean, belt, 21+lean, belt+1, 7, 7)
    c.line(17+lean, belt+1, 15+lean, belt+5, 1)


def idle(walk=0):
    c = Canvas()
    legs(c, (12, 11-walk, 9-walk, 30), (19, 20+walk, 22+walk, 30))
    torso(c)
    # Far arm and near arm, held in a fighting stance.
    c.poly([(9,11),(6,13),(5,20),(8,21),(11,15)], 3)
    c.rect(4, 18, 8, 21, 8, 1)
    c.poly([(21,11),(24,13),(25,18),(22,20),(20,16)], 2)
    c.rect(22, 17, 26, 20, 8, 1)
    head(c)
    return c


def jump():
    c = Canvas()
    legs(c, (12, 8, 6, 26), (19, 23, 25, 24))
    torso(c, belt=19)
    c.poly([(9,11),(5,12),(3,17),(6,18),(11,14)], 3)
    c.rect(2, 15, 6, 18, 8, 1)
    c.poly([(21,11),(25,9),(27,5),(24,4),(20,8)], 2)
    c.rect(24, 2, 28, 6, 8, 1)
    head(c, 13, 1, "up")
    return c


def punch():
    c = Canvas()
    legs(c, (12, 10, 7, 30), (19, 21, 24, 30))
    torso(c, lean=1)
    c.poly([(10,12),(6,14),(5,20),(8,21),(12,15)], 3)
    c.rect(4, 18, 8, 21, 8, 1)
    # Extended striking arm reaches the right edge of the canvas.
    c.poly([(21,11),(25,11),(29,13),(29,16),(23,16),(20,14)], 2)
    c.rect(27, 12, 31, 16, 8, 1)
    head(c, 14, 2)
    return c


def kick():
    c = Canvas()
    # Planted left leg, right leg extended horizontally.
    legs(c, (12, 10, 9, 30), (18, 23, 30, 19))
    c.poly([(17,19),(22,16),(30,15),(31,18),(23,21),(18,23)], 2)
    c.rect(28, 15, 31, 19, 7, 1)
    torso(c, lean=-1)
    c.poly([(9,12),(5,15),(4,21),(7,22),(11,16)], 3)
    c.rect(3, 19, 7, 22, 8, 1)
    c.poly([(20,12),(23,14),(22,19),(19,18),(18,14)], 2)
    c.rect(20, 17, 24, 20, 8, 1)
    head(c, 11, 2)
    return c


def hadouken():
    c = Canvas()
    legs(c, (12, 10, 7, 30), (19, 21, 24, 30))
    torso(c)
    # Both palms thrust forward; the projectile itself is a separate OBJ.
    c.poly([(10,11),(7,14),(12,18),(21,17),(22,14),(15,13)], 3)
    c.poly([(20,11),(24,12),(27,14),(26,18),(20,17),(18,14)], 2)
    c.rect(24, 13, 28, 17, 8, 1)
    c.rect(21, 15, 25, 19, 8, 1)
    c.set(29, 14, 11)
    c.set(29, 16, 9)
    head(c, 13, 2)
    return c


def shoryuken():
    c = Canvas()
    legs(c, (12, 10, 8, 27), (19, 21, 23, 25))
    torso(c, belt=19)
    c.poly([(10,12),(7,15),(5,21),(8,22),(12,16)], 3)
    c.rect(4, 19, 8, 22, 8, 1)
    c.poly([(20,12),(22,8),(23,2),(20,0),(18,8)], 2)
    c.rect(20, 0, 24, 4, 8, 1)
    head(c, 12, 3, "up")
    return c


def hurricane():
    c = Canvas()
    # A wide, rotating kick silhouette; animation spins the trailing limbs.
    c.poly([(8,18),(15,17),(25,18),(31,16),(31,20),(23,23),(12,23),(5,21)], 2)
    c.line(12, 20, 26, 20, 3)
    c.rect(28, 16, 31, 20, 7, 1)
    c.poly([(11,9),(18,8),(22,13),(19,19),(12,19),(8,14)], 2)
    c.line(12, 17, 20, 17, 7)
    c.poly([(10,11),(5,10),(1,13),(3,16),(10,15)], 3)
    c.rect(0, 12, 4, 15, 8, 1)
    head(c, 13, 0, "left")
    return c


def guard():
    c = idle()
    # Raised forearms and gloves cover the face and torso.
    c.poly([(20,12),(23,9),(26,10),(25,18),(22,21),(20,18)], 2)
    c.rect(22, 7, 26, 12, 8, 1)
    c.poly([(11,12),(13,10),(17,14),(18,20),(14,20),(11,16)], 3)
    c.rect(14, 12, 18, 16, 8, 1)
    return c


def crouch():
    c = Canvas()
    legs(c, (12, 8, 5, 29), (19, 23, 27, 29))
    c.poly([(10,15),(17,13),(23,17),(21,24),(10,24),(7,20)], 2)
    c.line(10, 23, 21, 23, 7)
    c.poly([(21,16),(25,17),(27,22),(24,24),(20,20)], 2)
    c.rect(24, 20, 28, 24, 8, 1)
    head(c, 12, 7)
    return c


def projectile(super=False):
    c = Canvas(32, 16)
    if super:
        # 32x16 Shinku core, with a hot center and cyan/blue outer flame.
        c.poly([(0,12),(5,7),(10,4),(22,3),(30,7),(31,11),(27,15),
                (13,16),(5,15)], 10, 10)
        c.poly([(3,12),(9,7),(20,5),(28,8),(27,12),(18,14),(8,14)], 9, 9)
        c.poly([(9,11),(14,7),(23,7),(26,9),(22,12),(14,13)], 11, 11)
        c.line(1, 14, 8, 15, 6)
    else:
        c.poly([(0,10),(3,5),(8,2),(13,4),(16,9),(13,14),(7,16),(2,14)], 10, 10)
        c.poly([(3,9),(6,4),(11,4),(14,8),(12,12),(7,14),(4,12)], 9, 9)
        c.poly([(6,8),(9,5),(12,7),(11,11),(8,12)], 11, 11)
    return c


def hud_tiles():
    tiles = []
    for base, hi, empty in ((6, 14, False), (12, 12, True),
                            (9, 11, False), (12, 12, True)):
        c = Canvas(8, 8)
        c.rect(0, 1, 7, 6, 1)
        c.rect(1, 2, 6, 5, base)
        if not empty:
            c.line(1, 2, 6, 2, hi)
        tiles.append(c)
    return tiles


def encode_4bpp(canvas):
    """Encode an 8x8-multiple canvas as row-major SNES 4bpp characters."""
    assert canvas.w % 8 == 0 and canvas.h % 8 == 0
    out = bytearray()
    for ty in range(0, canvas.h, 8):
        for tx in range(0, canvas.w, 8):
            plane = [[0] * 8 for _ in range(4)]
            for y in range(8):
                for x in range(8):
                    v = canvas.p[ty+y][tx+x]
                    for bit in range(4):
                        plane[bit][y] |= ((v >> bit) & 1) << (7-x)
            for y in range(8):
                out.extend((plane[0][y], plane[1][y]))
            for y in range(8):
                out.extend((plane[2][y], plane[3][y]))
    return bytes(out)


def png(path, canvases, scale=8):
    """Write a contact sheet using only the standard library."""
    width = len(canvases) * W
    raw = bytearray()
    for y in range(H):
        row = bytearray()
        for c in canvases:
            for value in c.p[y]:
                row.extend(RGB[value] if value else (28, 34, 48))
        expanded = bytearray()
        for x in range(width):
            expanded.extend(row[3*x:3*x+3] * scale)
        for _ in range(scale):
            raw.append(0)
            raw.extend(expanded)
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind+data) & 0xffffffff)
    data = b"\x89PNG\r\n\x1a\n"
    data += chunk(b"IHDR", struct.pack(">IIBBBBB", width*scale, H*scale, 8, 2, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    data += chunk(b"IEND", b"")
    path.write_bytes(data)


def main():
    OUT.mkdir(exist_ok=True)
    frames = [idle(0), idle(1), jump(), punch(), kick(), hadouken(),
              shoryuken(), hurricane(), guard(), crouch()]
    (OUT / "fighter.chr").write_bytes(b"".join(encode_4bpp(f) for f in frames))
    shots = [projectile(False), projectile(True)]
    (OUT / "projectile.chr").write_bytes(b"".join(encode_4bpp(f) for f in shots))
    (OUT / "hud.chr").write_bytes(b"".join(encode_4bpp(t) for t in hud_tiles()))
    palette = bytearray()
    for r, g, b in RGB:
        color = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)
        palette.extend(struct.pack("<H", color))
    (OUT / "palette.bin").write_bytes(palette)
    png(OUT / "preview.png", frames, scale=6)
    print(f"Generated {len(frames)} fighter poses, 2 projectiles, 4 HUD tiles and palette in {OUT}")


if __name__ == "__main__":
    main()
