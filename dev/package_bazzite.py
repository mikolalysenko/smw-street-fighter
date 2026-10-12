#!/usr/bin/env python3
"""Repackage the Linux x86-64 release for Bazzite and Steam Gaming Mode.

Adds Steam library artwork (media/steam, from dev/media/steam_art.py) and an
installer that registers a non-Steam shortcut. The game itself is the
unchanged, boot-tested Linux build.
"""
import argparse
import json
import shutil
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LINUX = 'super-mario-world-ii-linux-x86_64'
NAME = 'super-mario-world-ii-bazzite-x86_64'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('linux_archive', nargs='?', default=ROOT / 'dist/releases' / (LINUX + '.tar.gz'), type=Path)
    args = ap.parse_args()
    out = ROOT / 'dist/releases'
    stage = out / NAME
    if stage.exists():
        shutil.rmtree(stage)
    with tarfile.open(args.linux_archive) as archive:
        # Extraction filters arrived in Python 3.12 (and security backports).
        safe = {'filter': 'tar'} if hasattr(tarfile, 'tar_filter') else {}
        archive.extractall(out / '.bazzite-tmp', **safe)
    shutil.move(out / '.bazzite-tmp' / LINUX, stage)
    shutil.rmtree(out / '.bazzite-tmp')
    steam = stage / 'steam'
    steam.mkdir()
    for art in ('portrait.png', 'wide.png', 'hero.png', 'logo.png', 'icon.png'):
        shutil.copy2(ROOT / 'media/steam' / art, steam / art)
    shutil.copy2(ROOT / 'dev/bazzite/steam_shortcut.py', steam / 'steam_shortcut.py')
    shutil.copy2(ROOT / 'dev/bazzite/install.sh', stage / 'install.sh')
    (stage / 'install.sh').chmod(0o755)
    start = stage / 'START-HERE.txt'
    start.write_text(start.read_text().replace(
        'Extract the whole archive, then run ./play.sh (or ./play.sh --fullscreen).\n'
        'For Steam Gaming Mode, add play.sh as a non-Steam game and set --fullscreen in Launch Options.\n',
        'Bazzite: extract the archive in Desktop Mode and run ./install.sh. It installs to ~/Games,\n'
        'adds Super Mario World II to Steam with library art and fullscreen launch options, and\n'
        'restarts Steam. Then play from Gaming Mode. ./play.sh --fullscreen also runs it directly.\n'))
    build = json.loads((stage / 'build.json').read_text())
    build['package'] = 'bazzite'
    (stage / 'build.json').write_text(json.dumps(build, indent=2) + '\n')
    print(shutil.make_archive(str(out / NAME), 'gztar', out, NAME))


if __name__ == '__main__':
    main()
