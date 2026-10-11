# World Warrior in Dinosaur Land

A native C/SDL2 crossover prototype: Super Mario World's original levels,
scenery, enemies, music and platforming, with ripped Street Fighter II fighter
sprites, voices and impacts. Select Ryu, E. Honda, Chun-Li or Guile and use
specials and a three-level, Alpha-inspired super gauge.

The actual SMW game is the default build. The earlier placeholder dojo is only
a combat test fixture. This is a native game, not a SNES ROM patch.

## Play on this Mac

Open `dist/World Warrior SMW.app`, or:

```sh
cd /Users/mikolalysenko/smw-street-fighter
make run
# Fullscreen:
make tv
```

Start a one-player game using Enter or controller Start. Pick your fighter at
character select; use the normal SMW overworld to enter a level. Each life starts
with one super bar. Hits refill it; supers spend one to three complete bars.
Health refills on level entry and meter carries between levels. Start opens
Resume / Character Select / Quit Game during play.

| Action | Xbox/Steam gamepad | Keyboard |
| --- | --- | --- |
| Move / menu | D-pad or left stick | Arrows |
| Jump / confirm | A | Z |
| Punch | X | X |
| Kick | B | C |
| Special | Y | A |
| Secondary special | Up or Down + Y | Up or Down + A |
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

Ryu also accepts quarter-circle forward + punch, the dragon-punch motion,
and double quarter-circle forward + punch. Honda and Guile accept a 30-frame
horizontal charge then the opposite direction + punch; Guile accepts down
charge, up + kick. Honda's repeated punches and Chun-Li's repeated kicks trigger
rapid attacks. Landed normals can cancel into specials; landed attacks can
cancel into supers. These are forgiving platformer adaptations, not arcade
frame data. Sonic Hurricane is a later-series move adapted to this prototype.

Enemy contact causes health loss, hitstop, hit stun, knockback and SF2 hurt
sounds. KO uses ripped SF2 screams. Ordinary whitelisted enemies die with launch
knockback from attacks; stomps and the original level logic still work.

## Build from a fresh checkout

Requirements: C/C++ compilers, Make, SDL2 development files, Git, Python 3 with
venv support, and CMake. On macOS: `brew install sdl2 cmake` and Xcode command
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

Boss combat, foreground occlusion of the fighter, terrain collision for
projectiles, and complete swimming/climbing/Yoshi/powerup behavior remain.
The fighter keeps Mario's smaller terrain collision box. Chun-Li's basic
standing/walking/punch sequences currently reuse guard frames from an incomplete
sheet; her special, jump, hurt and KO frames are original rips. Other animation
clips also use adapted timings and anchors. The original title/overworld still
show Mario. Snapshot/replay hotkeys are disabled because they omit combat state.

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
```

The replay produces `selection.bmp`, `original-level.bmp`, `super.bmp` and
`ko.bmp` under `build/smw/`. Source submodules stay unmodified; the patch script
creates a separate build tree and refuses missing or ambiguous hook matches.
