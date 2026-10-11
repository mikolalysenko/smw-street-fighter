#!/usr/bin/env python3
"""Package the current platform's native build and locally prepared assets."""
from pathlib import Path
import platform
import plistlib
import shutil
import stat

ROOT=Path(__file__).resolve().parents[1]
build=ROOT/'build/smw'
for required in ('smw','smw_assets.dat','sf2/sprites.dat','sf2/audio/ko.wav'):
    if not (build/required).exists():
        raise SystemExit('Run make original-assets and make smw before packaging.')
if platform.system()=='Darwin':
    dest=ROOT/'dist/Super Mario Fighter 2 The World Warriors.app/Contents'
    (dest/'MacOS').mkdir(parents=True,exist_ok=True)
    resources=dest/'Resources';resources.mkdir(exist_ok=True)
    # Keep progress from the earlier app name when first building this bundle.
    old_saves=ROOT/'dist/World Warrior SMW.app/Contents/Resources/saves'
    if old_saves.is_dir() and not (resources/'saves').exists():
        shutil.copytree(old_saves,resources/'saves')
    shutil.copy2(build/'smw',dest/'MacOS/world-warrior-smw')
    (dest/'Info.plist').write_bytes(plistlib.dumps({
        'CFBundleExecutable':'world-warrior-smw','CFBundleIdentifier':'local.worldwarrior.smw',
        'CFBundleName':'Super Mario Fighter 2 The World Warriors','CFBundlePackageType':'APPL','CFBundleVersion':'0.4',
        'NSHighResolutionCapable':False}))
else:
    dest=ROOT/'dist/super-mario-fighter-2-linux'
    dest.mkdir(parents=True,exist_ok=True);resources=dest
    shutil.copy2(build/'smw',dest/'world-warrior-smw')
    launcher=dest/'play.sh'
    launcher.write_text('#!/bin/sh\nset -eu\ncd "$(dirname "$0")"\nexec ./world-warrior-smw --config tv.ini "$@"\n')
    launcher.chmod(launcher.stat().st_mode|stat.S_IXUSR|stat.S_IXGRP|stat.S_IXOTH)
for name in ('smw.ini','tv.ini','smw_assets.dat'):
    shutil.copy2(build/name,resources/name)
shutil.copytree(build/'sf2',resources/'sf2',dirs_exist_ok=True)
shutil.copy2(ROOT/'README.md',resources/'README.md')
shutil.copy2(ROOT/'LICENSE',resources/'LICENSE')
shutil.copy2(ROOT/'references/smw/LICENSE.txt',resources/'SMW-engine-LICENSE.txt')
shutil.copy2(ROOT/'docs/ASSETS.md',resources/'ASSETS.md')
print(f'Packaged {dest}')
