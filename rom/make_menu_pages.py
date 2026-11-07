#!/usr/bin/env python3
"""Build native title/roster screens from the pinned WWA1 art.

Thirteen full-screen indexed pages share one 4bpp tile dictionary. Each page has
its own 32x28 tilemap, so the ROM stores at most 28,672 tile bytes plus 23,296
bytes of tilemaps rather than thirteen uncompressed 28,672-byte screens.
"""
from pathlib import Path
import hashlib
import json
import struct
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "rom/generated"
PAGE_TILES = 32 * 28
PAGE_MAP_BYTES = PAGE_TILES * 2
MAX_TILES = 1024
BACKGROUND = (3, 6, 18)
NAMES = ("RYU", "E. HONDA", "CHUN-LI", "GUILE", "KEN", "BLANKA",
         "ZANGIEF", "DHALSIM", "BALROG", "VEGA", "SAGAT", "M. BISON")


def read_records():
    data = (ROOT / "local/sf2/sprites.dat").read_bytes()
    assert data[:4] == b"WWA1"
    wanted = {}
    cursor = 6
    for _ in range(struct.unpack_from("<H", data, 4)[0]):
        character, pose, ticks, w, h, ax, ay = struct.unpack_from("<7H", data, cursor)
        cursor += 14
        pixels = data[cursor:cursor + w * h * 4]
        cursor += w * h * 4
        if (pose in (15, 16, 27, 28) and (pose, character) not in wanted):
            wanted[pose, character] = Image.frombytes("RGBA", (w, h), pixels, "raw", "BGRA")
    assert cursor == len(data)
    return data, wanted


def opaque_crop(image):
    box = image.getbbox()
    return image.crop(box) if box else image


def text_layer(size, lines):
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    for xy, text, scale in lines:
        x, y = xy
        small = Image.new("RGBA", (max(1, len(text) * 6), 8), (0, 0, 0, 0))
        ImageDraw.Draw(small).text((0, 0), text, fill=(230, 240, 255, 255))
        if scale != 1:
            small = small.resize((small.width * scale, small.height * scale), Image.Resampling.NEAREST)
        layer.alpha_composite(small, (x, y))
    return layer


def title_page(records):
    page = Image.new("RGBA", (256, 224), BACKGROUND + (255,))
    page.alpha_composite(text_layer((256, 32), [((62, 12), "SUPER MARIO WORLD", 1)]), (0, 0))
    logo = opaque_crop(records[27, 0])
    logo = logo.resize((46, 53), Image.Resampling.NEAREST)
    page.alpha_composite(logo, ((256 - logo.width) // 2, 30))
    subtitle = opaque_crop(records[28, 0])
    subtitle = subtitle.resize((193, 10), Image.Resampling.NEAREST)
    page.alpha_composite(subtitle, ((256 - subtitle.width) // 2, 118))
    page.alpha_composite(text_layer((256, 224), [
        ((88, 170), "PRESS START", 1),
    ]))
    return page


def selection_page(records, selected, show_1p):
    page = Image.new("RGBA", (256, 224), (8, 12, 81, 255))
    draw = ImageDraw.Draw(page)
    for y in range(0, 224, 4):
        draw.rectangle((0, y, 255, y + 1), fill=(16, 21, 99, 255))
    draw.rectangle((4, 4, 251, 219), outline=(56, 84, 161, 255))
    page.alpha_composite(text_layer((256, 24), [((62, 6), "PLAYER SELECT", 2)]))
    page.alpha_composite(records[16, 0], (32, 29))
    for index, name in enumerate(NAMES):
        x, y = 8 + (index % 6) * 40, 114 + (index // 6) * 44
        active = index == selected
        draw = ImageDraw.Draw(page)
        draw.rectangle((x, y, x + 37, y + 40),
                       fill=(41, 121, 200, 255) if active else (19, 29, 89, 255))
        draw.rectangle((x - 1, y - 1, x + 38, y + 41),
                       outline=(255, 255, 138, 255) if active else (97, 124, 203, 255))
        portrait = opaque_crop(records[15, index])
        portrait.thumbnail((27, 36), Image.Resampling.NEAREST)
        page.alpha_composite(portrait, (x + 19 - portrait.width // 2, y + 39 - portrait.height))
        if active and show_1p:
            page.alpha_composite(text_layer((16, 12), [((0, 0), "1P", 1)]), (x + 2, y + 2))
    name = NAMES[selected]
    page.alpha_composite(text_layer((256, 224), [
        ((128 - len(name) * 3, 94), name, 1),
        ((31, 212), "DPAD SELECT  A / START CONFIRM", 1),
    ]))
    return page


def quantize_page(page, palette=None):
    if palette is None:
        palette = page.convert("RGB").quantize(colors=16, method=Image.Quantize.MEDIANCUT)
        raw = list(palette.getpalette()[:48])
        bg_index = min(range(16), key=lambda i: sum((raw[i * 3 + c] - BACKGROUND[c]) ** 2
                                                    for c in range(3)))
        if bg_index:
            raw[:3], raw[bg_index * 3:bg_index * 3 + 3] = raw[bg_index * 3:bg_index * 3 + 3], raw[:3]
            palette.putpalette(raw)
            palette = palette.point(lambda value: bg_index if value == 0 else (0 if value == bg_index else value))
        page = palette
    else:
        page = page.convert("RGB").quantize(palette=palette, dither=Image.Dither.NONE)
    return page


def encode_tile(page, tile):
    tx, ty = tile % 32, tile // 32
    planes = [[0] * 8 for _ in range(4)]
    for y in range(8):
        for x in range(8):
            value = page.getpixel((tx * 8 + x, ty * 8 + y))
            for bit in range(4):
                planes[bit][y] |= ((value >> bit) & 1) << (7 - x)
    out = bytearray()
    for group in ((0, 1), (2, 3)):
        for y in range(8):
            out.extend((planes[group[0]][y], planes[group[1]][y]))
    return bytes(out)


def main():
    source, records = read_records()
    rgba_pages = [title_page(records)]
    for choice in range(12):
        rgba_pages.extend((selection_page(records, choice, True),
                           selection_page(records, choice, False)))
    selection_palette = rgba_pages[1].convert("RGB").quantize(colors=16, method=Image.Quantize.MEDIANCUT)
    pages = [quantize_page(rgba_pages[0])] + [quantize_page(page, selection_palette)
                                              for page in rgba_pages[1:]]
    tile_data = bytearray()
    tile_ids = {}
    maps = bytearray()
    for page in pages:
        for tile in range(PAGE_TILES):
            raw = encode_tile(page, tile)
            if raw not in tile_ids:
                tile_ids[raw] = len(tile_ids)
                tile_data.extend(raw)
            maps.extend(struct.pack("<H", tile_ids[raw]))
    assert len(tile_ids) <= MAX_TILES, f"menu needs {len(tile_ids)} tiles, limit {MAX_TILES}"
    assert len(maps) == PAGE_MAP_BYTES * 25
    # Page 18 would otherwise cross $7AFFFF during one DMA. Keep every page
    # contiguous by padding before it; pointers below use the same layout.
    maps = maps[:0x7E00] + bytes(0x200) + maps[0x7E00:]
    assert len(maps) == PAGE_MAP_BYTES * 25 + 0x200
    palettes = bytearray()
    for page in pages:
        raw = page.getpalette()[:48]
        for index in range(16):
            r, g, b = raw[index * 3:index * 3 + 3]
            palettes.extend(struct.pack("<H", (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)))
    (OUT / "menu_tiles.chr").write_bytes(tile_data)
    (OUT / "menu_tilemaps.bin").write_bytes(maps)
    (OUT / "menu_pages.pal").write_bytes(palettes)
    rows = (len(tile_ids) + 31) // 32
    pointers = [f"!WWMenuTileRows = {rows}", "WW_MenuTilemapPointers:"]
    for page in range(25):
        offset = page * PAGE_MAP_BYTES
        address = (0x7A8000 + offset) if page < 18 else (0x7B8000 + offset - 0x7E00)
        pointers.append(f"    dl ${address:06X}")
    pointers.append("WW_MenuPalettePointers:")
    palette_base = len(maps)
    for page in range(25):
        offset = palette_base + page * 32
        address = 0x7B8000 + offset - 0x8000
        pointers.append(f"    dl ${address:06X}")
    (OUT / "menu_page_pointers.asm").write_text("\n".join(pointers) + "\n")
    pages[0].convert("RGB").resize((512, 448), Image.Resampling.NEAREST).save(OUT / "menu-title.png")
    pages[1].convert("RGB").resize((512, 448), Image.Resampling.NEAREST).save(OUT / "menu-select.png")
    manifest = {"source_sha256": hashlib.sha256(source).hexdigest(), "pages": 25,
                "unique_tiles": len(tile_ids), "tile_bytes": len(tile_data), "tile_rows": rows,
                "tilemap_bytes": len(maps), "palette_bytes": len(palettes)}
    (OUT / "menu_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Wrote {len(tile_ids)} menu tiles ({len(tile_data)} bytes), 25 tilemaps and palettes")


if __name__ == "__main__":
    main()
