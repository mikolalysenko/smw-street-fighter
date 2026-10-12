# Build and development

## Build from a fresh checkout

Requirements: C/C++ compilers, Make, SDL2 development files, Git, Python 3 with
venv support, CMake, ffmpeg and bsdtar (libarchive-tools on Linux). On macOS: `brew install sdl2 cmake ffmpeg` and Xcode command
line tools. On Debian/Ubuntu install `build-essential libsdl2-dev python3-venv cmake ffmpeg libarchive-tools`.
For playing on Bazzite, use the [TV setup guide](../docs/BAZZITE.md).

Clone this repository with its submodules, or initialize them in an existing
checkout before building:

```sh
git submodule update --init
make original-assets
make smw
make run
```

`make original-assets` builds the pinned public SMW disassembly, checks that the
result exactly matches the USA ROM, and extracts the original game data. It also
downloads checksum-pinned community SF2 sprite sheets and WAV rips and packs them
for the renderer. This works without the NAS. Source details, credits and the
precise scope of the art are in [ASSETS.md](ASSETS.md).

Alternatively, use your existing SMW USA ROM:

```sh
make assets ROM="/path/to/Super Mario World (USA).sfc"
make build/asset-tools/bin/python3
build/asset-tools/bin/python3 scripts/import_community_sf2.py
make smw
```

Game data, downloads and generated builds remain in ignored `local/`, `build/`
and `dist/` on `main`. The prepared `gh-pages` branch contains the browser runtime
and its packed game data. `make package` creates a complete bundle for the current
platform, including runtime assets and shared libraries. Linux packaging requires
`patchelf`; Windows uses MSYS2 with the MINGW64 GCC/SDL2 toolchain. Mac packages
bundle SDL2 and SDL3 and are ad-hoc signed. The CI workflow builds Linux x86-64,
Windows x86-64, and separate Apple Silicon and Intel Mac packages on native runners.

```sh
make smw
python3 dev/package_native.py --version 0.6.2
python3 dev/smoke_package.py dist/releases/super-mario-world-ii-macos-arm64
```

Use the matching platform/architecture folder for the smoke check. It relocates
the package, removes build-time library paths, boots twice and verifies save RAM.
It does not use or modify personal saves. Desktop saves use SDL's per-user data
directory; `WW_SAVE_DIR` is an isolated test override.

`python3 dev/package_bazzite.py [linux.tar.gz]` repackages the Linux x86-64 build
with Steam library art from `media/steam` (regenerate with
`dev/media/steam_art.py`) and an `install.sh` that installs to `~/Games` and
adds a non-Steam shortcut with that art.

## Tests and publishing

```sh
make test sanitize test-smw              # Core tests and SMW hooks; no game data needed
make test-animations test-audio          # Requires prepared original assets
make test-play test-bosses test-movement # Replays against the original game
make test-final-depth                   # Boss phases, counters and native ending
make web test-web                       # Build and exercise the actual WASM module
make pages-branch                       # Build, test, and commit a local gh-pages update
```

Run native replay targets serially because they share a generated build tree.
`make pages-branch` prepares a local branch without pushing or changing the
current checkout. Start from committed source; the deployment includes its
source revision and build timestamp. [PUBLISHING.md](PUBLISHING.md) covers
the first push and later updates.

The shared combat and drawing code is in `src/`, engine hooks in `adapters/smw/`,
and the WASM interface in `adapters/web/`. `web/` supplies the browser shell;
`scripts/` builds and imports the pinned assets. `references/` contains pinned
upstream submodules.

Browser audio lives in `web/audio.mjs`. The Play gesture requests a playback
Audio Session, resumes the context and starts a silent primer synchronously.
Older iOS uses a silent media element to request the same playback category.
Muting, backgrounding and watching the teaser release the session. Failed or
stalled resumes expose an Enable Sound retry instead of stopping the game.
This follows [WebKit's explanation of ambient vs. playback audio](https://bugs.webkit.org/show_bug.cgi?id=252746)
and the [Audio Session API](https://www.w3.org/TR/audio-session/).
`make test-web` includes lifecycle regressions. Optional
`node tests/test_browser.mjs` runs the built game in WebKit and Chromium using
Playwright installed under `build/browser-tools`. It checks non-silent audio
buffers, mute/resume and phone controls; a physical iPhone speaker and silent
switch still need device testing.

New code and documentation are MIT licensed. Original game content retains its
separate rights and credits; see [asset provenance](ASSETS.md) and [LICENSE](../LICENSE).

