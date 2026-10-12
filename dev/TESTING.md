# Testing

## Verify

```sh
make test        # Combat, sprite loader/rendering, ROM/tool checks
make sanitize    # Combat and sprite tests under ASan/UBSan
make test-smw    # Actual SMW RAM/clipping/damage/menu hooks; no assets needed
make test-play   # Original game boot + level/combat replay; assets required
make test-bosses # Ten actual boss rooms, attacks, KO and native endings
make test-movement # Jump/air attacks, Continue and original goal tape
make test-presentation # Moving platforms, Yoshi and new/resumed file selection
make test-hud    # Native timer/pause, coins, lives, meter pickups, max counters and HUD captures
make test-roster # All twelve fighters: 17 ground/air actions, Bison's stage exit
make test-collision # Pipe walls, both camera edges, rolls and slides
make test-pipes     # Original bonus-room return entrances
make test-specials  # Sustained attacks, horizontal headbutt, fireballs and beam
make test-features  # Normal/super blocks, all pipe clips, low guard and twelve endings
make test-scroll # Bison in the original scrolling smasher room, through its door
make test-animations # Packed art: both facings, strike timing, landing and freeze
make audit-animations # Sprite contact sheets and loops in build/animation-audit
make test-audio  # Voices, original Alpha charge and selection/continue music
make test-web    # Built WASM module through the original game flow
```

The replay produces `selection.bmp`, `original-level.bmp`, `super.bmp` and
`ko.bmp` under `build/smw/`. Source submodules stay unmodified; the patch script
creates a separate build tree and refuses missing or ambiguous hook matches.

Optional real-browser checks (start `make serve-web` first):

```sh
npm install --prefix build/browser-tools playwright@1.58.2
build/browser-tools/node_modules/.bin/playwright install chromium webkit
node tests/test_browser.mjs
```

These run the actual WASM build in WebKit and Chromium with phone, tablet and
desktop profiles, including touch-only game startup/combat, multi-touch,
rotation-friendly layouts, expansion, audio resume and the debug query flag.
They emulate phone browsers; physical iPhone testing remains separate.

The native packaging workflow also boots each relocated desktop package with a
clean user save directory and without developer library search paths. Windows
runs this check from PowerShell, outside MSYS2. Both engines share one audio
device; `test-audio` checks the common signed-16-bit mix and voice gains.
