# Gameplay media

The [teaser](../../media/teaser.mp4), screenshots and social preview use the actual
256 × 224 game framebuffer. Gameplay is enlarged with nearest-neighbor scaling.
The teaser is about 23 seconds: 1920 × 1080, 60 fps, H.264 with AAC game audio. It does not autoplay
on the site; playing it suspends the game until the player chooses Resume.

## Scenes

| Time | Scene |
| --- | --- |
| 0:00 | Hadouken cold open |
| 0:01 | Character selection |
| 0:03 | Chun-Li's Kikoken and Lightning Legs |
| 0:05 | Level-three Shinku Hadouken |
| 0:07 | Psycho Crusher through Yoshi's Island 2 and the goal |
| 0:13 | A glimpse of Akuma's takeover |
| 0:16 | Fireballs in the final fight |
| 0:19 | Zangief piledrives Akuma and the car |
| 0:21 | Original crossover logo and PRESS START |

Capture fixtures choose the fighter, starting meter and nearby enemies before
recording. Combat, collisions, original level scripts, the boss AI and the goal
sequence then run in the real engine. Audio combines the original SPC stream
and fighter voices/effects at the same gains as the game. There is no added music.
Gameplay fills the video height with black sides, preserving the whole playfield.
There are no feature captions or interstitial text cards. The only card is the
two-second ending, using logo art extracted from the original title framebuffer.
Character endings are left for the player to discover.

## Rebuild

Run from the repository root after preparing the runtime assets:

```sh
make smw
python3 dev/media/capture.py
build/asset-tools/bin/python3 dev/media/edit.py
```

Requires SDL2, ffmpeg and Pillow. `WW_SCENE=shinku` captures only that scene.
No external fonts are needed. Original game content has the same
[credits and provenance](../ASSETS.md) as the game itself.

Raw captures, lossless masters and the edit list stay in ignored `build/media/`.
Only the finished teaser, optimized images and application icons are committed
under `media/`. `dev/site_files.json` is the explicit deployment allowlist.
