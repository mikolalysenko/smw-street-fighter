# Native roster graphics conversion

`make_roster_graphics.py` converts the pinned community/Alpha WWA1 pack into
SNES 4bpp data for the full roster. `roster_assets.asm` exposes the generated
art through 24-bit pointers, animation descriptors, frame metadata, palettes,
and OAM templates. It does not replace the current Ryu vertical-slice renderer.

## Regenerate

From the repository root, with the asset-tool Python environment:

```sh
build/asset-tools/bin/python3 -I rom/make_roster_graphics.py --self-test
build/asset-tools/bin/python3 -I rom/make_roster_graphics.py
```

The default input is `local/sf2/sprites.dat`. Outputs go to the ignored
`rom/generated/` directory, and `rom/roster_assets.asm` is regenerated with the
current record offsets and bank layout. `--sprites`, `--out`, `--asm`, and
`--skip-previews` are available for fixture testing. Pillow is required for
median-cut palettes, nearest-neighbor sampling, and contact sheets.

The converter strictly checks the WWA1 magic, record count, dimensions,
anchors, pixel lengths, and exact end-of-file. The current input SHA-256 is
recorded in `rom/generated/roster_manifest.json`.

## WWA1 input records

The file begins with `WWA1` and a little-endian 16-bit frame count. Each record
then contains seven little-endian 16-bit values followed by pixels:

| Field | Meaning |
| --- | --- |
| `character` | 0 through 11, in the roster order below |
| `pose` | 0 through 52, matching `FC_POSE_*` in `src/assets.h` |
| `ticks` | Source display duration in 60 Hz ticks |
| `w`, `h` | Source pixel dimensions |
| `ax`, `ay` | Horizontal axis and feet/effect origin |
| `pixels` | `w*h` little-endian `0xAARRGGBB` values |

Alpha below 128 becomes transparent index zero. Roster order is Ryu, E. Honda,
Chun-Li, Guile, Ken, Blanka, Zangief, Dhalsim, Balrog, Vega, Sagat, M. Bison.
The importer stores global UI and Alpha records under character zero; the
assembly keeps that addressing and also gives those descriptors `WW_UI_*` and
`WW_Akuma_*` labels.

## Conversion formats

Every output canvas uses row-major 8x8 SNES 4bpp characters, 32 bytes per tile.
Bitplanes 0/1 are interleaved for the first 16 bytes and planes 2/3 for the
last 16 bytes of each character. Transparent index zero is never used for a
visible black pixel.

| Format ID | Poses | Canvas and sampling | OBJ/tile budget |
| --- | --- | --- | --- |
| 0, action | Fighter poses 0-13, 20-24, 37-39, 43, 52; Akuma action poses | **48x48**, 1,152 bytes; nearest half-size, reduced further only when needed | **36 8x8 OBJs**, exactly 6x6 tiles |
| 1, projectile | 14, 33, 49 | **32x32**, 512 bytes; nearest half-size or less | **16 8x8 OBJs**, exactly 4x4 tiles |
| 2, portrait | 15, 25, 40 | Selection/continue portraits at half size; 20x26 HUD portraits at native size; pad to 8-pixel boundaries | Variable; intended for dedicated slots/BG use |
| 3, UI | 16-19, 26-28, 34, 41, 42 | Native size, padded right/down to 8-pixel boundaries; no font resampling | Variable; includes map, three fonts, KO and title art |
| 4, effect | 29, 36 | Half size: 128x128 super charge and 160x96 Psycho Crusher canvases | Large effects; not a single 36-OBJ pose |
| 5, ending | 51 | Native **256x144**, 18,432 bytes and 576 tiles per available illustration | BG-oriented, one dedicated palette per ending |

Action and projectile scale is uniform across each animation to prevent size
pumping. The preferred action origin is `(24,48)`. Before reducing scale, the
converter shifts the canvas origin to accommodate extended limbs and unusual
source anchors. The scaled origin is stored per frame, so subtracting it during
rendering preserves the source's world alignment. Feet remain at Y=48 whenever
the source permits it. Only a scaled width greater than 48 or height greater
than 48 causes additional reduction. No source rectangle is cropped.

All action frames, including transparent padding, occupy exactly 36 tiles. The
provided right/left OAM templates map those tiles in row-major order and mirror
both tile positions and tile selection for left-facing rendering. A fighter and
one projectile therefore consume 52 small-OBJ entries before any HUD or other
engine sprites. Scanline limits and the remaining OAM allocation are renderer
concerns.

## Generated binary streams

Current output from the completed 1,746-record import:

| File | Contents | Bytes |
| --- | --- | ---: |
| `roster_action.chr` | 1,502 fixed 48x48 action frames | 1,730,304 |
| `roster_projectiles.chr` | 46 fixed 32x32 projectile frames | 23,552 |
| `roster_portraits.chr` | 36 selection, continue and HUD portraits | 53,120 |
| `roster_ui.chr` | 115 map/font/KO/title/UI frames | 30,816 |
| `roster_effects.chr` | 36 charge frames and 4 Psycho frames | 325,632 |
| `roster_endings.chr` | 7 available 256x144 ending illustrations | 129,024 |
| `roster_frames.meta` | 1,746 fixed 16-byte metadata records | 27,936 |
| `roster_palettes.bin` | 28 palettes, 32 bytes each | 896 |
| **Logical binary payload** | | **2,321,280** |

`roster_manifest.json` contains source hashes, every animation's frame offsets,
palette colors, binary hashes, and the complete bank-chunk map. Contact sheets
are under `rom/generated/previews/`: one action sheet per fighter, plus Alpha,
projectiles, portraits, effects, endings, map, fonts, and UI marks. These show
the actual quantized SNES colors and transparent output canvases.

## Palette format

Each palette is 16 little-endian SNES BGR555 words. Entry zero is transparent;
entries 1-15 are the visible colors. The 12 fighter palettes are shared by that
fighter's action art, projectiles, and portraits. Alpha has a dedicated palette.
The map, combined font set, KO, continue, title, subtitle, round mark, and Bison
hat have dedicated UI palettes. Each ending has its own 15-color palette. This
is 28 palettes in total.

Palette selection is deterministic median-cut sampling with balanced samples
from every source frame. Colors are quantized to 5 bits per channel before the
final indexed image and previews are produced, avoiding a preview/hardware
color mismatch.

## Assembly integration labels

The most useful entry points in `roster_assets.asm` are:

| Label | Purpose |
| --- | --- |
| `WW_RosterPoseTablePointers` | 12 long pointers, one per character, to 53-entry pose tables |
| `WW_Ryu_PoseTable` ... `WW_Bison_PoseTable` | 53 long animation-descriptor pointers; absent animations are `dl 0` |
| `WW_RosterPalettePointers` | 12 long pointers to the per-fighter palettes |
| `WW_RosterEndingPalettePointers` | 12 long palette pointers; unavailable endings use zero |
| `WW_Ryu_Idle`, `WW_Dhalsim_Throw`, etc. | Individual animation descriptors |
| `WW_UI_Map`, `WW_UI_Font`, `WW_UI_KOMark`, etc. | Global UI descriptors |
| `WW_Akuma_SuperCharge`, `WW_Akuma_Psycho`, etc. | Alpha/Akuma descriptors |
| `WW_Ending_Honda_Ending`, etc. | Available ending descriptors |
| `WW_RosterFrameMetadata` | Base of the fixed 16-byte records |
| `WW_RosterPalettes` | Base of the packed BGR555 palettes |
| `WW_RosterActionGraphics` and other `WW_Roster*Graphics` labels | First bank chunk of each format |
| `WW_RosterActionOamRight`, `WW_RosterActionOamLeft` | 36 four-byte `x,y,tile,attribute` tuples |
| `WW_RosterProjectileOamRight`, `WW_RosterProjectileOamLeft` | 16 four-byte OAM tuples |
| `WW_RosterGraphicsEnd` | End of the allocated asset area |

Constants define the format IDs, 13-byte descriptors, 16-byte metadata, 1,152
byte/36-tile action frames, and 512 byte/16-tile projectile frames. All frame,
pose-table, descriptor, metadata, and palette pointers are **24-bit `dl`
values**. Zero is the only missing-animation sentinel.

### Animation descriptor: 13 bytes

| Offset | Type | Meaning |
| ---: | --- | --- |
| `$00` | `db` | Frame count |
| `$01` | `db` | Format ID from the table above |
| `$02` | `dw` | Sum of all frame durations in source ticks |
| `$04` | `dl` | Frame graphics pointer table; each entry is another `dl` |
| `$07` | `dl` | First contiguous 16-byte metadata record |
| `$0A` | `dl` | Animation palette |

The source font glyphs are interleaved between pose IDs in WWA1. Conversion
therefore groups metadata by animation rather than retaining file order. The
original WWA1 index is stored in each metadata record. Font pose 17 uses record
order A-Z then 0-9, as supplied by the importer.

### Frame metadata: 16 bytes

| Offset | Type | Meaning |
| ---: | --- | --- |
| `$00` | `db` | Frame duration in ticks; current source maximum is 120 |
| `$01` | `db` | Format ID |
| `$02` | `db` | Canvas width in 8x8 tiles |
| `$03` | `db` | Canvas height in 8x8 tiles |
| `$04` | `db` | Scaled horizontal origin in canvas pixels |
| `$05` | `db` | Scaled feet/effect origin in canvas pixels |
| `$06` | `db` | Scale numerator |
| `$07` | `db` | Scale denominator; `1/2` means exact nearest half-size |
| `$08` | `dw` | Original WWA1 record index |
| `$0A` | `dw` | Original source width |
| `$0C` | `dw` | Original source height |
| `$0E` | `dw` | Flags: bit 0 = hold final frame after duration, bit 1 = opaque |

Graphics byte count is `width_tiles * height_tiles * 32`. Flag bit 0 matches the
one-shot poses in `fc_sprite()`; looping behavior is not baked into the art.

## LoROM bank layout and size

`roster_assets.asm` starts its tables at **`org $128000`**, leaving bank `$10`
code and the SPC image at `$118000` untouched. Graphics pointers use direct-page
scratch `$F0-$F2`; palette pointers use `$F7-$F9`, not the controller mirrors.
Metadata and palettes share an aligned bank. Large streams are included with Asar `:start..end` ranges in
**75 bank-local chunks**, starting at `$xx8000`. Chunk boundaries occur only
between whole frames, so no graphics DMA or metadata record straddles a bank.
The `*_000`, `*_001`, ... chunk labels are used directly by the frame pointer
tables; callers do not need to calculate global offsets.

The generated table/OAM assembly occupies 12,030 bytes. Bank alignment and the
reserved bank `$10` area account for 208,258 additional bytes in the assembled
image. The independently assembled result is:

- ROM size: **3,065,856 bytes** including the 512 KiB base image
- 4 MiB LoROM limit: **4,194,304 bytes**
- Remaining space: **1,128,448 bytes**
- Final asset address: **`$5DC800`**

Native ending pages are uploaded one 32-tile row per NMI to VMADD `$4000`, with
BG1's tilemap at `$7800`. Their source pointers are generated as canonical LoROM
addresses and carry across bank boundaries.

The converter rejects an output layout over 4 MiB. `roster_manifest.json`
contains each chunk's SNES address and source-file range.

## Current limits

- Seven ending illustrations exist in WWA1. Ryu, Chun-Li, Guile, Ken, and Vega
  use `art_mode: "sprite_scene"` in `config/endings.json` and have no ending
  image records to convert. Their ending descriptor and palette pointers remain
  zero; no substitute artwork is fabricated.
- Fifteen visible colors per palette require visible quantization loss,
  especially in portraits and ending illustrations. The previews show the exact
  result rather than the original RGB images.
- Large effects, the map, portraits, and endings need BG rendering, multiple OBJ
  batches, or dedicated streaming. They are not claimed to fit the 36-OBJ action
  budget.
- This track supplies data and integration symbols only. `main.asm`,
  `graphics.asm`, and the build files are intentionally unchanged; the existing
  `make rom` path still builds the original Ryu vertical slice until a renderer
  consumes these descriptors.
