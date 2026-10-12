"""Import original Alpha rips as data, without executing the source JavaScript."""
from pathlib import Path
import base64
import hashlib
import json
import re
import struct
import subprocess
from urllib.request import urlopen
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def append_alpha(records, output):
    root = ROOT / 'local/community-sfa'
    manifest = json.loads((ROOT / 'config/alpha-assets.json').read_text())
    for entry in manifest['files']:
        path = root / entry['path']
        if not path.exists():
            url = manifest['url'].replace('github.com', 'raw.githubusercontent.com')
            data = urlopen(url+'/'+manifest['commit']+'/'+entry['path'], timeout=30).read()
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        if hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']:
            raise ValueError('Alpha asset hash mismatch: '+entry['path'])
    metadata = (root / 'script/player-akuma-spritedata.js').read_text()
    pattern = r'spriteLookup_\.load\("([^"]+)","([^"]+)",\s*"(-?\d+)px",\s*"(-?\d+)px",\s*"(\d+)px",\s*"(\d+)px"\)'
    atlas = {}
    images = {}
    for name, sheet, x, y, w, h in re.findall(pattern, metadata):
        if sheet not in atlas:
            atlas[sheet] = Image.open(root / sheet).convert('RGBA')
        x, y, w, h = -int(x), -int(y), int(w), int(h)
        images[Path(name).name] = atlas[sheet].crop((x,y,x+w,y+h))

    def pack(pose, im, ticks=4, anchor=None):
        w,h=im.size
        ax,ay=anchor if anchor else (w//2,h)
        pixels=b''.join(struct.pack('<I',0xff000000|(r<<16)|(g<<8)|b if a>=128 else 0)
                        for r,g,b,a in im.getdata())
        records.append(struct.pack('<7H',0,pose,ticks,w,h,ax,ay)+pixels)

    # Source atlases are enlarged 2x. Restore original pixels with nearest
    # sampling; the game applies its own integer scale on both renderers.
    def original(name):
        im=images[name]
        return im.resize(((im.width+1)//2,(im.height+1)//2), Image.Resampling.NEAREST)

    # Alpha's actual 36-frame activation particles, with their original
    # per-frame offsets placed on a stable canvas instead of jumping anchors.
    code=(root/'script/player-akuma.js').read_text()
    offsets=list(dict.fromkeys(re.findall(r'charge_up.addFrame\(player,folder \+ "/x-energize-0-(\d+).png",\d+,(-?\d+),(-?\d+)\)',code)))
    assert len(offsets)==36
    for index,x,y in offsets:
        im=original('r-energize-0-'+index+'.png')
        canvas=Image.new('RGBA',(256,256))
        canvas.alpha_composite(im,(80+int(x)//2,180-int(y)//2-im.height))
        pack(29,canvas,1,(128,128))
    for i in range(4):
        im=original(f'stance-{i}.png')
        pack(30,im.crop((0,0,im.width,76)),7)
    for i in range(4):
        im=original(f'r-fb-{i}.png')
        pack(31,im.crop((0,0,im.width,min(76,im.height))),4)
    for i in range(8):
        pack(32,original(f'win-1-{i}.png'),4 if i<7 else 120)
    for i in range(1,9):
        im=original(f'r-rfb-projectile-{i}.png')
        pack(33,im,3,(im.width//2,im.height//2))
    # The source projectile faces left; callers mirror it when moving right.
    hat=Image.open(ROOT/'local/community-sf2/Game/assets/images/sprites/characters/bison-sf2-s1.gif').convert('RGBA')
    hat=hat.crop((42,11,65,20))
    pack(34,hat,60,(hat.width//2,hat.height))
    pack(35,original('fb-charge-6.png'),60)
    # Original Alpha Psycho Crusher spin, used only for Bison's level 3.
    # The reference clip's enormous wake is composed around these pixels.
    metadata=(root/'script/player-mbison-spritedata.js').read_text()
    for name,sheet,x,y,w,h in re.findall(pattern,metadata):
        if Path(name).name not in [f'psycho-crusher-{i}.png' for i in range(4,8)]:continue
        im=Image.open(root/sheet).convert('RGBA')
        x,y,w,h=-int(x),-int(y),int(w),int(h)
        im=im.crop((x,y,x+w,y+h)).resize(((w+1)//2,(h+1)//2),Image.Resampling.NEAREST)
        im=im.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        canvas=Image.new('RGBA',(320,180))
        canvas.alpha_composite(im,(300-im.width,90-im.height//2))
        pack(36,canvas,3,(220,90))
    for file, name, dest in [('match','audio/misc/super-charge.zzz','alpha_charge'),
                             ('akuma','audio/akuma/super-0.zzz','akuma_super'),
                             ('akuma','audio/akuma/warp-0.zzz','akuma_warp'),
                             ('mbison','audio/mbison/psycho-crusher.zzz','psycho_super')]:
        data=(root/f'script/audio/{file}.ogg.js').read_text()
        match=re.search(r'loadBase64\("'+re.escape(name)+r'"[^\n]*?data:audio/ogg;base64,([^"\s]+)',data)
        if not match:raise ValueError('Missing original Alpha audio: '+name)
        ogg=root/(dest+'.ogg');ogg.write_bytes(base64.b64decode(match[1]))
        subprocess.run(['ffmpeg','-y','-hide_banner','-loglevel','error','-i',str(ogg),
                        '-ac','1','-ar','48000',str(output/'audio'/f'{dest}.wav')],check=True)
