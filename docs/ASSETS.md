# Original asset provenance

The runtime now uses original game data and ripped SF2 art/audio. The new C
combat code and UI layout are separate from the original game content. No game
binaries or extracted sprites/sounds are tracked by this repository.

## Super Mario World

Source: [IsoFrieze/SMWDisX](https://github.com/IsoFrieze/SMWDisX), revision
`30643c7595a7d097d731de69476f8059c7cf98c3`.

Assembler: [Asar 1.91](https://github.com/RPGHacker/asar), revision
`6a6dbdaed8cb773e583052ce4f66375bd896c3a1`.

`scripts/bootstrap_originals.py` assembles the USA version with `-D_VER=1` and
requires SHA-1 `6b47bb75d16514b6a476aa0c73a683a2a4c18765`. The locally rebuilt
512 KiB ROM matches that original USA image exactly. The pinned SMW port's
asset extractor then creates `local/smw_assets.dat` without embedding the ROM.
The native engine reads the original level layouts, tiles, palettes, sprites,
music and sound data. Levels are not approximated or redrawn.
The extractor explicitly includes the original four secondary-entrance tables
at $05F800–$05FFFF; the pinned upstream extractor omitted them for vanilla ROMs.
Return pipes require these tables even without Lunar Magic.

## Street Fighter II

Fighter animations: [N64Mario's original SFII World Warrior sprite rip](http://gcnmario.free.fr/mugen/sf2ww.html).
`config/sf2-motion-source.json` pins the sprite-only SFF archive by SHA-256.
`scripts/sf2_motion_assets.py` reads its indexed images, shared palettes and
signed axes as data. Vega's separate claw layer is composited at its original
axis. Grounded poses align their lowest visible pixels with SMW's platform
collision plane. Airborne frames retain their relative origins, including spins.
`config/sf2-sprites.json` records the selected frame identifiers and durations.
All twelve fighters use these original walking, jumping, crouching, normal
attack and special-move frames. Forward and neutral jumps have separate clips.
Normal attacks reach their strike frames when combat damage becomes active;
specials have startup, active and recovery sections. Boss fighters' limited
World Warrior moves are adapted to the common controls: Balrog uses punches on
both attack buttons, Sagat uses his tucked aerial elbow pose for an air jab,
and Vega uses his original aerial claw slash. Throws, air specials and supers
remain platformer adaptations, not a frame-exact recreation of arcade combat.

UI, audio and supplementary sheets: [Rengrow / Street Fighter II: Gamusinos Fighters](https://github.com/Rengrow/Street-Fighter-II-Gamusinos-Fighters-),
revision `f6ed596a92d96f7f6dbcad9d92967d1e2dec2e1f`.

The asset tree preserves ripped SF2 sprites, portraits, selection map and WAV
effects/voices. `config/community-assets.json` pins the source revision and
SHA-256 of each downloaded file. `config/sf2-sprites.json` also records selected
rectangles from these supplementary sheets.

`scripts/import_community_sf2.py` packs the selected frame records (including repeated
frames, crouching attacks, portraits, original fonts, KO lettering, countdown
lamps, victory poses, map, original title II/subtitle, small HUD portraits,
system font, round markers and Alpha effects), 39 effects/voices and the selection/continue music.
It does not generate replacement
art. All twelve SF2 fighters render at half arcade size to fit Mario's
levels. Cropping, nearest-neighbor scaling and animation timings are adaptations.
Chun-Li’s added Kikoken uses her two-palm strike frames with her own blue/yellow
energy projectile, isolated from the pinned `chunny-fireball1212.gif` rip at
[Fighters Generation](https://www.fightersgeneration.com/characters/chunny-fireball1212.gif).
Grounded poses are aligned to their lowest
visible pixels at the game’s half-size rendering scale.
The supplementary sheets are SF2-family community rips; variants and palette
differences are possible. No claim is made to a verified `sf2ua` ROM extraction.
The HUD's coins and bonus stars are decoded from SMW's
original common graphics and palettes into private buffers. They remain correct
when boss rooms replace the live VRAM with Mode 7 art.
`config/animation-assets.json` pins supplementary GIFs by URL and SHA-256.
The four original World Warrior defeated portraits come from
[Street Fighter Galleries](https://sfgalleries.net/art/sf2/sf2ww/series2/), also
pinned in that manifest. Their green color key is made transparent at import.

Ripped audio includes Hadouken, Shoryuken, Tatsumaki, Honda's voice, Sonic Boom,
Chun-Li's Spinning Bird Kick call and scream, general impacts, block, menu and KO effects. The
main game loads `sf2/audio/`; it does not use the dojo's synthesized fallback.
The boss KO announcement is the pinned Street Fighter sample in
[Citedy/game-sounds](https://github.com/Citedy/game-sounds), commit
`cc18cd9734f8ec3b2b853a294e56698d746ffadb`; its URL and SHA-256 are in
`config/ko-asset.json`. This clip is a later Street Fighter voice, not claimed
as an SF2 World Warrior announcer extraction. The SF2 `player_select.ogg` and
`continue.ogg` from the pinned Rengrow asset tree are converted to PCM at import
time. Selection loops; Continue plays once against its countdown. SMW's music
is muted during both menus, then continues through the original
sound engine. Native and browser builds use the same imported audio.

Chun-Li's spoken Kikoken, Lightning Legs attack cry and kick swish come from
[Jmorphman's Chun-Li sound archive](https://network.mugenguild.com/jmorphman/characters/chun-li),
which contains original Capcom audio. Its move definitions identify SND 1000,0
as Kikoken, 4,2 as the rapid-kick cry, and 1,3 as the swish. These are supplementary
CvS-era samples, not World Warrior recordings. `config/chunli-audio.json` pins
the archive and each extracted WAV; `scripts/chunli_audio.py` reads only the
SND data, without executing the character scripts. Extraction needs bsdtar.

Supers and their gauge are new Alpha-inspired mechanics using SF2 animation
frames and voices, with the original Alpha activation effect and audio described below.

## Street Fighter Alpha

Source: [gamedev8/js-sfa](https://github.com/gamedev8/js-sfa), revision
`66cf60d2198ef613a84229d4e9d0f07c2347d215`. `config/alpha-assets.json` pins each
source file's SHA-256. `scripts/alpha_assets.py` reads atlas rectangles and
base64 audio as data; it does not execute downloaded JavaScript.

Imported material includes the 36-frame super-charge effect and sound, Akuma's
stance, fireball, warp and back-turned victory artwork/voices, and Bison's Psycho
Crusher spin and voice. The source atlases are enlarged 2×; nearest-neighbor
sampling restores the native pixels. The clown-car pilot uses upper-body crops.
Bowser's falling body and car still come from the original SMW Mode 7 graphics;
his hat is cropped from the pinned original SF2 Bison animation.

Ryu’s Shinku charge and Hadouken release voices are imported from the pinned
Alpha `script/audio/ryu.ogg.js` asset; the level-three beam is a procedural
pixel effect around the original energy sprite.

Bison's level-three wake and continuous flight take inspiration from
[N64Mario's Psycho Crusher, FTW!](https://www.youtube.com/watch?v=oIXilgKNYaY).
No video frames or MUGEN code are included. The new beam geometry, timing,
combat rules and Akuma encounter are platformer adaptations.

The crossover title reads SMW's original BG3 lettering and palettes directly
from the running engine and repositions it with nearest-neighbor scaling. The
SF2 II and “The World Warrior” art are transparent crops of the pinned
`logo2.png`; disconnected fragments of the neighboring words are discarded.
The start prompt and high score use the original SF2 font sheet.

## Paths and notices

- `local/smw-rebuilt.sfc`: rebuilt original data, ignored.
- `local/smw_assets.dat`: extracted SMW runtime data, ignored.
- `local/community-sf2/`: downloaded source rips, ignored.
- `local/sf2-motion/`: original arcade fighter sprite archive, ignored.
- `local/community-sfa/`: pinned Alpha sprite/audio data, ignored.
- `local/sf2/`: packed fighter sprites and WAVs, ignored.
- `build/smw/`, `dist/`: local runtime copies, ignored.

Nintendo and Capcom retain their rights to the respective games and assets.
The repository's MIT license applies to the newly written code/documentation,
not these assets. Preserve the original source project credits and engine
notices when moving the local build. The SF2 rewrite is reference-only and is
not linked into the executable.
