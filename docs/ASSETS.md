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

## Street Fighter II

Source: [Rengrow / Street Fighter II: Gamusinos Fighters](https://github.com/Rengrow/Street-Fighter-II-Gamusinos-Fighters-),
revision `f6ed596a92d96f7f6dbcad9d92967d1e2dec2e1f`.

The asset tree preserves ripped SF2 sprites, portraits, selection map and WAV
effects/voices. `config/community-assets.json` pins the source revision and
SHA-256 of each downloaded file. `config/sf2-sprites.json` records each selected
frame rectangle and the prototype's animation duration.

`scripts/import_community_sf2.py` packs 371 frame records (including repeated
frames, crouching attacks, portraits, original fonts, KO lettering, countdown
lamps, victory poses and map), 20 effects and the selection/continue music.
It does not generate replacement
art. Ryu, Honda, Chun-Li and Guile render at half arcade size to fit Mario's
levels. Cropping, nearest-neighbor scaling, foot anchors and animation timings
are adaptations. These sheets are SF2-family community rips, not a verified
frame-for-frame `sf2ua` extraction; variants and palette differences are possible.
Chun-Li's missing idle and walk clips come from the original SF2 animations at
[The Fighters Generation](https://www.fightersgeneration.com/characters/chunli-a2.html).
Her standing punch comes from
[vomira/street-fighter](https://github.com/vomira/street-fighter), commit
`973a13fb1101a12406ac0bb14b531d3010b5c508`.
`config/animation-assets.json` pins these files by URL and SHA-256. GIF frames
retain their common canvas origin so feet do not shift between walking frames.
The four original World Warrior defeated portraits come from
[Street Fighter Galleries](https://sfgalleries.net/art/sf2/sf2ww/series2/), also
pinned in that manifest. Their green color key is made transparent at import.

Ripped audio includes Hadouken, Shoryuken, Tatsumaki, Honda's voice, Sonic Boom,
Chun-Li's voice and scream, general impacts, block, menu and KO effects. The
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

Supers and their gauge are new Alpha-inspired mechanics using SF2 animation
frames and voices. No original Alpha super animation/audio extraction is claimed.

## Paths and notices

- `local/smw-rebuilt.sfc`: rebuilt original data, ignored.
- `local/smw_assets.dat`: extracted SMW runtime data, ignored.
- `local/community-sf2/`: downloaded source rips, ignored.
- `local/sf2/`: packed fighter sprites and WAVs, ignored.
- `build/smw/`, `dist/`: local runtime copies, ignored.

Nintendo and Capcom retain their rights to the respective games and assets.
The repository's MIT license applies to the newly written code/documentation,
not these assets. Preserve the original source project credits and engine
notices when moving the local build. The SF2 rewrite is reference-only and is
not linked into the executable.
