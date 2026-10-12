#!/usr/bin/env python3
"""Boot a relocated release with clean saves and without build-time library paths."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import sys

package = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='world-warrior-release-') as folder:
    stage = Path(folder) / 'game'
    shutil.copytree(package, stage)
    saves = Path(folder) / 'save-test'; saves.mkdir()
    if list(stage.glob('*.app')):
        exe = next(stage.glob('*.app/Contents/MacOS/world-warrior-smw'))
    elif (stage / 'Super Mario World II.exe').exists():
        exe = stage / 'Super Mario World II.exe'
    else:
        exe = stage / 'world-warrior-smw'
    env = {k: v for k, v in os.environ.items() if not k.startswith(('DYLD_', 'LD_LIBRARY'))}
    if os.name == 'nt':
        env['PATH'] = str(Path(os.environ['SystemRoot']) / 'System32') + ';' + os.environ['SystemRoot']
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WW_SAVE_DIR=str(saves))
    result = subprocess.run([str(exe), '--smoke-test'], cwd=folder, env=env, capture_output=True, text=True, timeout=90)
    print(result.stdout, result.stderr)
    assert result.returncode == 0, result.returncode
    assert 'Package smoke:' in result.stdout
    assert (saves / 'smw.srm').stat().st_size == 2048
    # A second clean process must read the save, and retain the former copy.
    subprocess.run([str(exe), '--smoke-test'], cwd=folder, env=env, check=True, timeout=90)
    assert (saves / 'smw.srm.bak').stat().st_size == 2048
    print('Relocated package boots, loads original data and writes/reloads save RAM.')
