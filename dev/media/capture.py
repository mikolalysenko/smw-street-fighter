#!/usr/bin/env python3
"""Record deterministic gameplay and mixed game audio from the native engine.
Run make smw first. Outputs raw captures and lossless masters under build/media.
"""
from pathlib import Path
import os
import shlex
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
tree=ROOT/'build/smw';out=ROOT/'build/media';out.mkdir(parents=True,exist_ok=True)
cc=shlex.split(os.environ.get('CC','cc'))
cflags=shlex.split(subprocess.check_output(['sdl2-config','--cflags'],text=True))
libs=shlex.split(subprocess.check_output(['sdl2-config','--libs'],text=True))
flags=['-O2','-g','-fno-strict-aliasing','-I'+str(tree),'-I'+str(tree/'src'),'-DSYSTEM_VOLUME_MIXER_AVAILABLE=0']+cflags
source=(tree/'src/main.c').read_text().replace('int main(int argc, char** argv) {','int ww_upstream_main(int argc, char** argv) {')
source=source.replace('#include "ww_smw.h"','#include "ww_smw.h"\nuint32_t ww_test_input(void);\nvoid ww_test_render(uint8_t*,size_t,int,int);')
source=source.replace('    ww_pending_buttons = 0;','    ww_pending_buttons = 0;\n    inputs = ww_test_input();')
source=source.replace('  g_renderer_funcs.EndDraw();','  ww_test_render(pixel_buffer,pitch,g_snes_width,g_snes_height);\n  g_renderer_funcs.EndDraw();')
source=source.replace('  bool enable_audio = true;','  bool enable_audio = false;')
(out/'main-capture.c').write_text(source)
subprocess.run(cc+flags+['-c',str(out/'main-capture.c'),'-o',str(out/'main.o')],check=True)
subprocess.run(cc+flags+['-DFC_AUDIO_MANUAL','-c',str(ROOT/'src/audio.c'),'-o',str(out/'audio.o')],check=True)
objects=[str(p) for p in sorted(tree.rglob('*.o')) if p.name not in ('main.o','main-test.o','main-play-test.o','audio.o')]
subprocess.run(cc+flags+[str(ROOT/'dev/media/capture.c'),str(out/'main.o'),str(out/'audio.o')]+objects+libs+['-lm','-o',str(out/'capture')],check=True)
config=(ROOT/'config/smw.ini').read_text().replace('DisableFrameDelay = 0','DisableFrameDelay = 1').replace('OutputMethod = SDL','OutputMethod = SDL-Software').replace('WindowScale = 4','WindowScale = 1')
scenes=['title','select','bonk','shinku','psycho-crusher','chunli','takeover','akuma','piledriver']
for number,name in enumerate(scenes):
    if os.environ.get('WW_SCENE') and os.environ['WW_SCENE']!=name:continue
    with tempfile.TemporaryDirectory(prefix='capture-',dir=out) as folder:
        work=Path(folder);shutil.copy2(out/'capture',work/'capture')
        (work/'capture.ini').write_text(config)
        for item in ('smw_assets.dat','sf2'):(work/item).symlink_to(tree/item)
        (work/'saves').mkdir()
        env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','WW_SAVE_DIR':str(work/'saves')}
        subprocess.run([str(work/'capture'),str(number),str(out/(name+'.bgra')),str(out/(name+'.f32'))],env=env,check=True,timeout=120)
    subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-f','rawvideo','-pixel_format','bgra','-video_size','256x224',
        '-framerate','48000/801','-i',str(out/(name+'.bgra')),'-f','f32le','-ar','48000','-ac','2','-i',str(out/(name+'.f32')),
        '-c:v','ffv1','-c:a','pcm_s16le','-shortest',str(out/(name+'.mkv'))],check=True)
