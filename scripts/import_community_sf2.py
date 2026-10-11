#!/usr/bin/env python3
"""Pack the pinned community SF2 rips into an SDL-independent sprite file."""
from pathlib import Path
import hashlib
import json
import shutil
import struct
import subprocess
from urllib.request import urlopen
from concurrent.futures import ThreadPoolExecutor
from PIL import Image, ImageSequence

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
    for entry in json.loads((ROOT/'config/animation-assets.json').read_text()):
        path=source/'Game/assets/images/sprites/characters'/entry['name']
        if not path.exists():path.write_bytes(urlopen(entry['url'],timeout=30).read())
        if hashlib.sha256(path.read_bytes()).hexdigest()!=entry['sha256']:
            raise ValueError('Animation hash mismatch: '+entry['name'])
    out = ROOT / 'local/sf2'
    (out / 'audio').mkdir(parents=True, exist_ok=True)
    sheets = {}
    records = []
    mapping = json.loads((ROOT / 'config/sf2-sprites.json').read_text())
    for character, entry in enumerate(mapping['characters']):
        animations=list(enumerate(entry['poses']))
        animations += [(20,entry['victory']),(21,entry['air_punch']),(22,entry['air_kick']),
                       (23,entry['crouch_punch']),(24,entry['crouch_kick']),(25,entry['continue_portrait'])]
        for pose, animation in animations:
            name = animation['sheet']
            if name.endswith('.gif'):
                gif=Image.open(source/'Game/assets/images/sprites/characters'/name)
                for gif_frame in ImageSequence.Iterator(gif):
                    im=gif_frame.convert('RGBA');w,h=im.size
                    if pose==25:
                        # The original WW portraits use a green color key.
                        key=im.getpixel((0,0))[:3]
                        im.putdata([(r,g,b,0 if (r,g,b)==key else a) for r,g,b,a in im.getdata()])
                    ticks=max(1,round(gif_frame.info.get('duration',100)*60/1000))
                    pixels=b''.join(struct.pack('<I',(0xff000000|(r<<16)|(g<<8)|b) if a>=128 else 0) for r,g,b,a in im.getdata())
                    records.append(struct.pack('<7H',character,pose,ticks,w,h,w//2,h)+pixels)
                continue
            if name not in sheets:
                sheets[name] = Image.open(source / 'Game/assets/images/sprites/characters' / name).convert('RGBA')
            for frame in animation['frames']:
                frame_name=frame.get('sheet',name)
                if frame_name not in sheets:
                    sheets[frame_name]=Image.open(source/'Game/assets/images/sprites/characters'/frame_name).convert('RGBA')
                x, y, w, h = frame['rect']
                im = sheets[frame_name].crop((x,y,x+w,y+h))
                # Feet locate the body origin despite extended punches/kicks.
                feet = [px for py in range(max(0,h-3),h) for px in range(w) if im.getpixel((px,py))[3]>128]
                ax = round(sum(feet)/len(feet)) if feet else w//2
                ay = h
                if pose in (0,1,2):ax=w//2
                if pose == 14: ax,ay=w//2,h//2
                if 'anchor' in frame:ax,ay=frame['anchor']
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
    def ui(pose, name, rect, component=False):
        im=Image.open(source/'Game/assets/images/ui'/name).convert('RGBA').crop(rect)
        w,h=im.size
        if component:
            # The II touches neither word, but the bounding rectangle also
            # catches two small fragments of STREET/FIGHTER. Keep its connected
            # silhouette, including the original gold/red fill and dark shadow.
            remaining={(x,y) for y in range(h) for x in range(w) if im.getpixel((x,y))[3]}
            parts=[]
            while remaining:
                pending=[remaining.pop()];part=set(pending)
                while pending:
                    x,y=pending.pop()
                    for point in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
                        if point in remaining:
                            remaining.remove(point);part.add(point);pending.append(point)
                parts.append(part)
            keep=max(parts,key=len)
            for y in range(h):
                for x in range(w):
                    if (x,y) not in keep:im.putpixel((x,y),(0,0,0,0))
        pixels=b''.join(struct.pack('<I',(0xff000000|(r<<16)|(g<<8)|b) if a>=128 else 0) for r,g,b,a in im.getdata())
        records.append(struct.pack('<7H',0,pose,1,w,h,0,0)+pixels)
    # Fixed cells from the original SF2 font sheets; glyphs retain their
    # orange/white highlights and blue drop shadows, with no font smoothing.
    for i in range(36):
        ui(17,'Font_name.png',(i*12,0,(i+1)*12,11))
        # W spans two atlas cells; X through 9 follow the extra cell.
        j=i+(i>=23)
        ui(18,'Font_Big.png',(j*17+(2 if i==22 else 0),0,j*17+(26 if i==22 else 17),15))
    ui(19,'fight_ui.png',(137,0,173,18))
    ui(26,'vs_mode-game_over.png',(71,116,125,170))
    ui(27,'logo2.png',(269,53,337,132),component=True)
    ui(28,'logo2.png',(20,1484,261,1496))
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
    ko=json.loads((ROOT/'config/ko-asset.json').read_text())
    mp3=source/'ko.mp3'
    if not mp3.exists():mp3.write_bytes(urlopen(ko['url'],timeout=30).read())
    if hashlib.sha256(mp3.read_bytes()).hexdigest()!=ko['sha256']:
        raise ValueError('KO announcer source hash mismatch')
    wav=out/'audio/boss_ko.wav'
    if shutil.which('ffmpeg'):
        subprocess.run(['ffmpeg','-y','-hide_banner','-loglevel','error','-i',str(mp3),'-ac','1','-ar','48000',str(wav)],check=True)
    elif shutil.which('afconvert'):
        subprocess.run(['afconvert','-f','WAVE','-d','LEI16@48000','-c','1',str(mp3),str(wav)],check=True)
    else:raise SystemExit('Install ffmpeg to convert the pinned KO announcer clip.')
    # Ogg decoding is done once at import time; the native and WASM hosts
    # share the resulting PCM loop and never need an extra runtime decoder.
    if not shutil.which('ffmpeg'):raise SystemExit('Install ffmpeg to import the original SF2 selection music.')
    for name in ('player_select','continue'):
        music=source/'Game/assets/music'/f'{name}.ogg'
        subprocess.run(['ffmpeg','-y','-hide_banner','-loglevel','error','-i',str(music),
                        '-ac','1','-ar','48000',str(out/'audio'/f'{name}.wav')],check=True)
    print(f'Packed {len(records)} original sprite frames, {len(sounds)+1} effects and SF2 menu music into {out}')


if __name__ == '__main__':
    main()
