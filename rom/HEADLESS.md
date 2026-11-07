# Headless SNES verification

The ROM was exercised with [`snesref`](https://github.com/mstan/snesrecomp)
and the real Snes9x libretro core, including scripted input, framebuffer dumps,
and final WRAM assertions. This is optional; `make test-rom` has no emulator
dependency.

## Tools used for the recorded checks

- `snesref` source revision
  [`d1f7edf4f12d75be879c356f5d69949f0ea832a5`](https://github.com/mstan/snesrecomp/commit/d1f7edf4f12d75be879c356f5d69949f0ea832a5).
- Snes9x revision
  [`b5cc7651f9fc02189cb51b5a43848877db5aec42`](https://github.com/snes9xgit/snes9x/commit/b5cc7651f9fc02189cb51b5a43848877db5aec42).
- SDL2 and a C++ compiler for the small frontend.

Example local build (paths match the commands used during this port):

```sh
git clone https://github.com/mstan/snesrecomp.git build/snesrecomp-src
git -C build/snesrecomp-src checkout d1f7edf4f12d75be879c356f5d69949f0ea832a5
c++ -std=c++11 -O2 -o build/snesref \
  build/snesrecomp-src/tools/snesref/frontend.cpp \
  $(sdl2-config --cflags --libs) -ldl

git init build/snes9x-libretro
git -C build/snes9x-libretro remote add origin https://github.com/snes9xgit/snes9x.git
git -C build/snes9x-libretro fetch --depth 1 origin \
  b5cc7651f9fc02189cb51b5a43848877db5aec42
git -C build/snes9x-libretro checkout --detach FETCH_HEAD
make -C build/snes9x-libretro/libretro -j4
```

The emulator and core are test-only downloads, not game dependencies. Remove
those build directories when finished.

## Gameplay scene

```sh
make rom
mkdir -p local/rom-play
SNESREF_HEADLESS=1 \
SNESREF_WRAM_FILL=0 \
SNESREF_SCRIPT=rom/scenes/play.scene \
SNESREF_DUMP_DIR=local/rom-play \
SNESREF_WRAM_DUMP=local/rom-play/final-wram.bin \
  build/snesref \
  build/snes9x-libretro/libretro/snes9x_libretro.dylib \
  local/smw-world-warrior.sfc
python3 rom/verify_headless.py
```

The scene uses the actual title/file/player menus, waits through the original
intro, moves right on the overworld, enters Yoshi's Island 1, and exercises every
implemented move. It captures `*.fb.bmp`, `*.info.json`, and `*.wram.bin` files.
The two images in `rom/README.md` come from this scene.

The checks assert move IDs, an active normal projectile, an active super
projectile, and complete meter consumption. They do not claim a full-game
completion test.

## Full-roster, ending, and finale scenes

Run the all-fighter move matrix and compare the expected character/move IDs:

```sh
mkdir -p local/rom-all-roster
SNESREF_HEADLESS=1 SNESREF_WRAM_FILL=0 \
SNESREF_SCRIPT=rom/scenes/all-roster.scene \
SNESREF_DUMP_DIR=local/rom-all-roster \
  build/snesref build/snes9x-libretro/libretro/snes9x_libretro.dylib \
  local/smw-world-warrior.sfc
python3 -I rom/verify_headless.py --all-roster
```

`all-endings.scene` uploads every ending page. The verifier compares all
28,672 tile bytes per page at VMADD `$4000`, the tilemap at `$7800`, and each
page's CGRAM palette:

```sh
mkdir -p local/rom-endings
SNESREF_HEADLESS=1 SNESREF_WRAM_FILL=0 \
SNESREF_SCRIPT=rom/scenes/all-endings.scene \
SNESREF_DUMP_DIR=local/rom-endings \
  build/snesref build/snes9x-libretro/libretro/snes9x_libretro.dylib \
  local/smw-world-warrior.sfc
python3 -I rom/verify_headless.py --endings
```

`finale.scene` enters the C7 Akuma fight, performs an aligned boss attack, and
forces the boss-defeat transition. `roster.scene` additionally covers Chun-Li
selection, moves, and a buffered super:

```sh
mkdir -p local/rom-finale local/rom-roster
SNESREF_HEADLESS=1 SNESREF_WRAM_FILL=0 \
SNESREF_SCRIPT=rom/scenes/finale.scene \
SNESREF_DUMP_DIR=local/rom-finale \
  build/snesref build/snes9x-libretro/libretro/snes9x_libretro.dylib \
  local/smw-world-warrior.sfc
SNESREF_HEADLESS=1 SNESREF_WRAM_FILL=0 \
SNESREF_SCRIPT=rom/scenes/roster.scene \
SNESREF_DUMP_DIR=local/rom-roster \
  build/snesref build/snes9x-libretro/libretro/snes9x_libretro.dylib \
  local/smw-world-warrior.sfc
python3 -I rom/verify_headless.py --roster
```

These scenes also leave APURAM, OAM, CGRAM, framebuffer and WAV captures for
inspection. They exercise native CPU/PPU/SPC execution rather than an overlay.

## Title and roster presentation

`presentation.scene` runs the native title, file menu, six-by-two roster,
selection blink/navigation, intro, overworld, and gameplay return. The verifier
compares the exact menu tileset and every captured tilemap, including the
LoROM-bank-padded second-row pages, and checks that the selected fighter persists
into gameplay:

```sh
mkdir -p local/rom-presentation
SNESREF_HEADLESS=1 SNESREF_WRAM_FILL=0 \
SNESREF_SCRIPT=rom/scenes/presentation.scene \
SNESREF_DUMP_DIR=local/rom-presentation \
  build/snesref build/snes9x-libretro/libretro/snes9x_libretro.dylib \
  local/smw-world-warrior.sfc
python3 -I rom/verify_headless.py --presentation
```

## Native carrying, power-ups, and Map16

The carry scene uses an independent input event to keep Guard+Punch held through
native sprite contact, then releases it before collecting a mushroom. Its WRAM
checks require carried status `$0B`, both native carry flags, raw Y/L input, and
the mushroom's meter award. The Map16 scene checks the complete attack/lookup/
bounce-sprite/return sequence:

```sh
mkdir -p local/rom-carry local/rom-map16
SNESREF_HEADLESS=1 SNESREF_WRAM_FILL=0 \
SNESREF_INPUT_FILE=rom/scenes/carry.input \
SNESREF_SCRIPT=rom/scenes/carry.scene \
SNESREF_DUMP_DIR=local/rom-carry \
  build/snesref build/snes9x-libretro/libretro/snes9x_libretro.dylib \
  local/smw-world-warrior.sfc
SNESREF_HEADLESS=1 SNESREF_WRAM_FILL=0 \
SNESREF_SCRIPT=rom/scenes/map16.scene \
SNESREF_DUMP_DIR=local/rom-map16 \
  build/snesref build/snes9x-libretro/libretro/snes9x_libretro.dylib \
  local/smw-world-warrior.sfc
python3 -I rom/verify_headless.py --carry
```

These checks use ordinary WRAM captures and do not require emulator debug
modifications.

## Collision fixtures

```sh
mkdir -p local/rom-combat
SNESREF_HEADLESS=1 \
SNESREF_WRAM_FILL=0 \
SNESREF_SCRIPT=rom/scenes/combat.scene \
SNESREF_DUMP_DIR=local/rom-combat \
  build/snesref \
  build/snes9x-libretro/libretro/snes9x_libretro.dylib \
  local/smw-world-warrior.sfc
```

This uses a loaded native sprite and harness `forcepoke` commands to keep a
target present in the attack path. It verifies projectile score/meter and melee
contact. The melee fixture deliberately resurrects its target during active
frames, so its score is greater than one hit; normal production targets stay in
killed status instead. The artificial target is not a native player-contact
damage test.

## Damage and parry fixtures

`scenes/damage.scene` and `scenes/parry.scene` are for **test-only debug ROMs**
that invoke `WW_Hurt` from a temporary trigger. They are not intended for the
production build. The recorded checks covered:

- unblocked: 100 → 80 HP and +8 meter;
- guarded sequence: chip damage after the fresh-guard window;
- fresh L press: no damage and +24 meter.

`python3 rom/verify_headless.py --all` checks all recorded WRAM captures after
these fixtures have been run. The debug trigger is intentionally not included
in the shipping assembly.

Real console/flash-cart testing remains outstanding. The captures prove
execution under Snes9x's SNES CPU/PPU emulation, not compatibility with every
emulator or physical cartridge setup.
