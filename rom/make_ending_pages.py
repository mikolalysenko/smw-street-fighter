#!/usr/bin/env python3
"""Build twelve full-screen 4bpp ending pages and native palettes."""
from pathlib import Path
import json
import struct
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "rom/generated"
TILE = 32
PAGE_BYTES = 32 * 28 * TILE
ILLUSTRATORS = {1: 0, 5: 1, 6: 2, 7: 3, 8: 4, 10: 5, 11: 6}
ENDING_PALETTES = {1: 0x2A0, 5: 0x2C0, 6: 0x2E0, 7: 0x300,
                   8: 0x320, 10: 0x340, 11: 0x360}


def encode(im):
    assert im.size == (256, 224) and im.mode == "P"
    out = bytearray()
    for ty in range(28):
        for tx in range(32):
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


def decode_snes(data, width_tiles, height_tiles):
    im = Image.new("P", (width_tiles * 8, height_tiles * 8), 0)
    for tile in range(width_tiles * height_tiles):
        tx, ty = tile % width_tiles, tile // width_tiles
        for y in range(8):
            p0, p1, p2, p3 = (data[tile * 32 + y * 2], data[tile * 32 + y * 2 + 1],
                              data[tile * 32 + 16 + y * 2], data[tile * 32 + 16 + y * 2 + 1])
            for x in range(8):
                value = ((p0 >> (7 - x) & 1) | ((p1 >> (7 - x) & 1) << 1) |
                         ((p2 >> (7 - x) & 1) << 2) | ((p3 >> (7 - x) & 1) << 3))
                im.putpixel((tx * 8 + x, ty * 8 + y), value)
    return im


def palette_words(data, offset):
    return [struct.unpack_from("<H", data, offset + i * 2)[0] for i in range(16)]


def rgb_colors(words):
    return [((v & 31) << 3, ((v >> 5) & 31) << 3, ((v >> 10) & 31) << 3)
            for v in words]


def main():
    config = json.loads((ROOT / "config/endings.json").read_text())
    source = (ROOT / "local/sf2/sprites.dat").read_bytes()
    palettes = (OUT / "roster_palettes.bin").read_bytes()
    illustrations = (OUT / "roster_endings.chr").read_bytes()
    records = {}
    cursor = 6
    for _ in range(struct.unpack_from("<H", source, 4)[0]):
        character, pose, ticks, w, h, ax, ay = struct.unpack_from("<7H", source, cursor)
        cursor += 14
        pixels = source[cursor:cursor + w * h * 4]
        cursor += w * h * 4
        if pose == 20 and character not in records:
            records[character] = (w, h, pixels)
    pages = bytearray()
    page_palettes = bytearray()
    for character, entry in enumerate(config["characters"]):
        if character in ILLUSTRATORS:
            palette = palette_words(palettes, ENDING_PALETTES[character])
            page = Image.new("P", (256, 224), 0)
            raw = illustrations[ILLUSTRATORS[character] * 18432:(ILLUSTRATORS[character] + 1) * 18432]
            art = decode_snes(raw, 32, 18)
            page.paste(art, (0, 32))
        else:
            palette = palette_words(palettes, character * 32)
            page = Image.new("P", (256, 224), 0)
            w, h, pixels = records[character]
            rgba = Image.frombytes("RGBA", (w, h), pixels)
            bbox = rgba.getbbox()
            if bbox:
                rgba = rgba.crop(bbox)
            rgba.thumbnail((128, 128), Image.Resampling.NEAREST)
            rgba = rgba.resize((rgba.width * 2, rgba.height * 2), Image.Resampling.NEAREST)
            colors = rgb_colors(palette)
            indexed = Image.new("P", rgba.size, 0)
            flat = [component for color in colors for component in color]
            indexed.putpalette(flat)
            indexed = rgba.convert("RGB").quantize(palette=indexed, dither=Image.Dither.NONE)
            alpha = rgba.getchannel("A")
            for y in range(rgba.height):
                for x in range(rgba.width):
                    if alpha.getpixel((x, y)) < 128:
                        indexed.putpixel((x, y), 0)
            page.paste(indexed, ((256 - rgba.width) // 2, 40 + (128 - rgba.height) // 2))
        draw = ImageDraw.Draw(page)
        draw.text((8, 8), entry["title"], fill=15)
        for i, line in enumerate(entry["lines"]):
            draw.text((8, 192 + i * 10), line, fill=15)
        pages.extend(encode(page))
        page_palettes.extend(struct.pack("<H", 0) + b"".join(struct.pack("<H", v) for v in palette[1:]))
    tilemap = bytearray()
    for tile in range(32 * 28):
        tilemap.extend(struct.pack("<H", tile))
    (OUT / "ending_pages.chr").write_bytes(pages)
    (OUT / "ending_pages.pal").write_bytes(page_palettes)
    (OUT / "ending_tilemap.bin").write_bytes(tilemap)
    pointers = []
    for page in range(12):
        offset = page * PAGE_BYTES
        address = (0x60 + offset // 0x8000) << 16 | 0x8000 + offset % 0x8000
        pointers.append(f"    dl ${address:06X}")
    (OUT / "ending_page_pointers.asm").write_text(
        "; Canonical LoROM addresses computed from linear page offsets.\n" +
        "\n".join(pointers) + "\n")
    assert len(pages) == PAGE_BYTES * 12 and len(page_palettes) == 32 * 12
    (OUT / "ending_manifest.json").write_text(json.dumps({
        "pages": 12, "bytes_per_page": PAGE_BYTES, "palette_bytes": len(page_palettes),
        "illustrated": sorted(ILLUSTRATORS), "victory_scenes": [0, 2, 3, 4, 9]
    }, indent=2) + "\n")
    print(f"Wrote {len(pages)} bytes of ending pages and {len(page_palettes)} palette bytes")


if __name__ == "__main__":
    main()
