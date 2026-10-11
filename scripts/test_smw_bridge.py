#!/usr/bin/env python3
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
tree = ROOT / 'build/smw'
compiler = shlex.split(os.environ.get('CC', 'cc'))
sdl_config = os.environ.get('SDL_CONFIG', 'sdl2-config')
flags = shlex.split(subprocess.check_output([sdl_config, '--cflags'], text=True))
libs = shlex.split(subprocess.check_output([sdl_config, '--libs'], text=True))
main = (tree / 'src/main.c').read_text()
assert main.count('int main(int argc, char** argv) {') == 1
(tree / 'src/main-test.c').write_text(main.replace('int main(int argc, char** argv) {',
    'int ww_upstream_main(int argc, char** argv) {'))
subprocess.run(compiler + ['-O1', '-g', '-fno-strict-aliasing', '-I.', '-DSYSTEM_VOLUME_MIXER_AVAILABLE=0'] +
    flags + ['-c', 'src/main-test.c', '-o', 'main-test.o'], cwd=tree, check=True)
(tree / 'src/main-test.c').unlink()
objects = [str(p) for p in sorted(tree.rglob('*.o')) if p.name not in ('main.o', 'main-test.o', 'main-play-test.o')]
binary = ROOT / 'build/test-smw-bridge'
subprocess.run(compiler + ['-O1', '-g', '-I'+str(tree / 'src')] + flags + [str(ROOT / 'tests/test_smw_bridge.c'),
    str(tree / 'main-test.o')] + objects + ['-o', str(binary)] + libs + ['-lm'], check=True)
subprocess.run([str(binary)], env={**os.environ, 'SDL_AUDIODRIVER': 'dummy'}, check=True)
