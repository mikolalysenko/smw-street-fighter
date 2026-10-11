# Integration findings

Research and implementation started 2026-10-11. The local repository contains
two pinned, unmodified source submodules and our separate combat implementation.

## Why the native C port fits

[snesrev/smw](https://github.com/snesrev/smw) owns the platform physics, overworld,
levels, sprite logic and a software SNES PPU/audio implementation. SDL2 provides
the platform window and controller layer. Its code license and third-party
notices are in `references/smw/LICENSE.txt`.

[dasi/sf2ww](https://github.com/dasi/sf2ww) preserves the MustardTiger/FistBlue
rewrite of the arcade World Warrior `sf2ua` version. It is a reverse-engineered
C rewrite, not a complete matching SNES SF2 decompilation. The README reports
unfinished behavior; the code couples fighter state to two-player rounds,
global game state and CPS graphics data. No repository-wide reuse license was
found; fighter files have the author's copyright notices. It is a read-only
reference here, not a runtime dependency.

The older `bentorkington/sf2ww` link did not resolve during this investigation.
The surviving `dasi` source is pinned at
`ec0818719ac0d9f6ecc35252e44231af7f8ae1e8`.

The SMW host is pinned at `eae20c65c58930c8b62c76188d259579ad4130f1`.
Its C function boundaries make a native mod practical. A ROM hack would also
need a 65816 combat implementation, OAM/VRAM budgeting, streaming fighter
animation tiles and SPC700 sound work. For Linux/Bazzite, that is extra work
without a hardware compatibility requirement. This prototype does not produce
a `.sfc` patch.

## Source map

Links use the pinned revisions so future upstream changes cannot move the target.

| Area | Source to inspect | Application here |
| --- | --- | --- |
| SMW level update, movement, damage, death, player graphics | [smw_00.c](https://github.com/snesrev/smw/blob/eae20c65c58930c8b62c76188d259579ad4130f1/src/smw_00.c) | Hook `DamagePlayer_Hurt`, `DamagePlayer_PitFall`, `UpdatePlayerSpritePosition`, `PlayerDraw` and built-in attacks |
| SMW sprite collision rectangles | [smw_03.c](https://github.com/snesrev/smw/blob/eae20c65c58930c8b62c76188d259579ad4130f1/src/smw_03.c) | Call the real Mario/sprite clipping helpers; no invented RAM offsets |
| SMW renderer and controller input | [main.c](https://github.com/snesrev/smw/blob/eae20c65c58930c8b62c76188d259579ad4130f1/src/main.c) | Intercept per-frame input, freeze on hitstop/menus, composite fighter/HUD into the output framebuffer |
| SMW reference-vs-native comparison | [common_cpu_infra.c](https://github.com/snesrev/smw/blob/eae20c65c58930c8b62c76188d259579ad4130f1/src/common_cpu_infra.c) | Force `RM_MINE`; intentional new mechanics cannot match the original ROM state |
| Ryu/Ken power-move states and inputs | [ryuken_human.c](https://github.com/dasi/sf2ww/blob/ec0818719ac0d9f6ecc35252e44231af7f8ae1e8/FistBlue/avatars/ryuken_human.c) | Reference for separating special startup, action and recovery |
| Honda human move logic | [ehonda_human.c](https://github.com/dasi/sf2ww/blob/ec0818719ac0d9f6ecc35252e44231af7f8ae1e8/FistBlue/avatars/ehonda_human.c) | Reference for charge and repeated-input move families |
| SF2 collision/reaction state | [collision.c](https://github.com/dasi/sf2ww/blob/ec0818719ac0d9f6ecc35252e44231af7f8ae1e8/FistBlue/collision.c), [playerstate.c](https://github.com/dasi/sf2ww/blob/ec0818719ac0d9f6ecc35252e44231af7f8ae1e8/FistBlue/playerstate.c) | Separate impact freeze, hit stun, invulnerability and KO |
| SF2 sound event identities | [sound.h](https://github.com/dasi/sf2ww/blob/ec0818719ac0d9f6ecc35252e44231af7f8ae1e8/FistBlue/sound.h) | Distinct move, impact, block, hurt and KO cues; sound IDs are not PCM samples |
| CPS1 ROM interleave | [mt2-merge.sh](https://github.com/dasi/sf2ww/blob/ec0818719ac0d9f6ecc35252e44231af7f8ae1e8/bin/mt2-merge.sh) | Reproduce and verify the expected code/graphics layout using local inputs |

## Implemented architecture

```text
Keyboard / SDL controller / browser Gamepad API
           |
           v
60 Hz combat core (src/fight.c)
  input history -> move states -> attack boxes/projectiles
  vitality / guard / hitstop / hit stun / meter / KO
           |
     +-----+-------------------------+
     |                               |
Dojo platformer              SMW C adapter
original test stage          original terrain, enemies, map
     |                               |
     +----------+--------------------+
                |
ripped SF2 fighter sprites / SF2 HUD art + imported WAV audio
                |
       SDL2 native or WASM + WebGPU presentation
```

The combat layer uses pixels and fixed 60 Hz steps. SMW stores speeds in
sixteenths of a pixel per tick; the adapter converts velocity overrides before
the host's position update, leaving terrain collision to SMW. The original
compact collision hull remains useful for navigating Mario-sized tunnels.

`scripts/prepare_smw.py` copies only tracked source files into `build/smw`, checks
the expected commit, and requires each source hook to match exactly once. It
does not mutate either reference checkout. The wrapper still runs the original
`PlayerDraw` for star/palette/pose side effects, restoring the OAM writes before
our overlay renders a fighter. Health damage hooks replace shrinking/death from
ordinary enemy contact; pits, time and crushing still use SMW's death transition.

`ww_targets.c` adapts all 12 normal, 10 extended and 20 cluster sprite slots.
Normal actors have one HP and an explosion replaces their species-specific
death code. Essential keys, exits, rides and switches keep functioning after
a visual hit. Full sprite initialization invalidates old per-slot hit masks.
Wooden plungers have their complete 80-pixel hull; the layer-three crusher
retracts on destruction. Terrain impacts leave Map16 routes intact.

Boss health adapts all seven Koopalings, Reznor, Big Boo and Bowser. Defeat
enters each native death state instead of erasing its controller. Reznor's
platforms remain until the native exit timer completes. Iggy/Larry's transformed
hitboxes use signed camera coordinates; an unsigned camera offset originally
placed them 65,536 pixels away. Bowser's offscreen death clamps his starting Y
so the unsigned native rise-to-death test can finish. Aggregate boss health
emits the KO sound once and supplies the opposing health bar.

Air attacks preserve SMW's held-jump arc and normal horizontal control.
Walking animation uses distance traveled, and jump/normal clips no longer
loop on the global clock. The Continue menu lets the native death finish,
then re-enters the current overworld node through SMW's normal midpoint loader.

The three-segment gauge, super flash and multi-hit rushes are new,
Alpha-inspired behavior. World Warrior itself is only the fighter-move
reference; no claim is made to extracting Alpha mechanics from that source.

## Original assets and demonstrated play

The NAS was unavailable, so the implementation now builds original SMW data
from pinned SMWDisX/Asar sources and verifies the USA ROM hash. The native host
loads that exact game's levels, graphics and audio. SF2 sprite sheets and WAV
rips come from the pinned Gamusinos Fighters asset tree. See [ASSETS.md](ASSETS.md)
for file hashes, credits and animation limitations.

The automated replay boots a fresh game, confirms the fighter, dismisses the
introduction, navigates the overworld into Yoshi's Island 2, attacks a deterministic
Goomba fixture, triggers a super, receives contact damage and enters KO. It uses
the actual level loader, enemy update, collision, framebuffer and main loop.
Temporary runtime directories isolate it from the player's SRAM saves.
Native visual checks also confirmed the original title, character selection,
Ryu and Mario World terrain. The initial integration fixes include preserving
short input taps, finding assets when launched from Finder, avoiding pause over
intro/message boxes, and keeping the fighter visible during super startup.

## Remaining work

1. Play through ordinary level exits, slopes, doors, pipes, checkpoints and the
   overworld. The replay covers a narrow combat slice, not complete progression.
2. Audit swimming, climbing, Yoshi, carried shells, stars, vertical rooms and
   pickups throughout the full game. Essential sprite interactions currently
   survive combat hits to preserve progression.
3. Add foreground occlusion and further animation refinements. Projectiles
   deliberately pass through scenery with one visual impact.
4. Expand the roster and add more super animations.
   The imported Alpha-inspired attacks currently use SF2 pose clips.
5. Test on the actual Bazzite machine with its TV and controller, including
   Steam Input, sound output, latency, refresh rate and reconnect behavior.
6. Add combat-state persistence before re-enabling snapshots/replay; make a
   portable Linux package after validating the native build on that machine.

## Verification scope

Built locally on Apple Silicon with Clang and SDL2. Combat tests cover both
facing directions, input expiry, charge thresholds, cancels, hit deduplication,
super piercing, meter costs, health, guard, knockback, KO and 50,000 stress inputs
under ASan/UBSan. Sprite tests check transparency, flipping, clipping and rejection
of malformed/truncated data. The bridge tests link the actual patched SMW objects
and exercise all 201 normal sprite IDs, extended/cluster targets, essential
objects, slot reuse, damage, transitions and menus. Asset-backed replays cover
the real boot and PPU, all ten boss rooms, ten jump/attack combinations,
Continue and goal clear. The audio test verifies the KO sample and selection
loop. The WASM test boots the compiled browser engine through Node and verifies
air specials, movement, audio, pixels and SRAM without replacing the C engine.

No full-game completion, physical gamepad/TV test or Linux execution has been
completed. The Linux workflow is configuration, not a reported CI result. The
repository is local; no Git remote was published and no downstairs machine was
changed.
