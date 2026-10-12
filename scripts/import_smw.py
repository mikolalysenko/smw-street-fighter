#!/usr/bin/env python3
"""Validate a user-supplied ROM and run the pinned upstream asset extractor."""
import hashlib
from pathlib import Path
import subprocess
import sys
import shutil
import tempfile

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
    # The pinned extractor only included these four entrance tables for
    # Lunar Magic ROMs, although the vanilla loader reads them on every
    # secondary (return-pipe) entrance too. Patch a temporary tool copy;
    # retain the pinned upstream checkout unchanged.
    from prepare_smw import replace_once
    with tempfile.TemporaryDirectory(prefix='smw-extract-') as folder:
        copied=Path(folder)/'assets'
        shutil.copytree(tool.parent,copied)
        compiler=copied/'compile_resources.py';source=compiler.read_text()
        for name,pointer,address in (('kLm5FE00','0x5DC81','0x5FE00'),
                                     ('kLevelInfo_05F800','0xde191','0x5F800'),
                                     ('kLevelInfo_05FA00','0xde198','0x5FA00'),
                                     ('kLevelInfo_05FC00','0xde19f','0x5FC00')):
            size='512' if name=='kLm5FE00' else '0x200'
            old=f"add_asset_uint8('{name}', util.get_bytes(get_24({pointer}), {size}) if lm_load_level else b'')"
            new=f"add_asset_uint8('{name}', util.get_bytes(get_24({pointer}) if lm_load_level else {address}, {size}))"
            source=replace_once(source,old,new)
        compiler.write_text(source)
        subprocess.run([sys.executable,str(copied/'restool.py'),'--rom',str(path),'--no-include-rom'],cwd=output,check=True)
    print(f'Extracted {output / "smw_assets.dat"}; run make smw, then ./scripts/play-smw.sh')


if __name__ == '__main__':
    main()
