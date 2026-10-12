#!/usr/bin/env python3
"""Add (or refresh) the game as a non-Steam shortcut with library artwork.

Usage: steam_shortcut.py GAME_DIR. Steam must not be running: it rewrites
shortcuts.vdf from memory when it exits.
"""
import os
import shutil
import struct
import sys
import zlib
from pathlib import Path

NAME = 'Super Mario World II'
OPTIONS = '--fullscreen'
# Steam's grid file suffixes for each piece of art in steam/.
ART = {'portrait.png': 'p.png', 'wide.png': '.png', 'hero.png': '_hero.png', 'logo.png': '_logo.png'}


def read_map(data, i):
    out = {}
    while data[i] != 0x08:
        kind, i = data[i], i + 1
        end = data.index(b'\0', i)
        key, i = data[i:end].decode('utf-8', 'replace'), end + 1
        if kind == 0x00:
            out[key], i = read_map(data, i)
        elif kind == 0x01:
            end = data.index(b'\0', i)
            out[key], i = data[i:end].decode('utf-8', 'replace'), end + 1
        elif kind == 0x02:
            out[key], i = struct.unpack_from('<i', data, i)[0], i + 4
        else:
            raise SystemExit(f'Unsupported shortcuts.vdf field type {kind}')
    return out, i + 1


def write_map(values):
    out = b''
    for key, value in values.items():
        name = key.encode() + b'\0'
        if isinstance(value, dict):
            out += b'\x00' + name + write_map(value)
        elif isinstance(value, int):
            out += b'\x02' + name + struct.pack('<i', value)
        else:
            out += b'\x01' + name + str(value).encode() + b'\0'
    return out + b'\x08'


def steam_root():
    for path in ('~/.local/share/Steam', '~/.steam/steam', '~/.var/app/com.valvesoftware.Steam/data/Steam'):
        path = Path(path).expanduser()
        if (path / 'userdata').is_dir():
            return path.resolve()
    raise SystemExit('Steam user data not found; start Steam and sign in once first.')


def main():
    game = Path(sys.argv[1]).resolve()
    exe = f'"{game / "play.sh"}"'
    appid = (zlib.crc32((exe + NAME).encode()) | 0x80000000) & 0xffffffff
    signed = struct.unpack('<i', struct.pack('<I', appid))[0]
    users = [p for p in (steam_root() / 'userdata').iterdir() if p.name.isdigit() and p.name != '0']
    if not users:
        raise SystemExit('No Steam accounts found in userdata.')
    for user in users:
        config = user / 'config'
        config.mkdir(exist_ok=True)
        vdf = config / 'shortcuts.vdf'
        shortcuts = {}
        if vdf.exists() and vdf.stat().st_size:
            shutil.copy2(vdf, vdf.with_suffix('.vdf.bak'))
            shortcuts = read_map(vdf.read_bytes(), 1 + len('shortcuts') + 1)[0]
        kept = [s for s in shortcuts.values() if s.get('AppName', s.get('appname')) != NAME]
        kept.append({'appid': signed, 'AppName': NAME, 'Exe': exe, 'StartDir': f'"{game}"',
                     'icon': str(game / 'steam/icon.png'), 'ShortcutPath': '', 'LaunchOptions': OPTIONS,
                     'IsHidden': 0, 'AllowDesktopConfig': 1, 'AllowOverlay': 1, 'OpenVR': 0, 'Devkit': 0,
                     'DevkitGameID': '', 'DevkitOverrideAppID': 0, 'LastPlayTime': 0, 'FlatpakAppID': '',
                     'tags': {}})
        data = b'\x00shortcuts\0' + write_map({str(i): s for i, s in enumerate(kept)}) + b'\x08'
        tmp = vdf.with_suffix('.vdf.tmp')
        tmp.write_bytes(data)
        os.replace(tmp, vdf)
        grid = config / 'grid'
        grid.mkdir(exist_ok=True)
        for source, suffix in ART.items():
            shutil.copyfile(game / 'steam' / source, grid / f'{appid}{suffix}')
        print(f'Steam account {user.name}: added {NAME} (shortcut {appid}) with library art')


if __name__ == '__main__':
    main()
