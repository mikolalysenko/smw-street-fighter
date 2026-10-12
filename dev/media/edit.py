#!/usr/bin/env python3
"""Edit the recorded game/audio masters into the public teaser and screenshots.

Requires Pillow and ffmpeg. Capture with capture.py first. Art is taken from
the actual game framebuffer; typography and framing are composited here.
"""
from pathlib import Path
import json
import os
import subprocess
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'build/media'
OUT = ROOT / 'media'
OUT.mkdir(exist_ok=True)
FPS = 48000 / 801
FONT = Path(os.environ.get('WW_MEDIA_FONTS', '/System/Library/Fonts/Supplemental'))

def font(size, bold=False):
    return ImageFont.truetype(str(FONT / ('Arial Bold.ttf' if bold else 'Arial.ttf')), size)

def frame(name, number):
    with (WORK / (name + '.bgra')).open('rb') as stream:
        stream.seek(number * 256 * 224 * 4)
        return Image.frombytes('RGBA', (256, 224), stream.read(256 * 224 * 4), 'raw', 'BGRA').convert('RGB')

def lines(draw, text, xy, width, size, color, bold=False, spacing=1.2):
    x, y = xy
    line = ''
    for word in text.split():
        attempt = (line + ' ' + word).strip()
        if line and draw.textlength(attempt, font=font(size, bold)) > width:
            draw.text((x, y), line, font=font(size, bold), fill=color)
            y += int(size * spacing)
            line = word
        else:
            line = attempt
    draw.text((x, y), line, font=font(size, bold), fill=color)
    return y + int(size * spacing)

def card(headline, description, number, ending=False):
    image = Image.new('RGB', (1920, 1080), '#090d23')
    draw = ImageDraw.Draw(image)
    for x in range(-1080, 1920, 64):
        draw.line((x, 1080, x+1080, 0), fill='#10162e', width=2)
    draw.rectangle((0, 0, 1919, 9), fill='#ffc845')
    draw.text((64, 82), 'DINOSAUR LAND. ARCADE RULES.', font=font(22, True), fill='#ffc845')
    draw.text((62, 142), 'SUPER MARIO', font=font(65, True), fill='#f3f3ed')
    draw.text((62, 211), 'WORLD II', font=font(65, True), fill='#f3f3ed')
    draw.text((66, 296), 'THE WORLD WARRIOR', font=font(25, True), fill='#83d9ff')
    draw.line((64, 365, 700, 365), fill='#384263', width=2)
    y = lines(draw, headline, (64, 425), 660, 55, '#ffc845', True)
    lines(draw, description, (64, y+27), 646, 29, '#c5cce1', spacing=1.4)
    draw.text((64, 856), 'BROWSER  /  macOS  /  LINUX  /  WINDOWS', font=font(21, True), fill='#f3f3ed')
    draw.text((64, 901), 'Keyboard · Gamepad · Phone controls', font=font(24), fill='#9aaaca')
    draw.text((64, 988), 'PLAY THE CROSSOVER', font=font(22, True), fill='#ffc845')
    draw.text((1675, 1032), f'{number:02d} / TEASER', font=font(18), fill='#9aaaca')
    if not ending:
        draw.rectangle((831, 87, 1864, 992), fill='#040710', outline='#657499', width=2)
    return image

shots = {'shinku': ('shinku', 110), 'akuma': ('akuma', 300),
         'psycho-crusher': ('psycho-crusher', 285), 'piledriver': ('piledriver', 40),
         'select': ('select', 155)}
for name, (source, number) in shots.items():
    frame(source, number).resize((768, 672), Image.Resampling.NEAREST).save(OUT / (name+'.png'), optimize=True)

cover = card('Pick a fighter. Break the rules.', 'Twelve world warriors. Three bars of super. One very different final boss.', 1)
cover.paste(frame('shinku', 110).resize((1024, 896), Image.Resampling.NEAREST), (836, 92))
cover.save(OUT / 'cover.jpg', quality=94, subsampling=0)

social = Image.new('RGB', (1200, 630), '#090d23')
sd = ImageDraw.Draw(social)
sd.rectangle((0, 0, 1200, 7), fill='#ffc845')
sd.text((36, 52), 'DINOSAUR LAND. ARCADE RULES.', font=font(17, True), fill='#ffc845')
sd.text((34, 138), 'SUPER MARIO', font=font(47, True), fill='#f3f3ed')
sd.text((34, 191), 'WORLD II', font=font(47, True), fill='#f3f3ed')
sd.text((37, 262), 'THE WORLD WARRIOR', font=font(21, True), fill='#83d9ff')
lines(sd, 'Pick a fighter. Break the rules.', (36, 361), 470, 33, '#ffc845', True)
sd.text((36, 531), 'PLAY IN YOUR BROWSER', font=font(21, True), fill='#f3f3ed')
social.paste(frame('shinku', 110).resize((640, 560), Image.Resampling.NEAREST), (550, 40))
social.save(OUT / 'og-image.jpg', quality=95, subsampling=0)

# A recognizable in-game portrait for browser and native app icons.
portrait = frame('shinku', 110).crop((6, 28, 29, 56))
icon = Image.new('RGB', (1024, 1024), '#0b112b')
icon.paste(portrait.resize((736, 896), Image.Resampling.NEAREST), (144, 64))
ImageDraw.Draw(icon).rectangle((32, 32, 991, 991), outline='#ffc845', width=20)
icon.save(OUT / 'app.icns', format='ICNS')
icon.resize((180, 180), Image.Resampling.LANCZOS).save(OUT / 'apple-touch-icon.png')
icon.resize((32, 32), Image.Resampling.LANCZOS).save(OUT / 'favicon.png')

# Source frame cuts preserve the original 801-sample game tick exactly.
scenes = [
    ('title', 0, 170, 'Welcome back to Dinosaur Land.', 'The original world. A whole new way to fight.'),
    ('select', 0, 275, 'Twelve fighters. Your choice.', 'Pick your world warrior, then take on Mario’s original levels.'),
    ('bonk', 0, 195, 'Bonk. Flip. Hadouken.', 'Punches and kicks play with the world. Specials blow it apart.'),
    ('shinku', 0, 270, 'Three bars. One giant beam.', 'Build your meter and unleash Shinku Hadouken.'),
    ('chunli', 0, 285, 'Keep the kicks coming.', 'Chun-Li brings Kikoken and rapid-fire Lightning Legs.'),
    ('psycho-crusher', 0, 753, 'Psycho Crusher, all the way.', 'Steer the level-three super through the stage—and stick the landing.'),
    ('takeover', 695, 461, 'There’s a new final boss.', 'Akuma has a very different plan for Bowser’s clown car.'),
    ('akuma', 120, 300, 'Find your opening.', 'Dodge, parry and fight back between attack waves.'),
    ('piledriver', 0, 200, 'Yes. The entire clown car.', 'Zangief’s Final Atomic Buster has no size limit.'),
]
segments = []
for index, (name, start, count, headline, description) in enumerate(scenes, 1):
    background = WORK / f'card-{index:02}.png'
    card(headline, description, index).save(background)
    duration = count / FPS
    dest = WORK / f'part-{index:02}.mp4'
    subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y',
        '-loop', '1', '-framerate', '60', '-i', str(background),
        '-ss', str(start/FPS), '-i', str(WORK/(name+'.mkv')),
        '-filter_complex', '[1:v]scale=1024:896:flags=neighbor[game];[0:v][game]overlay=836:92,format=yuv420p[v]',
        '-map', '[v]', '-map', '1:a:0', '-t', str(duration), '-r', '60',
        '-af', f'volume=0.85,afade=t=in:d=0.025,afade=t=out:st={duration-.06}:d=0.06',
        '-c:v', 'libx264', '-preset', 'fast', '-crf', '21', '-c:a', 'aac', '-b:a', '192k', str(dest)], check=True)
    segments.append(dest)

end = card('Every fighter has an ending.', 'Play now. Bring a controller. Or bring your phone.', 10, True)
for i in range(12):
    art = Image.open(ROOT/f'build/smw/ending-{i:02}.bmp').convert('RGB')
    end.paste(art.resize((248, 217), Image.Resampling.NEAREST), (828+(i%4)*256, 185+(i//4)*231))
ed = ImageDraw.Draw(end)
ed.text((840, 931), 'mikolalysenko.github.io/smw-street-fighter', font=font(24, True), fill='#ffc845')
end.save(WORK/'end-card.png')
dest = WORK/'part-10.mp4'
subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-loop', '1', '-framerate', '60',
    '-i', str(WORK/'end-card.png'), '-i', str(WORK/'select.mkv'), '-t', '4',
    '-vf', 'format=yuv420p', '-af', 'volume=0.85,afade=t=out:st=3:d=1',
    '-c:v', 'libx264', '-preset', 'fast', '-crf', '21', '-c:a', 'aac', '-b:a', '192k', str(dest)], check=True)
segments.append(dest)
(WORK/'concat.txt').write_text(''.join(f"file '{p.name}'\n" for p in segments))
subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-f', 'concat', '-safe', '0',
    '-i', str(WORK/'concat.txt'), '-c', 'copy', '-movflags', '+faststart',
    '-metadata', 'title=Super Mario World II — The World Warrior | Gameplay teaser', str(OUT/'teaser.mp4')], check=True)
(WORK/'edit.json').write_text(json.dumps(scenes, indent=2)+'\n')
print(OUT/'teaser.mp4')
