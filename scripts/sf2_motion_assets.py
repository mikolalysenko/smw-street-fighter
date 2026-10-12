"""Read the pinned original arcade rips, retaining their fighter origins.

SFF v1 stores indexed PCX images with signed axes and shared palettes. The
palette-sharing flag is authoritative: compressed pixel data can also contain
0x0c exactly 769 bytes from the end, so it is not a palette-presence test.
"""
import hashlib
import io
import json
from pathlib import Path
import struct
from urllib.request import urlopen
from zipfile import ZipFile
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def read_sff(data):
    if data[:12] != b'ElecbyteSpr\0' or data[15] != 1:
        raise ValueError('Expected an SFF v1 sprite archive')
    count, offset = struct.unpack_from('<II', data, 20)
    if count > 10000:
        raise ValueError('Invalid SFF frame count')
    indexed, sprites, palette = [], {}, None
    for _ in range(count):
        following, size, ax, ay, group, number, link = struct.unpack_from('<IIhhHHH', data, offset)
        if size:
            pcx = data[offset+32:offset+32+size]
            if len(pcx) != size:
                raise ValueError('Truncated SFF sprite')
            if data[offset+18]:
                if palette is None:
                    raise ValueError('Missing shared SFF palette')
                pcx += palette
            else:
                if pcx[-769:-768] != b'\x0c':
                    raise ValueError('Missing PCX palette')
                palette = pcx[-768:]
            im = Image.open(io.BytesIO(pcx))
            alpha = Image.frombytes('L', im.size, bytes(255 if p else 0 for p in im.tobytes()))
            im = im.convert('RGBA')
            im.putalpha(alpha)
        else:
            im = indexed[link].copy()
        indexed.append(im)
        sprites[group, number] = im, ax, ay
        offset = following
    return sprites


def load_sprites():
    source = json.loads((ROOT/'config/sf2-motion-source.json').read_text())
    path = ROOT/'local/sf2-motion/sf2ww-sprite.zip'
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(urlopen(source['url'], timeout=30).read())
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != source['sha256']:
        raise ValueError('Original SF2 animation source hash mismatch')
    with ZipFile(io.BytesIO(data)) as archive:
        return read_sff(archive.read(source['member']))


def pack_frame(sprites, character, pose, frame):
    im, ax, ay = sprites[tuple(frame['sprite'])]
    layers = [(im, ax, ay)]
    group, number = frame['sprite']
    if character == 9 and (group, number+10000) in sprites:
        # The claw is a separate hardware sprite, with its own original axis.
        layers.append(sprites[group, number+10000])
    for layer in frame.get('layers', []):
        overlay, ox, oy = sprites[tuple(layer['sprite'])]
        dx, dy = layer.get('offset', [0, 0])
        layers.append((overlay, ox-dx, oy-dy))
    if len(layers) > 1:
        left = min(-x for _, x, _ in layers)
        top = min(-y for _, _, y in layers)
        right = max(part.width-x for part, x, _ in layers)
        bottom = max(part.height-y for part, _, y in layers)
        im = Image.new('RGBA', (right-left, bottom-top))
        for part, x, y in layers:
            im.alpha_composite(part, (-x-left, -y-top))
        ax, ay = -left, -top
    if frame.get('flip'):
        im = im.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        ax = im.width-ax
    if 'offset' in frame:
        dx, dy = frame['offset']
        ax -= dx
        ay -= dy
    if frame.get('center'):
        ax, ay = im.width//2, im.height//2
    else:
        # SF2's origin is a few pixels above the feet on its perspective floor.
        # Keep airborne pivots relative to one constant fighter baseline;
        # grounded poses below need an exact platform contact instead.
        order = (0,1,5,3,4,2,6,7,10,11,9,8)
        standing, _, ground_y = sprites[0, order[character]*100]
        ay += standing.height-ground_y
        if (pose in (0, 1, 3, 4, 5, 6, 10, 13, 20, 23, 24)
                or (character == 8 and pose in (7, 9))
                or (character == 6 and pose in (8, 37))):
            # Grounded art is sampled at half size by the game. A toe that
            # exists only on an odd source row can otherwise disappear and
            # leave a visible gap. Align the lowest sampled opaque pixel,
            # including Vega's separate claw layer, to the platform plane.
            bottom = max(y//2 for y in range(0, im.height, 2)
                         if any(im.getpixel((x, y))[3] for x in range(0, im.width, 2)))
            ay = (bottom+1)*2
    # WWA1 uses unsigned origins. Pad instead of moving the original axis.
    left, top = max(0, -ax), max(0, -ay)
    if left or top:
        padded = Image.new('RGBA', (im.width+left, im.height+top))
        padded.paste(im, (left, top))
        im, ax, ay = padded, ax+left, ay+top
    if max(im.width, im.height, ax, ay) > 512:
        raise ValueError('Sprite exceeds WWA1 bounds')
    pixels = b''.join(struct.pack('<I', 0xff000000 | r<<16 | g<<8 | b if a else 0)
                      for r, g, b, a in im.getdata())
    return struct.pack('<7H', character, pose, frame['ticks'], im.width, im.height, ax, ay)+pixels
