"""Read three original Capcom samples from a pinned community SND archive."""
from pathlib import Path
import hashlib
import json
import shutil
import struct
import subprocess
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parents[1]


def import_chunli_audio(output):
    manifest = json.loads((ROOT/'config/chunli-audio.json').read_text())
    archive = ROOT/'local/chunli-source.7z'
    if not archive.exists():
        archive.write_bytes(urlopen(manifest['url'], timeout=30).read())
    if hashlib.sha256(archive.read_bytes()).hexdigest() != manifest['sha256']:
        raise ValueError('Chun-Li audio archive hash mismatch')
    tar = shutil.which('bsdtar') or shutil.which('tar')
    data = subprocess.check_output([tar, '-xOf', str(archive), manifest['member']])
    if data[:12] != b'ElecbyteSnd\0':
        raise ValueError('Expected SND audio archive')
    count, offset = struct.unpack_from('<II', data, 16)
    found = set()
    for _ in range(count):
        following, size, group, index = struct.unpack_from('<4I', data, offset)
        for clip in manifest['clips']:
            if (group, index) != (clip['group'], clip['index']):
                continue
            sample = data[offset+16:offset+16+size]
            if hashlib.sha256(sample).hexdigest() != clip['sha256']:
                raise ValueError('Chun-Li sample hash mismatch')
            (output/f"{clip['name']}.wav").write_bytes(sample)
            found.add(clip['name'])
        offset = following
    if len(found) != len(manifest['clips']):
        raise ValueError('Missing Chun-Li samples')
