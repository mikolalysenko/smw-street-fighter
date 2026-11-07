#!/usr/bin/env python3
"""Convert the pinned WWA1 sprite pack to native SNES 4bpp graphics.

The generated assembly is deliberately independent of the current vertical-slice
renderer.  It exposes animation descriptors, 24-bit frame pointers, fixed-size
frame metadata, palettes, and OAM templates for a future full-roster renderer.
"""
from __future__ import annotations

import argparse
from collections import OrderedDict, defaultdict
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

try:
    from PIL import Image, ImageDraw
except ImportError as exc:  # pragma: no cover - depends on the build environment
    raise SystemExit(
        "Pillow is required. Run with build/asset-tools/bin/python3 after "
        "installing the asset-tool requirements."
    ) from exc

ROOT = Path(__file__).resolve().parents[1]
ROM_DIR = ROOT / "rom"
DEFAULT_SOURCE = ROOT / "local/sf2/sprites.dat"
DEFAULT_OUT = ROM_DIR / "generated"
ASM_PATH = ROM_DIR / "roster_assets.asm"

ROSTER = (
    "Ryu", "Honda", "ChunLi", "Guile", "Ken", "Blanka",
    "Zangief", "Dhalsim", "Balrog", "Vega", "Sagat", "Bison",
)
POSE_NAMES = (
    "Idle", "Walk", "Jump", "Duck", "Punch", "Kick", "Fire", "Rise",
    "Spin", "Rush", "Flurry", "Hurt", "KO", "Guard", "Shot", "Portrait",
    "Map", "Font", "BigFont", "KOMark", "Win", "AirPunch", "AirKick",
    "CrouchPunch", "CrouchKick", "Continue", "CountdownLamp", "TitleII",
    "TitleSubtitle", "SuperCharge", "AkumaPilot", "AkumaFire", "AkumaDemon",
    "AkumaShot", "BisonHat", "AkumaWarp", "Psycho", "Throw", "Teleport",
    "Dive", "HUDPortrait", "RoundMark", "HUDFont", "JumpForward",
    "AkumaAirFire", "AkumaRise", "AkumaGuard", "AkumaHurt", "AkumaTaunt",
    "AkumaPurpleShot", "AkumaKO", "Ending", "CrouchGuard",
)
ACTION_POSES = frozenset((
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
    20, 21, 22, 23, 24, 30, 31, 32, 35, 37, 38, 39,
    43, 44, 45, 46, 47, 48, 50, 52,
))
PROJECTILE_POSES = frozenset((14, 33, 49))
PORTRAIT_POSES = frozenset((15, 25, 40))
EFFECT_POSES = frozenset((29, 36))
UI_POSES = frozenset((16, 17, 18, 19, 26, 27, 28, 34, 41, 42))
ALPHA_POSES = frozenset((29, 30, 31, 32, 33, 34, 35, 36, 44, 45, 46, 47, 48, 49, 50))
ONCE_POSES = frozenset((
    2, 4, 5, 6, 7, 8, 9, 10, 12, 20, 21, 22, 23, 24, 37, 38, 39, 43,
))
assert len(POSE_NAMES) == 53
assert ACTION_POSES | PROJECTILE_POSES | PORTRAIT_POSES | EFFECT_POSES | UI_POSES | {51} == set(range(53))
assert not ((ACTION_POSES & PROJECTILE_POSES) | (ACTION_POSES & PORTRAIT_POSES) |
            (ACTION_POSES & EFFECT_POSES) | (ACTION_POSES & UI_POSES))

FORMAT_ACTION = 0
FORMAT_PROJECTILE = 1
FORMAT_PORTRAIT = 2
FORMAT_UI = 3
FORMAT_EFFECT = 4
FORMAT_ENDING = 5
FORMAT_NAMES = ("action", "projectile", "portrait", "ui", "effect", "ending")
CATEGORY_FILES = (
    "roster_action.chr", "roster_projectiles.chr", "roster_portraits.chr",
    "roster_ui.chr", "roster_effects.chr", "roster_endings.chr",
)
CATEGORY_LABELS = (
    "WW_RosterActionGraphics", "WW_RosterProjectileGraphics",
    "WW_RosterPortraitGraphics", "WW_RosterUIGraphics",
    "WW_RosterEffectGraphics", "WW_RosterEndingGraphics",
)
META_SIZE = 16
DESCRIPTOR_SIZE = 13
ACTION_SIZE = 48 * 48 // 2
PROJECTILE_SIZE = 32 * 32 // 2
SCALE_BASE = 252  # Fits in a byte after reduction; 126/252 reduces to exactly 1/2.
LOROM_LIMIT = 4 * 1024 * 1024
BASE_ROM_SIZE = 512 * 1024
BANK_SIZE = 32 * 1024
ASSET_LAYOUT_BASE = 0x128000  # Bank $10 is code; $118000 is the reserved SPC image.


@dataclass
class SourceFrame:
    index: int
    character: int
    pose: int
    ticks: int
    width: int
    height: int
    ax: int
    ay: int
    image: Image.Image


@dataclass
class ConvertedFrame:
    source: SourceFrame
    format: int
    palette_key: str
    image: Image.Image
    scale_num: int
    scale_den: int
    ax: int
    ay: int
    graphics_offset: int
    meta_offset: int

    @property
    def key(self) -> tuple[int, int]:
        return (self.source.character, self.source.pose)


@dataclass
class Palette:
    key: str
    colors: list[tuple[int, int, int]]  # 15 visible colors, already SNES-expanded
    offset: int = 0


def read_wwa1(path: Path) -> tuple[list[SourceFrame], str]:
    """Parse and strictly validate WWA1, including exact end-of-file."""
    data = path.read_bytes()
    if len(data) < 6 or data[:4] != b"WWA1":
        raise ValueError(f"{path} is not a WWA1 sprite pack")
    count = struct.unpack_from("<H", data, 4)[0]
    if not 1 <= count <= 4096:
        raise ValueError(f"Invalid WWA1 frame count: {count}")
    frames = []
    offset = 6
    for index in range(count):
        if offset + 14 > len(data):
            raise ValueError(f"Truncated WWA1 record header at frame {index}")
        character, pose, ticks, width, height, ax, ay = struct.unpack_from("<7H", data, offset)
        offset += 14
        if not (0 <= character < 12 and 0 <= pose < 53):
            raise ValueError(f"Invalid character/pose at frame {index}: {character}/{pose}")
        if not (1 <= ticks <= 600 and 1 <= width <= 512 and 1 <= height <= 512):
            raise ValueError(f"Invalid timing/dimensions at frame {index}")
        if ax > 512 or ay > 512:
            raise ValueError(f"Invalid anchor at frame {index}")
        size = width * height * 4
        if offset + size > len(data):
            raise ValueError(f"Truncated WWA1 pixels at frame {index}")
        # WWA1 stores little-endian 0xAARRGGBB, hence B,G,R,A byte order.
        image = Image.frombytes("RGBA", (width, height), data[offset:offset + size], "raw", "BGRA")
        offset += size
        frames.append(SourceFrame(index, character, pose, ticks, width, height, ax, ay, image))
    if offset != len(data):
        raise ValueError(f"WWA1 has {len(data) - offset} trailing bytes")
    return frames, hashlib.sha256(data).hexdigest()


def category_for(pose: int) -> int:
    if pose in ACTION_POSES:
        return FORMAT_ACTION
    if pose in PROJECTILE_POSES:
        return FORMAT_PROJECTILE
    if pose in PORTRAIT_POSES:
        return FORMAT_PORTRAIT
    if pose in EFFECT_POSES:
        return FORMAT_EFFECT
    if pose == 51:
        return FORMAT_ENDING
    return FORMAT_UI


def palette_key(frame: SourceFrame) -> str:
    if frame.pose == 51:
        return f"ending_{ROSTER[frame.character].lower()}"
    if frame.character == 0 and frame.pose in ALPHA_POSES and frame.pose != 34:
        return "alpha"
    if frame.character == 0 and frame.pose in UI_POSES:
        group = {
            16: "map", 17: "font", 18: "font", 42: "font", 19: "ko",
            26: "continue", 27: "title", 28: "subtitle", 34: "bison_hat", 41: "round",
        }[frame.pose]
        return f"ui_{group}"
    return f"fighter_{ROSTER[frame.character].lower()}"


def snes_color(color: tuple[int, int, int]) -> int:
    r, g, b = color
    return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)


def expand_color(value: int) -> tuple[int, int, int]:
    r = value & 31
    g = (value >> 5) & 31
    b = (value >> 10) & 31
    return ((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2))


def build_palette(key: str, frames: list[SourceFrame]) -> Palette:
    """Median-cut a deterministic, per-frame-balanced sample to 15 colors."""
    samples: list[tuple[int, int, int]] = []
    for frame in frames:
        pixels = list(frame.image.getdata())
        opaque = [p for p in pixels if p[3] >= 128]
        if not opaque:
            continue
        step = max(1, math.ceil(math.sqrt(len(opaque) / 2048)))
        for y in range(0, frame.height, step):
            for x in range(0, frame.width, step):
                r, g, b, a = frame.image.getpixel((x, y))
                if a >= 128:
                    samples.append((r, g, b))
    if not samples:
        colors = [(0, 0, 0)] * 15
    else:
        sample_image = Image.new("RGB", (len(samples), 1))
        sample_image.putdata(samples)
        quantized = sample_image.quantize(colors=15, method=Image.Quantize.MEDIANCUT)
        raw = quantized.getpalette()
        colors = [tuple(raw[i:i + 3]) for i in range(0, min(45, len(raw)), 3)]
        colors = [color for color in colors if len(color) == 3]
        if not colors:
            colors = [(0, 0, 0)]
        while len(colors) < 15:
            colors.append(colors[-1])
    # Quantize the palette itself to the hardware's 5 bits per channel now, so
    # previews and nearest-color corrections exactly match the SNES data.
    colors = [expand_color(snes_color(c)) for c in colors]
    return Palette(key, colors)


def palette_image(palette: Palette) -> Image.Image:
    result = Image.new("P", (1, 1))
    colors = [(0, 0, 0)] + palette.colors
    flat = [v for color in colors for v in color]
    # Duplicate a visible color instead of adding more black entries.  Any
    # duplicate index returned by Pillow is still remapped to 1..15 below.
    flat += list(flat[3:6]) * (256 - len(colors))
    result.putpalette(flat)
    return result


def quantize_rgba(image: Image.Image, palette: Palette) -> Image.Image:
    rgb = Image.new("RGB", image.size)
    rgb.paste(image, mask=image.getchannel("A"))
    result = rgb.quantize(palette=palette_image(palette), dither=Image.Dither.NONE)
    source = list(image.getdata())
    indices = bytearray(result.tobytes())
    nearest_cache: dict[tuple[int, int, int], int] = {}

    def nearest(color: tuple[int, int, int]) -> int:
        if color not in nearest_cache:
            nearest_cache[color] = 1 + min(
                range(15),
                key=lambda i: sum((color[channel] - palette.colors[i][channel]) ** 2 for channel in range(3)),
            )
        return nearest_cache[color]

    for i, index in enumerate(indices):
        r, g, b, a = source[i]
        if a < 128:
            indices[i] = 0
        elif index == 0 or index > 15:
            # Index zero is transparency, never a visible black.  This also
            # handles duplicate padded palette entries returned by Pillow.
            indices[i] = nearest((r, g, b))
    result.frombytes(bytes(indices))
    result.putpalette([v for color in [(0, 0, 0)] + palette.colors for v in color] + [0] * (768 - 48))
    return result


def scaled_round(value: int, num: int, den: int = SCALE_BASE) -> int:
    return (value * num + den // 2) // den


def reduced_scale(num: int, den: int = SCALE_BASE) -> tuple[int, int]:
    divisor = math.gcd(num, den)
    return num // divisor, den // divisor


def action_geometry(frame: SourceFrame, num: int) -> tuple[int, int, int, int, int, int]:
    width = max(1, scaled_round(frame.width, num))
    height = max(1, scaled_round(frame.height, num))
    # Prefer a centered horizontal axis and a feet origin at Y=48.  Shift the
    # canvas origin before considering extra scaling: the stored anchor keeps
    # world alignment even when an extended limb needs an asymmetric canvas.
    left = min(max(24 - scaled_round(frame.ax, num), 0), 48 - width)
    top = min(max(48 - scaled_round(frame.ay, num), 0), 48 - height)
    ax = left + scaled_round(frame.ax, num)
    ay = top + scaled_round(frame.ay, num)
    return width, height, left, top, ax, ay


def projectile_geometry(frame: SourceFrame, num: int) -> tuple[int, int, int, int, int, int]:
    width = max(1, scaled_round(frame.width, num))
    height = max(1, scaled_round(frame.height, num))
    left = min(max(16 - scaled_round(frame.ax, num), 0), 32 - width)
    top = min(max(16 - scaled_round(frame.ay, num), 0), 32 - height)
    return width, height, left, top, left + scaled_round(frame.ax, num), top + scaled_round(frame.ay, num)


def common_scale(frames: list[SourceFrame], category: int) -> int:
    """Find the largest animation-uniform scale no greater than one half."""
    limit = SCALE_BASE // 2
    for frame in frames:
        canvas = 48 if category == FORMAT_ACTION else 32
        # Origin shifting handles asymmetric anchors.  Only dimensions larger
        # than the canvas itself require dropping below nearest-neighbor half.
        constraints = [canvas / frame.width, canvas / frame.height]
        limit = min(limit, int(min(constraints) * SCALE_BASE))
    limit = max(1, min(SCALE_BASE // 2, limit))
    geometry = action_geometry if category == FORMAT_ACTION else projectile_geometry
    canvas = 48 if category == FORMAT_ACTION else 32
    while limit > 1:
        valid = True
        for frame in frames:
            width, height, left, top, _, _ = geometry(frame, limit)
            if left < 0 or top < 0 or left + width > canvas or top + height > canvas:
                valid = False
                break
        if valid:
            break
        limit -= 1
    for frame in frames:
        width, height, left, top, _, _ = geometry(frame, limit)
        if left < 0 or top < 0 or left + width > canvas or top + height > canvas:
            raise ValueError(f"Unable to fit source frame {frame.index} without clipping")
    return limit


def encode_4bpp(image: Image.Image) -> bytes:
    """Encode row-major 8x8 characters in SNES bitplane order."""
    if image.mode != "P" or image.width % 8 or image.height % 8:
        raise ValueError("4bpp input must be an indexed image with 8-pixel-multiple dimensions")
    pixels = image.tobytes()
    output = bytearray()
    for tile_y in range(0, image.height, 8):
        for tile_x in range(0, image.width, 8):
            planes = [[0] * 8 for _ in range(4)]
            for y in range(8):
                row = (tile_y + y) * image.width + tile_x
                for x in range(8):
                    value = pixels[row + x]
                    if value > 15:
                        raise ValueError("4bpp pixel index exceeds 15")
                    for bit in range(4):
                        planes[bit][y] |= ((value >> bit) & 1) << (7 - x)
            for y in range(8):
                output.extend((planes[0][y], planes[1][y]))
            for y in range(8):
                output.extend((planes[2][y], planes[3][y]))
    return bytes(output)


def pad8(value: int) -> int:
    return (value + 7) & ~7


def descriptor_label(character: int, pose: int) -> str:
    if pose == 51:
        return f"WW_Ending_{ROSTER[character].title()}_{POSE_NAMES[pose]}"
    if character == 0 and pose in ALPHA_POSES and pose != 34:
        return f"WW_Akuma_{POSE_NAMES[pose]}"
    if character == 0 and pose in UI_POSES:
        return f"WW_UI_{POSE_NAMES[pose]}"
    return f"WW_{ROSTER[character]}_{POSE_NAMES[pose]}"


def oam_table(width_tiles: int, height_tiles: int, left: bool) -> list[str]:
    lines = []
    for y in range(height_tiles):
        row = []
        for x in range(width_tiles):
            source_x = width_tiles - 1 - x if left else x
            tile = y * width_tiles + source_x
            row.append(f"{x * 8},{y * 8},{tile},${40 if left else 0:02X}")
        lines.append("    db " + ", ".join(row))
    return lines


def graphics_chunks(
    animations: OrderedDict[tuple[int, int], list[ConvertedFrame]],
) -> dict[int, list[dict]]:
    """Pack whole frames into <=32 KiB file ranges for LoROM-safe incbins."""
    result = {}
    for category, base_label in enumerate(CATEGORY_LABELS):
        items = sorted(
            (frame for group in animations.values() for frame in group if frame.format == category),
            key=lambda frame: frame.graphics_offset,
        )
        chunks = []
        current = None
        for frame in items:
            size = (frame.image.width // 8) * (frame.image.height // 8) * 32
            if size > BANK_SIZE:
                raise ValueError(f"Frame {frame.source.index} is larger than one LoROM bank")
            if current is None or frame.graphics_offset + size - current["start"] > BANK_SIZE:
                current = {
                    "start": frame.graphics_offset, "end": frame.graphics_offset,
                    "label": f"{base_label}_{len(chunks):03d}",
                }
                chunks.append(current)
            current["end"] = frame.graphics_offset + size
        for chunk in chunks:
            chunk["size"] = chunk["end"] - chunk["start"]
        result[category] = chunks
    return result


def render_asm(
    animations: OrderedDict[tuple[int, int], list[ConvertedFrame]],
    palettes: OrderedDict[str, Palette],
    frame_count: int,
    metadata_size: int,
    palette_size: int,
    incbin_prefix: str,
) -> tuple[str, int, dict]:
    chunks = graphics_chunks(animations)

    def graphics_pointer(frame: ConvertedFrame) -> str:
        for chunk in chunks[frame.format]:
            if chunk["start"] <= frame.graphics_offset < chunk["end"]:
                relative = frame.graphics_offset - chunk["start"]
                return chunk["label"] + (f"+${relative:X}" if relative else "")
        raise AssertionError(f"No bank chunk for source frame {frame.source.index}")

    descriptor_count = len(animations)
    assembly_bytes = (
        5 + 12 * 3 + 12 * 3 + 12 * 3 + 12 * 53 * 3
        + descriptor_count * DESCRIPTOR_SIZE + frame_count * 3
        + 36 * 4 * 2 + 16 * 4 * 2
    )

    def aligned_file(offset: int) -> int:
        return (offset + BANK_SIZE - 1) & -BANK_SIZE

    def snes_address(offset: int) -> int:
        return ((offset >> 15) << 16) | 0x8000 | (offset & 0x7FFF)

    file_pc = (ASSET_LAYOUT_BASE >> 16) * BANK_SIZE + (ASSET_LAYOUT_BASE & 0x7FFF)
    file_pc += assembly_bytes
    file_pc = aligned_file(file_pc)
    metadata_address = snes_address(file_pc)
    if metadata_size + palette_size > BANK_SIZE:
        raise ValueError("Metadata and palettes exceed their dedicated LoROM bank")
    file_pc += metadata_size + palette_size
    for category_chunks in chunks.values():
        for chunk in category_chunks:
            file_pc = aligned_file(file_pc)
            chunk["address"] = snes_address(file_pc)
            file_pc += chunk["size"]
    logical_payload = metadata_size + palette_size + sum(
        chunk["size"] for category_chunks in chunks.values() for chunk in category_chunks
    )
    rom_file_size = file_pc
    layout = {
        "assumed_base": ASSET_LAYOUT_BASE,
        "rom_span_bytes": rom_file_size - BASE_ROM_SIZE,
        "rom_file_size": rom_file_size,
        "alignment_padding_bytes": rom_file_size - BASE_ROM_SIZE - assembly_bytes - logical_payload,
        "end_address": snes_address(file_pc),
        "chunks": [
            {"format": FORMAT_NAMES[category], **chunk}
            for category, category_chunks in chunks.items() for chunk in category_chunks
        ],
    }

    lines = [
        "; Generated by rom/make_roster_graphics.py from local/sf2/sprites.dat.",
        "; All pointers are 24-bit SNES addresses.  Missing animations use dl 0.",
        "; See rom/GRAPHICS.md for descriptor, metadata, palette and DMA layouts.",
        "",
        f"!WW_RosterFormatAction = {FORMAT_ACTION}",
        f"!WW_RosterFormatProjectile = {FORMAT_PROJECTILE}",
        f"!WW_RosterFormatPortrait = {FORMAT_PORTRAIT}",
        f"!WW_RosterFormatUI = {FORMAT_UI}",
        f"!WW_RosterFormatEffect = {FORMAT_EFFECT}",
        f"!WW_RosterFormatEnding = {FORMAT_ENDING}",
        "!WW_RosterDescriptorSize = 13",
        "!WW_RosterMetadataSize = 16",
        "!WW_RosterActionFrameSize = 1152",
        "!WW_RosterActionTileCount = 36",
        "!WW_RosterProjectileFrameSize = 512",
        "!WW_RosterProjectileTileCount = 16",
        "",
        f"org ${ASSET_LAYOUT_BASE:06X}",
        "WW_RosterAssetVersion: db 1",
        f"WW_RosterFrameCount: dw {frame_count}",
        "WW_RosterCharacterCount: db 12",
        "WW_RosterPoseCount: db 53",
        "WW_RosterPoseTablePointers:",
        "    dl " + ", ".join(f"WW_{name}_PoseTable" for name in ROSTER),
        "WW_RosterPalettePointers:",
        "    dl " + ", ".join(
            f"WW_RosterPalettes+${palettes[f'fighter_{name.lower()}'].offset:X}" for name in ROSTER
        ),
        "WW_RosterEndingPalettePointers:",
    ]
    for character, name in enumerate(ROSTER):
        key = f"ending_{name.lower()}"
        value = f"WW_RosterPalettes+${palettes[key].offset:X}" if key in palettes else "0"
        lines.append(f"    dl {value} ; {name}")

    for character, name in enumerate(ROSTER):
        lines.extend(("", f"WW_{name}_PoseTable:"))
        for pose, pose_name in enumerate(POSE_NAMES):
            frames = animations.get((character, pose))
            target = descriptor_label(character, pose) if frames else "0"
            lines.append(f"    dl {target} ; {pose:02d} {pose_name}")

    for (character, pose), frames in animations.items():
        label = descriptor_label(character, pose)
        duration = sum(frame.source.ticks for frame in frames)
        palette = palettes[frames[0].palette_key]
        lines.extend((
            "",
            f"{label}:",
            f"    db {len(frames)}, {frames[0].format} ; frame count, format {FORMAT_NAMES[frames[0].format]}",
            f"    dw {duration} ; total source ticks",
            f"    dl {label}_FramePointers",
            f"    dl WW_RosterFrameMetadata+${frames[0].meta_offset:X}",
            f"    dl WW_RosterPalettes+${palette.offset:X} ; {palette.key}",
            f"{label}_FramePointers:",
        ))
        for frame in frames:
            lines.append(
                f"    dl {graphics_pointer(frame)} "
                f"; source {frame.source.index}, {frame.source.ticks} ticks"
            )

    lines.extend(("", "; Per-animation descriptors above select palettes by 24-bit pointer."))

    lines.extend(("", "WW_RosterActionOamRight:"))
    lines.extend(oam_table(6, 6, False))
    lines.append("WW_RosterActionOamLeft:")
    lines.extend(oam_table(6, 6, True))
    lines.append("WW_RosterProjectileOamRight:")
    lines.extend(oam_table(4, 4, False))
    lines.append("WW_RosterProjectileOamLeft:")
    lines.extend(oam_table(4, 4, True))

    lines.extend((
        "",
        "; Tables and every individual frame stay inside one 32 KiB LoROM bank.",
        f"org ${metadata_address:06X}",
        f"WW_RosterFrameMetadata: incbin \"{incbin_prefix}/roster_frames.meta\"",
        f"WW_RosterPalettes: incbin \"{incbin_prefix}/roster_palettes.bin\"",
    ))
    for category, (base_label, filename) in enumerate(zip(CATEGORY_LABELS, CATEGORY_FILES)):
        for index, chunk in enumerate(chunks[category]):
            lines.append(f"org ${chunk['address']:06X}")
            if index == 0:
                lines.append(f"{base_label}:")
            lines.append(f"{chunk['label']}:")
            lines.append(
                f"    incbin \"{incbin_prefix}/{filename}\":${chunk['start']:X}..${chunk['end']:X}"
            )
    lines.extend(("WW_RosterGraphicsEnd:", ""))

    return "\n".join(lines), assembly_bytes, layout


def display_image(frame: ConvertedFrame) -> Image.Image:
    rgba = frame.image.convert("RGBA")
    alpha = frame.image.convert("L").point(lambda value: 0 if value == 0 else 255)
    rgba.putalpha(alpha)
    return rgba


def contact_sheet(path: Path, title: str, frames: list[ConvertedFrame], scale: int, columns: int) -> None:
    if not frames:
        return
    label_height = 24
    header_height = 32
    cell_width = max(frame.image.width for frame in frames) * scale + 8
    cell_height = max(frame.image.height for frame in frames) * scale + label_height + 8
    columns = min(columns, len(frames))
    rows = math.ceil(len(frames) / columns)
    sheet = Image.new("RGB", (columns * cell_width, header_height + rows * cell_height), (24, 27, 38))
    draw = ImageDraw.Draw(sheet)
    draw.text((8, 10), title, fill=(240, 240, 240))
    for i, frame in enumerate(frames):
        col = i % columns
        row = i // columns
        x0 = col * cell_width
        y0 = header_height + row * cell_height
        # A checkerboard makes transparent padding and the 48x48 canvas visible.
        for y in range(y0 + 4, y0 + 4 + frame.image.height * scale, 8):
            for x in range(x0 + 4, x0 + 4 + frame.image.width * scale, 8):
                color = (48, 52, 65) if ((x + y) // 8) & 1 else (38, 42, 54)
                draw.rectangle((x, y, min(x + 7, x0 + 4 + frame.image.width * scale - 1),
                                min(y + 7, y0 + 4 + frame.image.height * scale - 1)), fill=color)
        image = display_image(frame).resize((frame.image.width * scale, frame.image.height * scale), Image.Resampling.NEAREST)
        sheet.paste(image, (x0 + 4, y0 + 4), image)
        source = frame.source
        caption = f"p{source.pose:02d} f{source.index} {source.ticks}t {frame.scale_num}/{frame.scale_den}"
        draw.text((x0 + 4, y0 + 6 + frame.image.height * scale), caption, fill=(220, 220, 220))
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path)


def write_previews(out: Path, converted: list[ConvertedFrame]) -> list[str]:
    preview_dir = out / "previews"
    written = []
    for character, name in enumerate(ROSTER):
        frames = [f for f in converted if f.source.character == character and f.format == FORMAT_ACTION
                  and f.palette_key != "alpha"]
        filename = f"{name.lower()}_action.png"
        contact_sheet(preview_dir / filename, f"{name} - 48x48 action frames", frames, 2, 10)
        if frames:
            written.append(f"previews/{filename}")
    groups = (
        ("projectiles.png", "Projectiles - 32x32 / at most 16 OBJs", lambda f: f.format == FORMAT_PROJECTILE, 3, 12),
        ("portraits.png", "Selection, continue and HUD portraits", lambda f: f.format == FORMAT_PORTRAIT, 2, 8),
        ("ui_map.png", "Character-select map", lambda f: f.format == FORMAT_UI and f.source.pose == 16, 2, 1),
        ("ui_fonts.png", "Small, large and HUD fonts", lambda f: f.format == FORMAT_UI and f.source.pose in (17, 18, 42), 3, 12),
        ("ui_marks.png", "KO, title, continue and round marks", lambda f: f.format == FORMAT_UI and f.source.pose not in (16, 17, 18, 42), 1, 3),
        ("alpha.png", "Akuma and Alpha action art", lambda f: f.palette_key == "alpha" and f.format == FORMAT_ACTION, 2, 10),
        ("effects.png", "Large Alpha effects (not single-pose OBJ sets)", lambda f: f.format == FORMAT_EFFECT, 1, 8),
        ("endings.png", "Available ending illustrations", lambda f: f.format == FORMAT_ENDING, 1, 3),
    )
    for filename, title, predicate, scale, columns in groups:
        frames = [f for f in converted if predicate(f)]
        contact_sheet(preview_dir / filename, title, frames, scale, columns)
        if frames:
            written.append(f"previews/{filename}")
    return written


def convert(source: Path, out: Path, asm_path: Path, previews: bool) -> dict:
    frames, source_hash = read_wwa1(source)
    animations_source: OrderedDict[tuple[int, int], list[SourceFrame]] = OrderedDict()
    for frame in frames:
        animations_source.setdefault((frame.character, frame.pose), []).append(frame)
    # UI glyphs are interleaved by pose in WWA1.  Conversion order groups them
    # by animation so each descriptor can point at one contiguous metadata run;
    # the original source record index remains in every metadata entry.
    conversion_order = [frame for group in animations_source.values() for frame in group]

    palette_groups: OrderedDict[str, list[SourceFrame]] = OrderedDict()
    for frame in frames:
        palette_groups.setdefault(palette_key(frame), []).append(frame)
    palettes: OrderedDict[str, Palette] = OrderedDict()
    palette_data = bytearray()
    for key, group in palette_groups.items():
        palette = build_palette(key, group)
        palette.offset = len(palette_data)
        palettes[key] = palette
        palette_data.extend(struct.pack("<H", 0))
        for color in palette.colors:
            palette_data.extend(struct.pack("<H", snes_color(color)))

    scales: dict[tuple[int, int], int] = {}
    for key, group in animations_source.items():
        category = category_for(key[1])
        if category in (FORMAT_ACTION, FORMAT_PROJECTILE):
            scales[key] = common_scale(group, category)

    graphics = [bytearray() for _ in CATEGORY_FILES]
    metadata = bytearray()
    converted: list[ConvertedFrame] = []
    animations: OrderedDict[tuple[int, int], list[ConvertedFrame]] = OrderedDict()
    for frame in conversion_order:
        category = category_for(frame.pose)
        key = (frame.character, frame.pose)
        if category in (FORMAT_ACTION, FORMAT_PROJECTILE):
            base_num = scales[key]
            scale_num, scale_den = reduced_scale(base_num)
            if category == FORMAT_ACTION:
                width, height, left, top, ax, ay = action_geometry(frame, base_num)
                canvas = Image.new("RGBA", (48, 48))
            else:
                width, height, left, top, ax, ay = projectile_geometry(frame, base_num)
                canvas = Image.new("RGBA", (32, 32))
        else:
            scale_num, scale_den = (1, 2) if category in (FORMAT_PORTRAIT, FORMAT_EFFECT) and frame.pose != 40 else (1, 1)
            width = max(1, scaled_round(frame.width, scale_num, scale_den))
            height = max(1, scaled_round(frame.height, scale_num, scale_den))
            left = top = 0
            ax = scaled_round(frame.ax, scale_num, scale_den)
            ay = scaled_round(frame.ay, scale_num, scale_den)
            canvas = Image.new("RGBA", (pad8(width), pad8(height)))
        resized = frame.image.resize((width, height), Image.Resampling.NEAREST)
        canvas.alpha_composite(resized, (left, top))
        if not (0 <= ax <= 255 and 0 <= ay <= 255):
            raise ValueError(f"Scaled anchor does not fit metadata at source frame {frame.index}")
        key_palette = palette_key(frame)
        indexed = quantize_rgba(canvas, palettes[key_palette])
        graphics_offset = len(graphics[category])
        graphics[category].extend(encode_4bpp(indexed))
        flags = (1 if frame.pose in ONCE_POSES else 0) | (2 if category == FORMAT_ENDING else 0)
        if frame.ticks > 255:
            raise ValueError(f"Frame {frame.index} duration does not fit the one-byte metadata field")
        meta_offset = len(metadata)
        metadata.extend(struct.pack(
            "<8B4H", frame.ticks, category, canvas.width // 8, canvas.height // 8,
            ax, ay, scale_num, scale_den, frame.index, frame.width, frame.height, flags,
        ))
        item = ConvertedFrame(
            frame, category, key_palette, indexed, scale_num, scale_den,
            ax, ay, graphics_offset, meta_offset,
        )
        converted.append(item)
        animations.setdefault(key, []).append(item)

    # Hard output invariants: fixed action/projectile formats and 36/16-OBJ budgets.
    if len(graphics[FORMAT_ACTION]) != sum(
        len(group) for key, group in animations.items() if category_for(key[1]) == FORMAT_ACTION
    ) * ACTION_SIZE:
        raise AssertionError("Action stream size mismatch")
    if len(graphics[FORMAT_PROJECTILE]) != sum(
        len(group) for key, group in animations.items() if category_for(key[1]) == FORMAT_PROJECTILE
    ) * PROJECTILE_SIZE:
        raise AssertionError("Projectile stream size mismatch")
    if len(metadata) != len(frames) * META_SIZE:
        raise AssertionError("Metadata stream size mismatch")
    for character in range(12):
        if (character, 0) not in animations or (character, 15) not in animations:
            raise AssertionError(f"Roster member {character} lacks idle or portrait art")

    incbin_prefix = out.relative_to(ROOT).as_posix() if out.is_relative_to(ROOT) else out.as_posix()
    asm_text, assembly_bytes, layout = render_asm(
        animations, palettes, len(frames), len(metadata), len(palette_data), incbin_prefix
    )
    payload_size = sum(map(len, graphics)) + len(metadata) + len(palette_data)
    rom_size = BASE_ROM_SIZE + layout["rom_span_bytes"]
    if rom_size > LOROM_LIMIT:
        raise ValueError(f"Native asset payload exceeds 4 MiB LoROM budget: {rom_size} bytes")

    out.mkdir(parents=True, exist_ok=True)
    for filename, data in zip(CATEGORY_FILES, graphics):
        (out / filename).write_bytes(data)
    (out / "roster_frames.meta").write_bytes(metadata)
    (out / "roster_palettes.bin").write_bytes(palette_data)
    asm_path.write_text(asm_text)
    preview_files = write_previews(out, converted) if previews else []

    binary_files = list(CATEGORY_FILES) + ["roster_frames.meta", "roster_palettes.bin"]
    hashes = {
        filename: {"size": (out / filename).stat().st_size,
                   "sha256": hashlib.sha256((out / filename).read_bytes()).hexdigest()}
        for filename in binary_files
    }
    animation_manifest = []
    for (character, pose), group in animations.items():
        animation_manifest.append({
            "character": character, "character_name": ROSTER[character], "pose": pose,
            "pose_name": POSE_NAMES[pose], "label": descriptor_label(character, pose),
            "frames": len(group), "ticks": sum(f.source.ticks for f in group),
            "format": FORMAT_NAMES[group[0].format], "palette": group[0].palette_key,
            "metadata_offset": group[0].meta_offset,
            "graphics_offsets": [f.graphics_offset for f in group],
        })
    ending_missing = [ROSTER[c] for c in range(12) if (c, 51) not in animations]
    manifest = {
        "format_version": 1,
        "source": str(source.relative_to(ROOT)) if source.is_relative_to(ROOT) else str(source),
        "source_sha256": source_hash,
        "source_frames": len(frames),
        "animations": animation_manifest,
        "palettes": {
            key: {"offset": palette.offset, "colors_bgr15": [snes_color(c) for c in palette.colors],
                  "colors_rgb": [list(c) for c in palette.colors]}
            for key, palette in palettes.items()
        },
        "files": hashes,
        "assembly_bytes": assembly_bytes,
        "payload_bytes": payload_size,
        "bank_layout": layout,
        "base_rom_bytes": BASE_ROM_SIZE,
        "estimated_rom_bytes": rom_size,
        "lorom_limit_bytes": LOROM_LIMIT,
        "lorom_bytes_remaining": LOROM_LIMIT - rom_size,
        "action_tiles_per_frame": 36,
        "projectile_tiles_per_frame": 16,
        "previews": preview_files,
        "missing_ending_records": ending_missing,
    }
    (out / "roster_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def self_test() -> None:
    """Exercise SNES bitplane order and action anchor/scale geometry."""
    image = Image.new("P", (8, 8))
    image.putdata([1 if x == 0 else 0 for _ in range(8) for x in range(8)])
    encoded = encode_4bpp(image)
    assert encoded == b"".join(bytes((0x80, 0)) for _ in range(8)) + bytes(16)
    rgba = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    for y in range(16):
        rgba.putpixel((8, y), (255, 0, 0, 255))
    source = SourceFrame(0, 0, 0, 4, 16, 16, 8, 16, rgba)
    assert common_scale([source], FORMAT_ACTION) == SCALE_BASE // 2
    width, height, left, top, ax, ay = action_geometry(source, SCALE_BASE // 2)
    assert (width, height, left, top, ax, ay) == (8, 8, 20, 40, 24, 48)
    print("Self-tests passed: 4bpp planes, half-size scale and action anchor geometry.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sprites", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--asm", type=Path, default=ASM_PATH)
    parser.add_argument("--skip-previews", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    manifest = convert(args.sprites.expanduser().resolve(), args.out.expanduser().resolve(),
                       args.asm.expanduser().resolve(), not args.skip_previews)
    print(f"Converted {manifest['source_frames']} WWA1 frames in {len(manifest['animations'])} animations.")
    print(f"Payload: {manifest['payload_bytes']:,} bytes; assembly tables/OAM: {manifest['assembly_bytes']:,} bytes; "
          f"bank alignment: {manifest['bank_layout']['alignment_padding_bytes']:,} bytes.")
    print(f"Estimated 512 KiB base + assets: {manifest['estimated_rom_bytes']:,} / {LOROM_LIMIT:,} bytes "
          f"({manifest['lorom_bytes_remaining']:,} bytes free).")
    print(f"Wrote {args.asm} and native data/previews under {args.out}.")
    if manifest["missing_ending_records"]:
        print("No WWA1 ending illustration records for: " + ", ".join(manifest["missing_ending_records"]))


if __name__ == "__main__":
    main()
