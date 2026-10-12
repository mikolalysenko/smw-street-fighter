#!/usr/bin/env python3
"""Commit a tested dist/web build to a local gh-pages branch. Never push."""
from pathlib import Path
import json
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SITE_FILES = (
    'index.html', 'style.css', 'app.mjs', 'input.mjs', 'presenter.mjs', 'touch.mjs', 'audio.mjs',
    'game.mjs', 'game.wasm', 'game.data', 'build.json', '.nojekyll',
    'README.txt', 'LICENSE.txt', 'SMW-LICENSE.txt', 'ASSETS.md',
) + tuple(json.loads((ROOT / 'dev/site_files.json').read_text()))


def git(*args, cwd=ROOT):
    return subprocess.check_output(['git', *args], cwd=cwd, text=True).strip()


def main():
    if git('status', '--porcelain'):
        raise SystemExit('Commit source changes before preparing gh-pages.')
    source = git('rev-parse', 'HEAD')
    site = ROOT / 'dist/web'
    for name in SITE_FILES:
        path = site / name
        if not path.is_file() or path.is_symlink():
            raise SystemExit(f'Missing built file: {name}. Run make pages-branch.')
        if path.stat().st_size >= 100 * 1024 * 1024:
            raise SystemExit(f'{name} exceeds GitHub\'s single-file limit.')
    info = json.loads((site / 'build.json').read_text())
    if info['source_revision'] != source or info['source_dirty']:
        raise SystemExit('The web build does not match committed HEAD. Run make pages-branch.')
    for name in ('index.html', 'app.mjs'):
        if '__BUILD__' in (site / name).read_text():
            raise SystemExit(f'Unversioned build placeholder in {name}.')
    with (site / 'game.wasm').open('rb') as wasm:
        if wasm.read(8) != b'\0asm\x01\0\0\0':
            raise SystemExit('Invalid WebAssembly module.')

    branch_exists = subprocess.run(
        ['git', 'show-ref', '--verify', '--quiet', 'refs/heads/gh-pages'], cwd=ROOT,
    ).returncode == 0
    with tempfile.TemporaryDirectory(prefix='pages-', dir=ROOT / 'build') as folder:
        worktree = Path(folder) / 'site'
        args = ['worktree', 'add']
        args += [str(worktree), 'gh-pages'] if branch_exists else ['--orphan', '-b', 'gh-pages', str(worktree)]
        git(*args)
        try:
            # Only this generated, temporary worktree is replaced. Source files,
            # local assets and saves in the user's checkout are never touched.
            git('rm', '-rf', '--ignore-unmatch', '.', cwd=worktree)
            for name in SITE_FILES:
                (worktree / name).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(site / name, worktree / name)
            git('add', '--all', cwd=worktree)
            changed = subprocess.run(
                ['git', 'diff', '--cached', '--quiet'], cwd=worktree,
            ).returncode
            if changed:
                git('commit', '-m', f'Prepare WASM demo {info["build"]} from {source[:8]}', cwd=worktree)
            print(f'Local gh-pages ready: {git("rev-parse", "--short", "HEAD", cwd=worktree)}')
            print(f'Build {info["build"]}; source {source[:8]}. Nothing was pushed.')
        finally:
            git('worktree', 'remove', '--force', str(worktree))


if __name__ == '__main__':
    main()
