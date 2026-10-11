#!/usr/bin/env python3
from pathlib import Path
import os
import shlex
import shutil
import subprocess
import tempfile

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
subprocess.run(cc+['-O1','-g','-I'+str(tree/'src')]+flags+[str(ROOT/'tests/test_smw_play.c'),str(tree/'main-play-test.o')]+objects+['-o',str(binary)]+libs+['-lm'],check=True)
config=(ROOT/'config/smw.ini').read_text().replace('DisableFrameDelay = 0','DisableFrameDelay = 1').replace('OutputMethod = SDL','OutputMethod = SDL-Software').replace('WindowScale = 4','WindowScale = 1')
with tempfile.TemporaryDirectory(prefix='play-test-',dir=tree) as folder:
    runtime=Path(folder)
    shutil.copy2(binary,runtime/binary.name)
    for name in ('smw_assets.dat','sf2'):
        (runtime/name).symlink_to(tree/name)
    (runtime/'test-play.ini').write_text(config)
    subprocess.run([str(runtime/binary.name)],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy'},check=True,timeout=60)
    for capture in runtime.glob('*.bmp'):
        shutil.copy2(capture,tree/capture.name)
