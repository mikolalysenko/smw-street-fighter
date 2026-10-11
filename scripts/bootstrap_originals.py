#!/usr/bin/env python3
"""Rebuild the verified SMW USA data from the pinned public disassembly."""
from pathlib import Path
import hashlib
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCES = (
    ('SMWDisX', 'https://github.com/IsoFrieze/SMWDisX.git', '30643c7595a7d097d731de69476f8059c7cf98c3'),
    ('asar-src', 'https://github.com/RPGHacker/asar.git', '6a6dbdaed8cb773e583052ce4f66375bd896c3a1'),
)


def run(args, cwd=ROOT, log=None):
    if log:
        with log.open('a') as stream:
            result = subprocess.run(args, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT)
        if result.returncode:
            raise SystemExit(f'Command failed; see {log}')
    else:
        subprocess.run(args, cwd=cwd, check=True)


def main():
    if not shutil.which('cmake'):
        raise SystemExit('Install CMake, a C++ compiler, git and Python 3. Then rerun make original-assets.')
    (ROOT / 'build').mkdir(exist_ok=True)
    (ROOT / 'local').mkdir(exist_ok=True)
    for name, url, pin in SOURCES:
        dest=ROOT/'build'/name
        if not dest.exists():
            run(['git','init',str(dest)])
            run(['git','remote','add','origin',url], dest)
            run(['git','fetch','--depth','1','origin',pin], dest)
            run(['git','checkout','--detach','FETCH_HEAD'], dest)
        actual=subprocess.check_output(['git','rev-parse','HEAD'],cwd=dest,text=True).strip()
        if actual!=pin:
            raise SystemExit(f'{dest} has revision {actual}; expected {pin}. Use a clean build directory.')
    log=ROOT/'build/original-assets-build.log'
    print('Building Asar 1.91...',flush=True)
    run(['cmake','-S','build/asar-src/src','-B','build/asar-build','-DCMAKE_BUILD_TYPE=Release','-DASAR_GEN_DLL=ON'],log=log)
    run(['cmake','--build','build/asar-build','--target','asar-standalone','-j4'],log=log)
    rom=ROOT/'local/smw-rebuilt.sfc'
    run([str(ROOT/'build/asar-build/asar/bin/asar'),'-D_VER=1','build/SMWDisX/smw.asm',str(rom)],log=log)
    digest=hashlib.sha1(rom.read_bytes()).hexdigest()
    if digest!='6b47bb75d16514b6a476aa0c73a683a2a4c18765':
        raise SystemExit(f'Rebuilt ROM checksum mismatch: {digest}')
    print('Rebuilt SMW matches the original USA ROM exactly.',flush=True)
    run([sys.executable,'scripts/import_smw.py',str(rom)])


if __name__ == '__main__':
    main()
