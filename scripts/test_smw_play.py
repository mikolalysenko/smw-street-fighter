#!/usr/bin/env python3
from pathlib import Path
import os
import shlex
import shutil
import subprocess
import tempfile
import sys

ROOT=Path(__file__).resolve().parents[1]
tree=ROOT/'build/smw'
for asset in ('smw_assets.dat','sf2/sprites.dat'):
    if not (tree/asset).exists():raise SystemExit('Run make original-assets smw first.')
cc=shlex.split(os.environ.get('CC','cc'))
sdl=os.environ.get('SDL_CONFIG','sdl2-config')
flags=shlex.split(subprocess.check_output([sdl,'--cflags'],text=True))
libs=shlex.split(subprocess.check_output([sdl,'--libs'],text=True))
main=(tree/'src/main.c').read_text().replace('int main(int argc, char** argv) {','int ww_upstream_main(int argc, char** argv) {')
main=main.replace('#include "ww_smw.h"','#include "ww_smw.h"\nuint32_t ww_test_input(void);\nvoid ww_test_render(uint8_t*,size_t,int,int);')
main=main.replace('    ww_pending_buttons = 0;','    ww_pending_buttons = 0;\n    inputs = ww_test_input();')
main=main.replace('  g_renderer_funcs.EndDraw();','  ww_test_render(pixel_buffer,pitch,g_snes_width,g_snes_height);\n  g_renderer_funcs.EndDraw();')
main=main.replace('  bool enable_audio = true;','  bool enable_audio = false;')
source=tree/'src/main-play-test.c';source.write_text(main)
try:
    subprocess.run(cc+['-O1','-g','-fno-strict-aliasing','-I.','-DSYSTEM_VOLUME_MIXER_AVAILABLE=0']+flags+['-c',str(source),'-o','main-play-test.o'],cwd=tree,check=True)
finally:source.unlink(missing_ok=True)
objects=[str(p) for p in sorted(tree.rglob('*.o')) if p.name not in ('main.o','main-test.o','main-play-test.o')]
binary=tree/'test-smw-play'
test='test_smw_final_depth.c' if '--final-depth' in sys.argv else 'test_smw_wallrun.c' if '--wallrun' in sys.argv else 'test_smw_final_grapple.c' if '--final-grapple' in sys.argv else 'test_smw_entrances.c' if '--entrances' in sys.argv else 'test_smw_grapple.c' if '--grapple' in sys.argv else 'test_smw_carry.c' if '--carry' in sys.argv else 'test_smw_specials.c' if '--specials' in sys.argv else 'test_smw_pipes.c' if '--pipes' in sys.argv else 'test_smw_collision.c' if '--collision' in sys.argv else 'test_smw_scroll.c' if '--scroll' in sys.argv else 'test_smw_hud.c' if '--hud' in sys.argv else 'test_smw_roster.c' if '--roster' in sys.argv else 'test_smw_title.c' if '--title' in sys.argv else 'test_smw_bosses.c' if '--bosses' in sys.argv else 'test_smw_presentation.c' if '--presentation' in sys.argv else 'test_smw_movement.c' if '--movement' in sys.argv else 'test_smw_play.c'
if '--features' in sys.argv:test='test_smw_features.c'
subprocess.run(cc+['-O1','-g','-I'+str(tree/'src'),'-I'+str(tree)]+flags+[str(ROOT/'tests'/test),str(tree/'main-play-test.o')]+objects+['-o',str(binary)]+libs+['-lm'],check=True)
config=(ROOT/'config/smw.ini').read_text().replace('DisableFrameDelay = 0','DisableFrameDelay = 1').replace('OutputMethod = SDL','OutputMethod = SDL-Software').replace('WindowScale = 4','WindowScale = 1')
cases=([os.environ['WW_TEST_BOSS']] if 'WW_TEST_BOSS' in os.environ else
       ['1f6','0e5','1f2','0d9','0cc','0d3','1eb','0d5','0e4','1c7']) if '--bosses' in sys.argv else [None]
for boss in cases:
    with tempfile.TemporaryDirectory(prefix='play-test-',dir=tree) as folder:
        runtime=Path(folder)
        shutil.copy2(binary,runtime/binary.name)
        for name in ('smw_assets.dat','sf2'):
            (runtime/name).symlink_to(tree/name)
        (runtime/'test-play.ini').write_text(config)
        (runtime/'saves').mkdir()
        env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','WW_SAVE_DIR':str(runtime/'saves')}
        if boss:env['WW_TEST_BOSS']=boss
        try:
            subprocess.run([str(runtime/binary.name)],env=env,check=True,timeout=60)
            if '--title' in sys.argv:
                subprocess.run([str(runtime/binary.name)],env={**env,'WW_TEST_RELOAD':'1'},check=True,timeout=60)
        finally:
            for capture in runtime.glob('*.bmp'):
                shutil.copy2(capture,tree/capture.name)
