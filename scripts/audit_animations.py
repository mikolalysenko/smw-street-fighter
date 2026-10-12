#!/usr/bin/env python3
"""Render the packed runtime sprites for visual review (requires asset venv).

This reads sprites.dat, including the final palettes, layers and signed-origin
padding, rather than assuming the source crops match what the game loads.
"""
import argparse
import json
from pathlib import Path
import struct
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
LABELS = {0:'Idle',1:'Walk',2:'Jump up',3:'Crouch',4:'Punch',5:'Kick',6:'Fireball',
          7:'Rising',8:'Spin',9:'Rush',10:'Flurry / flame',20:'Win',21:'Air punch',
          22:'Air kick',23:'Crouch punch',24:'Crouch kick',37:'Throw',38:'Teleport',
          39:'Dive',43:'Jump forward'}
SPECIALS = [[6,7,8],[9,10],[6,10,8],[6,7],[6,7,8],[9,7,10],
            [8,37],[6,10,38],[9,7],[9,39],[6,7],[9,39]]


def read_frames(path):
    data = path.read_bytes()
    if data[:4] != b'WWA1':
        raise ValueError('Not a WWA1 sprite pack')
    offset, frames = 6, {}
    for _ in range(struct.unpack_from('<H', data, 4)[0]):
        c,p,t,w,h,ax,ay = struct.unpack_from('<7H', data, offset)
        offset += 14
        im = Image.frombytes('RGBA', (w,h), data[offset:offset+w*h*4], 'raw', 'BGRA')
        offset += w*h*4
        frames.setdefault((c,p), []).append((im,ax,ay,t))
    if offset != len(data):
        raise ValueError('Truncated or trailing sprite data')
    return frames


def sprite(canvas, frame, x, feet):
    im,ax,ay,_ = frame
    im = im.resize(((im.width+1)//2,(im.height+1)//2), Image.Resampling.NEAREST)
    canvas.paste(im, (x-ax//2,feet-ay//2), im)


def sample(frames, tick):
    for frame in frames:
        if tick < frame[3]:
            return frame
        tick -= frame[3]
    return frames[-1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets',type=Path,default=ROOT/'local/sf2/sprites.dat')
    parser.add_argument('--output',type=Path,default=ROOT/'build/animation-audit')
    args = parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    names = [c['name'] for c in json.loads((ROOT/'config/sf2-sprites.json').read_text())['characters']]
    frames = read_frames(args.assets)
    for c in range(12):
        for category, poses in [('normals',[0,1,3,2,43,4,5,23,24,21,22]),('specials',SPECIALS[c])]:
            width = 110+max(len(frames[c,p]) for p in poses)*100
            canvas = Image.new('RGB',(width,24+len(poses)*108),'#242834')
            draw = ImageDraw.Draw(canvas);draw.text((4,4),names[c],fill='white')
            for row,pose in enumerate(poses):
                y = 24+row*108;draw.text((4,y+30),LABELS[pose],fill='white')
                for col,frame in enumerate(frames[c,pose]):
                    x = 110+col*100;draw.line((x,y+90,x+98,y+90),fill='#596577')
                    draw.line((x+40,y+12,x+40,y+90),fill='#394452')
                    draw.text((x+4,y+2),f'{col} ({frame[3]}t)',fill='#eadfca')
                    sprite(canvas,frame,x+40,y+90)
            canvas.save(args.output/f'{category}-{c:02d}.png')
    for pose in [0,1,2,43,4,5,23,24,21,22]:
        images=[]
        for t in range(0,96,2):
            canvas=Image.new('RGB',(4*170,3*130),'#242834');draw=ImageDraw.Draw(canvas)
            for c,name in enumerate(names):
                x=(c%4)*170;y=(c//4)*130
                draw.text((x+4,y+4),name+' / '+LABELS[pose],fill='white')
                draw.line((x+6,y+113,x+164,y+113),fill='#596577')
                clip=frames[c,pose];duration=sum(f[3] for f in clip)
                tick=t%duration if pose in (0,1) else t%48
                if pose not in (0,1,2,43) and tick>=duration:clip=frames[c,0];tick=0
                sprite(canvas,sample(clip,tick),x+75,y+113)
            images.append(canvas)
        images[0].save(args.output/f'roster-{pose:02d}.gif',save_all=True,append_images=images[1:],duration=33,loop=0)
    print(f'Wrote original sprite contact sheets and roster loops to {args.output}')


if __name__ == '__main__':
    main()
