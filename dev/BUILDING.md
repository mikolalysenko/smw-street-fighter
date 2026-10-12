# Build and development

## Build from a fresh checkout

Requirements: C/C++ compilers, Make, SDL2 development files, Git, Python 3 with
venv support, CMake, ffmpeg and bsdtar (libarchive-tools on Linux). On macOS: `brew install sdl2 cmake ffmpeg` and Xcode command
line tools. Linux/Bazzite instructions are in [BAZZITE.md](BAZZITE.md).

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
and its packed game data. `make package` creates a bundle for the current platform. The Mac
bundle uses the installed SDL library; the Linux bundle should run in its build
Distrobox until a portable release is prepared.

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

New code and documentation are MIT licensed. Original game content retains its
separate rights and credits; see [asset provenance](ASSETS.md) and [LICENSE](LICENSE).

