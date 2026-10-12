#!/usr/bin/env python3
"""Cut actual gameplay into a short, caption-free teaser. Requires Pillow/ffmpeg."""
from pathlib import Path
import json
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'build/media'
OUT = ROOT / 'media'
OUT.mkdir(exist_ok=True)
FPS = 48000 / 801


def frame(name, number):
    with (WORK / (name + '.bgra')).open('rb') as stream:
        stream.seek(number * 256 * 224 * 4)
        return Image.frombytes('RGBA', (256, 224), stream.read(256 * 224 * 4), 'raw', 'BGRA').convert('RGB')


def run(*args):
    subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', *map(str, args)], check=True)


shots = {'shinku': ('shinku', 110), 'akuma': ('akuma', 300),
         'psycho-crusher': ('psycho-crusher', 285), 'piledriver': ('piledriver', 40),
         'select': ('select', 155)}
for name, (source, number) in shots.items():
    frame(source, number).resize((768, 672), Image.Resampling.NEAREST).save(OUT / (name+'.png'), optimize=True)

# Full game frame, no captions, feature lists, counters, or decorative panels.
# Black sides preserve the original playfield and its HUD inside a 16:9 video.
cover = Image.new('RGB', (1920, 1080))
cover.paste(frame('shinku', 110).resize((1234, 1080), Image.Resampling.NEAREST), (343, 0))
cover.save(OUT / 'cover.jpg', quality=94, subsampling=0)

# The only title card uses the actual in-game title artwork and its native
# PRESS START prompt. Endings stay out of the teaser so players discover them.
title = frame('title', 180)
logo = title.crop((18, 34, 236, 124))
end = Image.new('RGB', (1920, 1080))
end.paste(logo.resize((1308, 540), Image.Resampling.NEAREST), (306, 190))
prompt = title.crop((80, 137, 176, 150)).resize((480, 65), Image.Resampling.NEAREST)
end.paste(prompt, (720, 842))
end.save(WORK / 'end-card.png')
social = Image.new('RGB', (1200, 630))
social.paste(logo.resize((1090, 450), Image.Resampling.NEAREST), (55, 90))
social.save(OUT / 'og-image.jpg', quality=95, subsampling=0)

# Frame-accurate cuts: action first, short character-select beat, escalating
# supers, a glimpse of the final challenger, then the clown-car slam.
scenes = [
    ('bonk', 85, 72),
    ('select', 48, 84),
    ('chunli', 12, 160),
    ('shinku', 22, 118),
    ('psycho-crusher', 20, 155),
    ('psycho-crusher', 375, 96),
    ('psycho-crusher', 683, 70),
    ('takeover', 873, 81),
    ('takeover', 987, 147),
    ('akuma', 250, 155),
    ('piledriver', 0, 102),
]
segments = []
for index, (name, start, count) in enumerate(scenes, 1):
    duration = count / FPS
    dest = WORK / f'cut-{index:02}.mp4'
    run('-ss', start/FPS, '-i', WORK/(name+'.mkv'), '-t', duration,
        '-vf', 'scale=1234:1080:flags=neighbor,pad=1920:1080:343:0:black,setsar=1,format=yuv420p',
        '-af', f'volume=0.85,afade=t=in:d=0.015,afade=t=out:st={duration-.035}:d=0.035',
        '-r', '60', '-c:v', 'libx264', '-preset', 'fast', '-crf', '20',
        '-c:a', 'aac', '-b:a', '192k', dest)
    segments.append(dest)

dest = WORK/'cut-end.mp4'
run('-loop', '1', '-framerate', '60', '-i', WORK/'end-card.png',
    '-i', WORK/'select.mkv', '-t', '2', '-vf', 'format=yuv420p',
    '-af', 'volume=0.75,afade=t=in:d=0.03,afade=t=out:st=1.4:d=0.6',
    '-c:v', 'libx264', '-preset', 'fast', '-crf', '20', '-c:a', 'aac', '-b:a', '192k', dest)
segments.append(dest)
(WORK/'concat.txt').write_text(''.join(f"file '{p.name}'\n" for p in segments))
run('-f', 'concat', '-safe', '0', '-i', WORK/'concat.txt', '-c', 'copy', '-movflags', '+faststart',
    '-metadata', 'title=Super Mario World II — The World Warrior | Teaser', OUT/'teaser.mp4')
(WORK/'edit.json').write_text(json.dumps(scenes, indent=2)+'\n')
print(OUT/'teaser.mp4')
