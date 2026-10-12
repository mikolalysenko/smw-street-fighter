# Super Mario World II — The World Warrior

A C/SDL2 and WebAssembly crossover: Super Mario World's original levels,
scenery, enemies, music and platforming, with ripped Street Fighter II fighter
sprites, voices and impacts. All twelve classic SF2 fighters are selectable,
including the four bosses, with character specials and a three-level super gauge.

The actual SMW game is the default build. The earlier placeholder dojo is only
a combat test fixture. The native and browser builds share the same game engine.

## Play in a browser

```sh
make web          # Requires Emscripten and the prepared original assets
make serve-web
```

Open **http://localhost:8765**, click Play, and use the keyboard or a gamepad.
The original SMW engine, PPU and music run in WASM. WebGPU presents the pixel
framebuffer; Canvas 2D is the fallback. Fullscreen is available below the game.
Reload the page after rebuilding; the status shows the loaded build timestamp.
Original SMW save points and your high score persist in browser storage, with an Export Save button.
This saves normal game progress, not an instant snapshot of the current room.

Copy `dist/web/` to Linux/Bazzite and serve that folder on localhost to play
without recompiling. See [BAZZITE.md](docs/BAZZITE.md) for the TV setup.

## Play on this Mac

Open `dist/Super Mario World II - The World Warrior.app`, or:

```sh
cd /Users/mikolalysenko/smw-street-fighter
make run
# Fullscreen:
make tv
```

Start a one-player game using Enter or controller Start. Pick your fighter at
character select after choosing a save file, before the intro or overworld loads.
This also applies when resuming an existing save. Use the normal SMW overworld to
enter a level. Each life starts
with one super bar. Hits refill it; supers spend one to three complete bars.
Health refills on level entry and meter carries between levels. Start opens
Resume / Character Select / Akuma Battle / Quit Game during play. On death, the SF2 continue
screen shows the defeated fighter and a ten-second countdown. Start/Jump retries
the level; Back or letting the countdown expire returns to the map. Continues
are unlimited and retain the selected fighter.
Character selection plays the original SF2 selection music. The title combines
SMW's original lettering, scenery, demo and music with SF2's gold/red II and
blue “The World Warrior” subtitle. Course clear shows the selected fighter.
Clearing without taking damage displays PERFECT with the original announcer voice.

**Jump to the final fight:** enter any level, press Enter/Start, choose
**AKUMA BATTLE**, then confirm. You start with full health and three super bars.
Bowser makes his entrance wearing Bison's hat before Akuma takes over the clown
car. Continue retries this arena. Practice does not write game saves or records;
returning to the map restores the score and lives from before practice.

The title and gameplay HUD show your high score. SMW's normal points from
enemies, pickups and the course-clear tally count; combat kills award the native
combo points too. Consecutive combat hits award **200, 400, 800, 1000, 2000,
4000, 8000, 1UP, 2UP, then 3UP**. Further hits keep awarding 3UP, up to SMW's
99-life limit. Damage or a 90-frame gap ends the chain; Bison's continuous level
3 flight keeps it going. New records save immediately, survive new games and continues,
and travel with Export Save in the same `smw.srm` as level progress. The title
demo cannot set a record. Native builds keep it in `saves/smw.srm`.

| Action | Xbox/Steam gamepad | Keyboard |
| --- | --- | --- |
| Move / menu | D-pad or left stick | Arrows |
| Jump / confirm | A | Z |
| Punch | X | X |
| Kick | B | C |
| Crouching punch / kick | Down + X / B | Down + X / C |
| Special | Y | A |
| Secondary special | Up + Y | Up + A |
| Extra special / Hurricane Kick | Down + Y | Down + A |
| Super | RB | S |
| Guard | LB | D |
| Pause / confirm | Start | Enter |
| Back / pause | Back | Escape |
| Fullscreen | Launcher option | F11 |

SMW's introduction and message boxes accept Jump. On the overworld,
Start + Back together quits. Controller labels use SDL's Xbox button positions;
Steam Input can map other controllers to this layout.

| Fighter | Special | Secondary | Super |
| --- | --- | --- | --- |
| Ryu | Hadouken | Shoryuken | Shinku Hadouken |
| E. Honda | Sumo Headbutt | Hundred Hand Slap | Super Oni Musou |
| Chun-Li | Lightning Legs | Spinning Bird Kick | Senretsukyaku |
| Guile | Sonic Boom | Flash Kick | Sonic Hurricane |
| Ken | Hadouken | Shoryuken | Shoryureppa |
| Blanka | Rolling Attack | Vertical Roll | Ground Shave Roll |
| Zangief | Double Lariat | Screw Piledriver | Final Atomic Buster |
| Dhalsim | Yoga Fire | Yoga Flame | Yoga Inferno |
| Balrog | Dash Straight | Dash Uppercut | Crazy Buffalo |
| Vega | Rolling Crystal Flash | Flying Barcelona | Rolling Izuna Drop |
| Sagat | Tiger Shot | Tiger Uppercut | Tiger Genocide |
| M. Bison | Psycho Crusher | Head Press | Psycho Crusher |

Ryu and Ken can Hurricane Kick with **Down + Special**, or quarter-circle back
+ Kick, on the ground or in the air. Down + Special gives Blanka Electricity
and Dhalsim Teleport. Ryu, Ken and Sagat also accept quarter-circle forward +
Punch and the dragon-punch motion. Double quarter-circle forward + Punch
activates a super. Honda, Guile, Blanka, Balrog, Vega and Bison accept back charge
then forward + Punch; Guile, Blanka, Vega and Bison accept down charge then
up + Kick. Zangief accepts a three-quarter circle + Punch; Dhalsim accepts
half-circle forward + Punch. Honda/Blanka's repeated punches and Chun-Li's
repeated kicks trigger rapid attacks. Landed normals can cancel into specials; landed attacks can
cancel into supers. These are forgiving platformer adaptations, not arcade
frame data. Sonic Hurricane is a later-series move adapted to this prototype.

Bison's **three-bar Psycho Crusher** is intentionally ridiculous: invulnerable
flight, 999-damage hits, a huge original Alpha spin and blue-white wake, and a
combo that lasts until he lands. It flies to the actual goal or end of the room.
Up/Down steer in horizontal rooms; Left/Right steer in vertical rooms. **Guard
lands early**. Doors, pipes, exits and essential platforms keep working; a room
without a goal still needs its normal exit. The camera and sprite loader continue
running, so the native goal, score tally and overworld event finish normally.

Enemy contact causes health loss, hitstop, hit stun, knockback and SF2 hurt
sounds. Death uses ripped fighter screams. Punches, kicks, specials and supers
also work in the air, including pressing jump and attack together.
Holding Down gives each fighter original crouching punch/kick poses and low
hitboxes. Moving platforms preserve fighter visibility, and the original Yoshi
renders both on foot and while ridden. Health and super gauges float over the
level, without an opaque HUD strip.
Mounted Hadoukens pass Yoshi. Punch activates his tongue; automatic running does
not. Mushrooms restore 25 health without erasing damage from the PERFECT check.

All normal sprite IDs and extended/cluster hazards accept attacks, including
Chucks, Thwomps, wooden castle plungers and the layer-three crusher. Enemies
explode. Keys, exits, switches and essential platforms take a visual hit but
retain their function. Terrain shows a single impact without erasing routes.
Boss rooms have opposing health bars, a KO announcement and their original
defeat/exit sequences. In the finale, Akuma assassinates Bowser, throws him
upside down offscreen, and pilots the original clown car with faster sweeps,
dives and aimed fireballs. Peach still throws mushrooms. Defeating Akuma starts
the original rescue and ending.

## Build from a fresh checkout

Requirements: C/C++ compilers, Make, SDL2 development files, Git, Python 3 with
venv support, CMake and ffmpeg. On macOS: `brew install sdl2 cmake ffmpeg` and Xcode command
line tools. Linux/Bazzite instructions are in [BAZZITE.md](docs/BAZZITE.md).

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
precise scope of the art are in [ASSETS.md](docs/ASSETS.md).

Alternatively, use your existing SMW USA ROM:

```sh
make assets ROM="/path/to/Super Mario World (USA).sfc"
make build/asset-tools/bin/python3
build/asset-tools/bin/python3 scripts/import_community_sf2.py
make smw
```

Game data, downloads and generated builds remain in ignored `local/`, `build/`
and `dist/`. `make package` creates a bundle for the current platform. The Mac
bundle uses the installed SDL library; the Linux bundle should run in its build
Distrobox until a portable release is prepared.

## Current limits

The main SMW integration was built and visually checked on Apple Silicon.
An automated replay boots the original game, reaches Yoshi's Island 2, and
checks an attack, super, Goomba contact damage and KO using deterministic enemy
fixtures. This is not a full-game completion test.

Foreground occlusion and dedicated swimming/climbing/riding attack poses
remain. Yoshi's original rendering and mounting work; the fighter keeps Mario's
compact terrain collision box. Animation
timings and anchors are adapted for platforming; jumps use separate airborne
strike clips and walking advances with actual movement. Projectiles pass
through scenery, and essential objects survive explosions to preserve routes.
The original title/overworld still show Mario. Snapshot/replay hotkeys are
disabled because they omit combat state.

Real-engine replays verify Hadoukens, supers, KO and native endings in all seven
Koopaling rooms, Reznor, Big Boo and Bowser. Movement tests cover ten jump/attack
combinations, all Continue choices and goal-tape clear. Presentation tests cover
platform redraws, Yoshi standing/riding, message boxes, and character selection
before both new and saved games. A Node test boots the shipped WASM module and
checks original menus/music, movement, crouching attacks, air specials, supers,
audio and save RAM. These checks are not a full-game completion test.

Linux CI is configured but has not run in this local repository. The physical
Bazzite machine, controller and downstairs TV have not been tested.

## Source research

- [snesrev/smw](https://github.com/snesrev/smw): pinned C reimplementation used as
  the actual game host.
- [dasi/sf2ww](https://github.com/dasi/sf2ww): pinned, incomplete World Warrior
  arcade C rewrite, kept as a read-only research reference.
- [IsoFrieze/SMWDisX](https://github.com/IsoFrieze/SMWDisX): matching assembly source
  used to rebuild the original game data.

[sources.lock.json](sources.lock.json) records revisions.
[RESEARCH.md](docs/RESEARCH.md) maps the engine hooks and outstanding work.
`scripts/prepare_sf2.py /path/to/sf2ua.zip` also prepares checksum-verified CPS1
research inputs; the current renderer uses the extracted community sheets.

## Verify

```sh
make test        # Combat, sprite loader/rendering, ROM/tool checks
make sanitize    # Combat and sprite tests under ASan/UBSan
make test-smw    # Actual SMW RAM/clipping/damage/menu hooks; no assets needed
make test-play   # Original game boot + level/combat replay; assets required
make test-bosses # Ten actual boss rooms, attacks, KO and native endings
make test-movement # Jump/air attacks, Continue and original goal tape
make test-presentation # Moving platforms, Yoshi and new/resumed file selection
make test-roster # All twelve fighters and Bison's full-stage goal/exit replay
make test-audio  # Voices, original Alpha charge and selection/continue music
make test-web    # Built WASM module through the original game flow
```

The replay produces `selection.bmp`, `original-level.bmp`, `super.bmp` and
`ko.bmp` under `build/smw/`. Source submodules stay unmodified; the patch script
creates a separate build tree and refuses missing or ambiguous hook matches.
