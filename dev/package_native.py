#!/usr/bin/env python3
"""Package a native executable with runtime data, shared libraries and notices.

Never packages a developer's saves. All staging is under dist/releases/.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import platform
import plistlib
import re
import shutil
import stat
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TITLE = 'Super Mario World II - The World Warrior'
SLUG = 'super-mario-world-ii'


def run(*args):
    return subprocess.check_output(list(map(str, args)), text=True).strip()


def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
    target.chmod(target.stat().st_mode | stat.S_IWUSR)


def data_into(dest, source):
    required = ['smw_assets.dat', 'sf2/sprites.dat', 'sf2/audio/ko.wav']
    for name in required:
        if not (source / name).is_file():
            raise SystemExit('Missing runtime data: ' + str(source / name))
    copy(source / 'smw_assets.dat', dest / 'smw_assets.dat')
    copy(source / 'sf2/sprites.dat', dest / 'sf2/sprites.dat')
    for audio in sorted((source / 'sf2/audio').glob('*.wav')):
        copy(audio, dest / 'sf2/audio' / audio.name)


def macho_deps(file):
    return [line.strip().split(' (')[0] for line in run('otool', '-L', file).splitlines()[1:]]


def mac_libraries(exe, frameworks, notices):
    frameworks.mkdir(parents=True)
    pending = [exe]
    # SDL2-compat opens SDL3 dynamically, so it does not appear in otool -L.
    for dep in macho_deps(exe):
        if 'sdl2' in dep.lower():
            sdl3 = Path(run('brew', '--prefix', 'sdl3')) / 'lib/libSDL3.dylib'
            copy(sdl3, frameworks / 'libSDL3.dylib')
            pending.append(frameworks / 'libSDL3.dylib')
    processed = set()
    while pending:
        file = pending.pop()
        if file in processed:
            continue
        processed.add(file)
        for dep in macho_deps(file):
            if dep.startswith(('/System/', '/usr/lib/', '@')):
                continue
            source = Path(dep)
            target = frameworks / source.name
            # A dylib's first dependency is its own install name.
            if file.name == source.name and file != exe:
                continue
            if not target.exists():
                copy(source, target)
                pending.append(target)
            relative = '@executable_path/../Frameworks/' if file == exe else '@loader_path/'
            subprocess.run(['install_name_tool', '-change', dep, relative + target.name, str(file)], check=True)
        if file != exe:
            subprocess.run(['install_name_tool', '-id', '@loader_path/' + file.name, str(file)], check=True)
    for formula in ('sdl2-compat', 'sdl2', 'sdl3'):
        prefix = subprocess.run(['brew', '--prefix', formula], capture_output=True, text=True)
        if prefix.returncode == 0:
            folder = Path(prefix.stdout.strip()) / 'share/licenses'
            if folder.is_dir():
                shutil.copytree(folder, notices / formula, dirs_exist_ok=True)
    for file in processed:
        if any(dep.startswith(('/opt/homebrew/', '/usr/local/')) for dep in macho_deps(file)):
            raise SystemExit('Unbundled macOS dependency: ' + str(file))
        subprocess.run(['codesign', '--force', '--sign', '-', str(file)], check=True)


def linux_libraries(exe, lib, notices):
    lib.mkdir()
    # Keep the host's glibc/loader and graphics drivers. Bundle SDL and its
    # linked desktop/audio support libraries, without replacing host drivers.
    system = re.compile(r'^(ld-linux|lib(c|m|pthread|dl|rt|resolv|util|nss_[^.]+)\.so)')
    paths = set()
    for line in run('ldd', exe).splitlines():
        match = re.search(r'=> (/\S+)', line)
        if match and not system.match(Path(match[1]).name):
            paths.add(Path(match[1]))
        if 'not found' in line:
            raise SystemExit('Unresolved dependency: ' + line)
    for source in sorted(paths):
        copy(source, lib / source.name)
        query = subprocess.run(['dpkg-query', '-S', str(source)], capture_output=True, text=True)
        if not query.returncode:
            package = query.stdout.split(': ')[0].split(':')[0]
            copyright_file = Path('/usr/share/doc') / package / 'copyright'
            if copyright_file.exists():
                copy(copyright_file, notices / (package + '.copyright'))
    subprocess.run(['patchelf', '--set-rpath', '$ORIGIN/lib', str(exe)], check=True)
    for file in lib.iterdir():
        subprocess.run(['patchelf', '--set-rpath', '$ORIGIN', str(file)], check=True)


def windows_libraries(exe, dest, notices):
    prefix = Path(run('sdl2-config', '--prefix'))
    binary = prefix / 'bin'
    available = {p.name.lower(): p for p in binary.glob('*.dll')}
    pending, seen = [exe], set()
    # Also handles MSYS2 installations which provide SDL through SDL2-compat.
    if 'sdl3.dll' in available:
        copy(available['sdl3.dll'], dest / 'SDL3.dll')
        pending.append(dest / 'SDL3.dll')
    while pending:
        file = pending.pop()
        if file in seen:
            continue
        seen.add(file)
        for name in re.findall(r'DLL Name:\s*(\S+)', run('objdump', '-p', file)):
            if name.lower() in available and not (dest / name).exists():
                copy(available[name.lower()], dest / name)
                pending.append(dest / name)
    if (prefix / 'share/licenses').is_dir():
        for package in (prefix / 'share/licenses').iterdir():
            if any(word in package.name.lower() for word in ('sdl', 'gcc', 'winpthread', 'mingw')):
                shutil.copytree(package, notices / package.name, dirs_exist_ok=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--version', default='0.6.0')
    ap.add_argument('--data-only', action='store_true')
    args = ap.parse_args()
    out = ROOT / 'dist/releases'
    out.mkdir(parents=True, exist_ok=True)
    if args.data_only:
        stage = out / 'runtime-data'
        if stage.exists():
            shutil.rmtree(stage)
        data_into(stage, ROOT / 'local')
        manifest = {p.relative_to(stage).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in sorted(stage.rglob('*')) if p.is_file()}
        (stage / 'SHA256.json').write_text(json.dumps(manifest, indent=2) + '\n')
        shutil.make_archive(str(out / 'runtime-data'), 'zip', stage)
        print(out / 'runtime-data.zip')
        return
    system = platform.system()
    arch = 'arm64' if platform.machine().lower() in ('arm64', 'aarch64') else 'x86_64'
    osname = {'Darwin': 'macos', 'Linux': 'linux', 'Windows': 'windows'}.get(system)
    if osname is None and system.startswith(('MINGW', 'MSYS')):
        osname = 'windows'
    if not osname:
        raise SystemExit('Unsupported platform: ' + system)
    name = f'{SLUG}-{osname}-{arch}'
    stage = out / name
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir()
    notices = stage / 'licenses'
    notices.mkdir()
    if osname == 'macos':
        bundle = stage / (TITLE + '.app')
        contents = bundle / 'Contents'
        resources = contents / 'Resources'
        exe = contents / 'MacOS/world-warrior-smw'
        copy(ROOT / 'build/smw/smw', exe)
        mac_libraries(exe, contents / 'Frameworks', notices)
        info = {'CFBundleExecutable': exe.name, 'CFBundleIdentifier': 'net.mikolalysenko.supermarioworldii',
                'CFBundleName': TITLE, 'CFBundlePackageType': 'APPL', 'CFBundleVersion': args.version,
                'CFBundleShortVersionString': args.version, 'NSHighResolutionCapable': False}
        if (ROOT / 'media/app.icns').exists():
            copy(ROOT / 'media/app.icns', resources / 'app.icns')
            info['CFBundleIconFile'] = 'app.icns'
        (contents / 'Info.plist').write_bytes(plistlib.dumps(info))
    else:
        resources = stage
        windows = osname == 'windows'
        source = ROOT / ('build/smw/smw.exe' if windows else 'build/smw/smw')
        exe = stage / ('Super Mario World II.exe' if windows else 'world-warrior-smw')
        copy(source, exe)
        if windows:
            windows_libraries(exe, stage, notices)
        else:
            linux_libraries(exe, stage / 'lib', notices)
            launcher = stage / 'play.sh'
            launcher.write_text('#!/bin/sh\nset -eu\ncd -- "$(dirname -- "$0")"\n'
                                'config=smw.ini\nif [ "${1:-}" = --fullscreen ]; then config=tv.ini; shift; fi\n'
                                'exec ./world-warrior-smw --config "$config" "$@"\n')
            launcher.chmod(0o755)
    data_into(resources, ROOT / 'local')
    for config in ('smw.ini', 'tv.ini'):
        copy(ROOT / 'config' / config, resources / config)
    for source, name_in_package in [('LICENSE', 'LICENSE.txt'), ('references/smw/LICENSE.txt', 'SMW-engine-LICENSE.txt'),
                                    ('dev/ASSETS.md', 'ASSETS.md')]:
        copy(ROOT / source, notices / name_in_package)
    copy(ROOT / 'docs/PLAYING.md', stage / 'PLAYER-GUIDE.md')
    # Keep the release guide focused on playing; repository-relative links
    # point back to their online pages rather than missing developer files.
    readme = (ROOT / 'README.md').read_text()
    readme = re.sub(r'\]\((?!https?://|#)([^)]+)\)',
                    lambda m: '](https://github.com/mikolalysenko/smw-street-fighter/blob/main/' + m[1] + ')', readme)
    (stage / 'README.md').write_text(readme)
    (stage / 'START-HERE.txt').write_text(
        TITLE + '\nVersion ' + args.version + '\n\n'
        + {'macos': 'Drag the app to Applications, then open it. Keep this folder for its guide and credits.\n'
                    'This is an unsigned community build. If macOS blocks it, review it in System Settings > Privacy & Security.\n',
           'linux': 'Extract the whole archive, then run ./play.sh (or ./play.sh --fullscreen).\n'
                    'For Steam Gaming Mode, add play.sh as a non-Steam game and set --fullscreen in Launch Options.\n',
           'windows': 'Extract the whole ZIP, then open Super Mario World II.exe. Keep its data and DLL files alongside it.\n'
                      'This community build is not publisher-signed.\n'}[osname]
        + '\nEnter / Start starts. Z / A jumps and confirms. Arrows / D-pad moves.\n'
          'X punches, C kicks, A uses a special, S uses a super, D guards. F11 toggles fullscreen.\n'
          'A gamepad uses X punch, B kick, Y special, RB super, LB guard.\n\n'
          'Progress saves at original SMW save points; high scores save immediately.\n'
          'Saves live in your user account, outside this release folder; updates keep them.\n'
          'Read PLAYER-GUIDE.md for moves, secrets, saves and troubleshooting.\n\n'
          'https://mikolalysenko.github.io/smw-street-fighter/\n'
          'https://github.com/mikolalysenko/smw-street-fighter/issues\n')
    revision = run('git', '-C', ROOT, 'rev-parse', 'HEAD')
    (stage / 'build.json').write_text(json.dumps({'version': args.version, 'platform': osname,
        'architecture': arch, 'source_revision': revision}, indent=2) + '\n')
    # Notices also travel when just the .app is installed.
    if osname == 'macos':
        shutil.copytree(notices, resources / 'licenses')
        copy(stage / 'build.json', resources / 'build.json')
        subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(bundle)], check=True)
        subprocess.run(['codesign', '--verify', '--deep', '--strict', str(bundle)], check=True)
    if any(p.name == 'saves' or p.suffix in ('.srm', '.sav', '.sfc', '.smc') for p in stage.rglob('*')):
        raise SystemExit('Unexpected save or ROM in package')
    archive = shutil.make_archive(str(out / name), 'gztar' if osname == 'linux' else 'zip', out, name)
    print(archive)

if __name__ == '__main__':
    main()
