#!/usr/bin/env python3
"""Steam library artwork from lossless gameplay captures. Requires Pillow.

Run dev/media/capture.py first; frames come from build/media/*.bgra and the
artwork lands in media/steam/ (Steam's grid file names are applied at install).
"""
from pathlib import Path
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'build/media'
OUT = ROOT / 'media/steam'
OUT.mkdir(parents=True, exist_ok=True)
NEAREST = Image.Resampling.NEAREST


def frame(name, number):
    with (WORK / (name + '.bgra')).open('rb') as stream:
        stream.seek(number * 256 * 224 * 4)
        return Image.frombytes('RGBA', (256, 224), stream.read(256 * 224 * 4), 'raw', 'BGRA').convert('RGB')


def scaled(image, factor):
    return image.resize((image.width * factor, image.height * factor), NEAREST)


def center_crop(image, size):
    left, top = (image.width - size[0]) // 2, (image.height - size[1]) // 2
    return image.crop((left, top, left + size[0], top + size[1]))


# The in-game title logo over black: key out the background, then restore a
# one-pixel dark outline so the letters stay legible over any library backdrop.
title = frame('title', 180).crop((18, 34, 236, 124))
mask = title.convert('L').point(lambda v: 255 if v > 24 else 0)
outline = mask.filter(ImageFilter.MaxFilter(3))
logo = Image.new('RGBA', title.size, (0, 0, 0, 0))
logo.paste((16, 16, 32, 255), mask=outline)
logo.paste(title, mask=mask)
logo = logo.crop(outline.getbbox())
scaled(logo, 5).save(OUT / 'logo.png', optimize=True)

# Wide capsule: the attract-mode title screen inside its wooden border.
frame('title', 20).crop((16, 30, 240, 135)).resize((920, 430), NEAREST).save(OUT / 'wide.png', optimize=True)

# Portrait capsule: Ryu in the forest stage under the title logo.
portrait = scaled(frame('bonk', 85).crop((36, 54, 136, 204)), 6)
mark = logo.resize((logo.width * 5 // 2, logo.height * 5 // 2), NEAREST)
portrait.paste(mark, ((600 - mark.width) // 2, 40), mark)
portrait.save(OUT / 'portrait.png', optimize=True)

# Hero banner sits behind the logo, so it is pure playfield with no HUD.
hero = scaled(frame('psycho-crusher', 285).crop((0, 102, 256, 180)), 8)
center_crop(hero, (1920, 620)).save(OUT / 'hero.png', optimize=True)

# Icon: the HUD fighter portrait, squared on navy.
face = frame('bonk', 85).crop((5, 27, 29, 57))
icon = Image.new('RGB', (32, 32), (16, 16, 32))
icon.paste(face, ((32 - face.width) // 2, (32 - face.height) // 2))
scaled(icon, 8).save(OUT / 'icon.png', optimize=True)

for path in sorted(OUT.glob('*.png')):
    print(path.relative_to(ROOT), Image.open(path).size)
