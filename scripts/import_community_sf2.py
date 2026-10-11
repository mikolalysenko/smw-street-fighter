#!/usr/bin/env python3
"""Pack the pinned community SF2 rips into an SDL-independent sprite file."""
from pathlib import Path
import hashlib
import json
import shutil
import struct
from urllib.request import urlopen
from concurrent.futures import ThreadPoolExecutor
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main():
    source = ROOT / 'local/community-sf2'
    manifest = json.loads((ROOT / 'config/community-assets.json').read_text())
    def fetch(entry):
        path = source / entry['path']
        if not path.exists():
            url = manifest['url'].replace('github.com', 'raw.githubusercontent.com') + '/' + manifest['commit'] + '/' + entry['path']
            data = urlopen(url, timeout=30).read()
            if hashlib.sha256(data).hexdigest() != entry['sha256']:
                raise ValueError('Source hash mismatch: ' + entry['path'])
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        if hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']:
            raise ValueError('Source hash mismatch: ' + entry['path'])
    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(fetch, manifest['files']))
    out = ROOT / 'local/sf2'
    (out / 'audio').mkdir(parents=True, exist_ok=True)
    sheets = {}
    records = []
    mapping = json.loads((ROOT / 'config/sf2-sprites.json').read_text())
    for character, entry in enumerate(mapping['characters']):
        for pose, animation in enumerate(entry['poses']):
            name = animation['sheet']
            if name not in sheets:
                sheets[name] = Image.open(source / 'Game/assets/images/sprites/characters' / name).convert('RGBA')
            for frame in animation['frames']:
                x, y, w, h = frame['rect']
                im = sheets[name].crop((x,y,x+w,y+h))
                # Feet locate the body origin despite extended punches/kicks.
                feet = [px for py in range(max(0,h-3),h) for px in range(w) if im.getpixel((px,py))[3]>128]
                ax = round(sum(feet)/len(feet)) if feet else w//2
                ay = h
                if pose == 14: ax,ay=w//2,h//2
                pixels = b''.join(struct.pack('<I', (0xff000000|(r<<16)|(g<<8)|b) if a>=128 else 0) for r,g,b,a in im.getdata())
                records.append(struct.pack('<7H',character,pose,frame['ticks'],w,h,ax,ay)+pixels)
    portraits=Image.open(source / 'Game/assets/images/ui/portraits.png').convert('RGBA')
    for character,rect in enumerate(((0,0,105,141),(124,0,232,141),(247,183,350,315),(0,173,105,315))):
        im=portraits.crop(rect);w,h=im.size
        pixels=b''.join(struct.pack('<I',(0xff000000|(r<<16)|(g<<8)|b) if a>=128 else 0) for r,g,b,a in im.getdata())
        records.append(struct.pack('<7H',character,15,60,w,h,w//2,h)+pixels)
    im=Image.open(source / 'Game/assets/images/ui/character_selection.png').convert('RGBA').crop((0,0,192,112))
    pixels=b''.join(struct.pack('<I',(0xff000000|(r<<16)|(g<<8)|b) if a>=128 else 0) for r,g,b,a in im.getdata())
    records.append(struct.pack('<7H',0,16,60,192,112,0,0)+pixels)
    (out / 'sprites.dat').write_bytes(b'WWA1'+struct.pack('<H',len(records))+b''.join(records))
    # These are original ripped effects, including the FM menu sound.
    sounds = {
        'swing':'effects/unhit_fist', 'hit':'effects/high_fist', 'hurt':'voices/ei',
        'ko':'voices/ryu-ken_uggh', 'special':'voices/ryu_ken_hadouken',
        'super':'effects/blocked_hadouken', 'block':'effects/72H', 'select':'effects/move_menu',
        'hadouken':'voices/ryu_ken_hadouken', 'shoryuken':'voices/ryu-ken_shoryuken',
        'tatsumaki':'voices/ryu-ken_kick', 'honda':'voices/honda_dufGoi',
        'sonic':'voices/guile_sonicBoom', 'bird':'voices/chunLi_kick',
        'chun_hurt':'voices/chunLi_yap', 'chun_ko':'voices/chunLi_scream',
        'confirm':'effects/select_menu_option', 'you':'voices/announcer_you', 'lose':'voices/announcer_lose',
    }
    for dest, src in sounds.items():
        shutil.copy2(source / 'Game/assets/sfx' / (src+'.wav'), out / 'audio' / (dest+'.wav'))
    print(f'Packed {len(records)} original sprite frames and {len(sounds)} original WAV sounds into {out}')


if __name__ == '__main__':
    main()
