#!/usr/bin/env python3
"""Create a compact native roster-selection sheet from the pinned WWA1 pack."""
from pathlib import Path
import hashlib
import json
import struct
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "rom/generated"


def bgr555_to_rgb(value):
    return tuple((component << 3) | (component >> 2)
                 for component in (value & 31, (value >> 5) & 31, (value >> 10) & 31))


def encode_4bpp(im):
    assert im.size == (16, 16)
    out = bytearray()
    for ty in range(2):
        for tx in range(2):
            planes = [[0] * 8 for _ in range(4)]
            for y in range(8):
                for x in range(8):
                    value = im.getpixel((tx * 8 + x, ty * 8 + y))
                    for bit in range(4):
                        planes[bit][y] |= ((value >> bit) & 1) << (7 - x)
            for y in range(8):
                out.extend((planes[0][y], planes[1][y]))
            for y in range(8):
                out.extend((planes[2][y], planes[3][y]))
    return bytes(out)


def main():
    source = ROOT / "local/sf2/sprites.dat"
    data = source.read_bytes()
    assert data[:4] == b"WWA1"
    count = struct.unpack_from("<H", data, 4)[0]
    palette_data = (OUT / "roster_palettes.bin").read_bytes()
    palettes = []
    for character in range(12):
        colors = [bgr555_to_rgb(struct.unpack_from("<H", palette_data, character * 32 + i * 2)[0])
                  for i in range(16)]
        palettes.append(colors)
    records = [None] * 12
    cursor = 6
    for _ in range(count):
        character, pose, ticks, w, h, ax, ay = struct.unpack_from("<7H", data, cursor)
        cursor += 14
        pixels = data[cursor:cursor + w * h * 4]
        cursor += w * h * 4
        if records[character] is None and pose == 0:
            records[character] = (w, h, ax, ay, pixels)
    assert cursor == len(data) and all(records)
    output = bytearray()
    for character, (w, h, ax, ay, pixels) in enumerate(records):
        im = Image.frombytes("RGBA", (w, h), pixels)
        bbox = im.getbbox()
        if bbox:
            im = im.crop(bbox)
        im.thumbnail((16, 16), Image.Resampling.NEAREST)
        canvas = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
        canvas.paste(im, ((16 - im.width) // 2, 16 - im.height))
        indexed = Image.new("P", (16, 16), 0)
        flat = []
        for r, g, b in palettes[character]:
            flat.extend((r, g, b))
        indexed.putpalette(flat)
        indexed.paste(canvas.convert("RGB").quantize(palette=indexed, dither=Image.Dither.NONE))
        alpha = canvas.getchannel("A")
        indexed.putalpha(alpha)
        # Quantize() may allocate transparent pixels to a visible palette entry.
        indexed = indexed.convert("RGBA")
        remapped = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
        remapped.putdata([(r, g, b, 0 if a < 128 else 255)
                          for r, g, b, a in indexed.getdata()])
        # Convert palette indices directly; transparent is always index zero.
        colors = palettes[character]
        indexed_pixels = []
        for r, g, b, a in remapped.getdata():
            if not a:
                indexed_pixels.append(0)
            else:
                indexed_pixels.append(min(range(1, 16),
                                          key=lambda i: sum((v - t) ** 2 for v, t in zip((r, g, b), colors[i]))))
        output.extend(encode_4bpp(Image.frombytes("P", (16, 16), bytes(indexed_pixels))))
    (OUT / "roster_select.chr").write_bytes(output)
    manifest = {"source_sha256": hashlib.sha256(data).hexdigest(),
                "frames": 12, "bytes": len(output), "canvas": [16, 16]}
    (OUT / "roster_select_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Wrote {len(output)} bytes of compact selection art")


if __name__ == "__main__":
    main()
