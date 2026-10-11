#!/usr/bin/env python3
"""Validate a user-supplied ROM and run the pinned upstream asset extractor."""
import hashlib
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SHA1 = '6b47bb75d16514b6a476aa0c73a683a2a4c18765'


def validate(data):
    if len(data) == 524800:
        data = data[512:]
    if len(data) != 524288 or hashlib.sha1(data).hexdigest() != SHA1:
        raise ValueError('Expected the unmodified USA SMW ROM (512 KiB, optionally a 512-byte copier header).')
    return data


def main():
    if len(sys.argv) != 2 or not sys.argv[1]:
        raise SystemExit('Usage: make assets ROM="/path/to/your/Super Mario World (USA).sfc"')
    path = Path(sys.argv[1]).expanduser().resolve()
    try:
        validate(path.read_bytes())
    except (OSError, ValueError) as exc:
        raise SystemExit(str(exc)) from exc
    tool = ROOT / 'references/smw/assets/restool.py'
    if not tool.exists():
        raise SystemExit('Run git submodule update --init references/smw first.')
    output = ROOT / 'local'
    output.mkdir(exist_ok=True)
    # No ROM copied into this repository. Asset data is gitignored.
    subprocess.run([sys.executable, str(tool), '--rom', str(path), '--no-include-rom'], cwd=output, check=True)
    print(f'Extracted {output / "smw_assets.dat"}; run make smw, then ./scripts/play-smw.sh')


if __name__ == '__main__':
    main()
