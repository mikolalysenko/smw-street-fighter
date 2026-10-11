#!/usr/bin/env python3
"""Import sf2ua from a local merged/split MAME ZIP or ROM directory.

Match contents by SHA-1, so both historical and current MAME filenames work.
Sibling sf2.zip is inspected for shared ROMs in split clone sets.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
CODE = [('sf2u.30a', 'sf2u.37a'), ('sf2u.31a', 'sf2u.38a'),
        ('sf2u.28a', 'sf2u.35a'), ('sf2_29a.bin', 'sf2_36a.bin')]
GFX = [('sf2-5m.4a', 'sf2-7m.6a', 'sf2-1m.3a', 'sf2-3m.5a'),
       ('sf2-6m.4c', 'sf2-8m.6c', 'sf2-2m.3c', 'sf2-4m.5c'),
       ('sf2-13m.4d', 'sf2-15m.6d', 'sf2-9m.3d', 'sf2-11m.5d')]
EXPECTED = {'allroms.bin': '4256ec60bf9eec21f4d6bb34c38990a9401af82e',
            'sf2gfx.bin': 'db52a6314b4c0cd4c48eb324720c83dd142c3bff'}


def interleave(parts, width):
    if not parts or width <= 0 or len({len(p) for p in parts}) != 1 or len(parts[0]) % width:
        raise ValueError('Interleave needs equal, whole-width input lanes.')
    output = bytearray(sum(map(len, parts)))
    for lane, part in enumerate(parts):
        for byte in range(width):
            output[lane * width + byte::len(parts) * width] = part[byte::width]
    return bytes(output)


def prepare(source, output):
    specs = json.loads((ROOT / 'config/sf2ua-roms.json').read_text())['roms']
    wanted = {r['sha1']: r for r in specs}
    blobs = {}
    paths = list(source.rglob('*')) if source.is_dir() else [source]
    if source.is_file():
        paths += [p for p in (source.parent / 'sf2.zip', source.parent / 'sf2ua.zip') if p.exists() and p != source]
    sizes = {r['size'] for r in specs}
    crcs = {int(r['crc32'], 16) for r in specs}
    def accept(blob):
        digest = hashlib.sha1(blob).hexdigest()
        if digest in wanted:
            blobs[digest] = blob
    for path in paths:
        if not path.is_file(): continue
        if path.suffix.lower() == '.zip':
            with zipfile.ZipFile(path) as archive:
                for entry in archive.infolist():
                    if entry.file_size in sizes and entry.CRC in crcs:
                        accept(archive.read(entry))
        elif path.stat().st_size in sizes:
            accept(path.read_bytes())
    missing = [r['name'] for r in specs if r['sha1'] not in blobs]
    if missing:
        raise ValueError('Missing verified sf2ua ROMs: ' + ', '.join(missing) + '. Supply sf2ua plus parent sf2, or a merged sf2.zip.')
    parts = [blobs[r['sha1']] for r in specs]
    data = {
        'allroms.bin': b''.join(interleave(parts[i:i+2], 1) for i in range(0, 8, 2)),
        'sf2gfx.bin': b''.join(interleave(parts[i:i+4], 2) for i in range(8, 20, 4)),
    }
    for name, blob in data.items():
        digest = hashlib.sha1(blob).hexdigest()
        if digest != EXPECTED[name]:
            raise ValueError(f'{name}: expected sf2ua SHA-1 {EXPECTED[name]}, got {digest}')
    output.mkdir(parents=True, exist_ok=True)
    for name, blob in data.items():
        (output / name).write_bytes(blob)
        print(f'{output / name}: {len(blob)} bytes, SHA-1 {EXPECTED[name]}')
    (output / 'sound.bin').write_bytes(parts[20])
    (output / 'oki.bin').write_bytes(parts[21] + parts[22])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('romset', type=Path, help='Local sf2ua folder or ZIP using the pinned sf2ww filenames')
    args = parser.parse_args()
    try:
        prepare(args.romset.expanduser(), ROOT / 'local/sf2')
    except (OSError, ValueError, zipfile.BadZipFile, KeyError) as exc:
        raise SystemExit(str(exc)) from exc
