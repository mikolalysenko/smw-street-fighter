# Gameplay media

The [teaser](../../media/teaser.mp4), screenshots and social preview use the actual
256 × 224 game framebuffer. Gameplay is enlarged with nearest-neighbor scaling.
The teaser is 1920 × 1080, 60 fps, H.264 with AAC game audio. It does not autoplay
on the site; playing it suspends the game until the player chooses Resume.

## Scenes

| Time | Scene |
| --- | --- |
| 0:00 | Original title art with the crossover title |
| 0:03 | Twelve-fighter character selection |
| 0:07 | Ryu bonks a shell, then fires Hadouken |
| 0:11 | Level-three Shinku Hadouken |
| 0:15 | Chun-Li's Kikoken and Lightning Legs |
| 0:20 | Psycho Crusher through Yoshi's Island 2 and the goal |
| 0:32 | Akuma takes Bowser's clown car |
| 0:40 | Fireballs and counters in the final fight |
| 0:45 | Zangief piledrives Akuma and the car |
| 0:49 | All twelve ending screens and play link |

Capture fixtures choose the fighter, starting meter and nearby enemies before
recording. Combat, collisions, original level scripts, the boss AI and the goal
sequence then run in the real engine. Audio combines the original SPC stream
and fighter voices/effects at the same gains as the game. There is no added music.
The end card is a montage of the twelve ending screens captured by `test-features`.

## Rebuild

Run from the repository root after preparing the runtime assets:

```sh
make smw test-features
python3 dev/media/capture.py
build/asset-tools/bin/python3 dev/media/edit.py
```

Requires SDL2, ffmpeg and Pillow. `WW_SCENE=shinku` captures only that scene.
The editor uses macOS Arial fonts by default; set `WW_MEDIA_FONTS` to a directory
containing Arial.ttf and Arial Bold.ttf on another machine. Fonts are rasterized
into the layout, not redistributed. Original game content has the same
[credits and provenance](../ASSETS.md) as the game itself.

Raw captures, lossless masters and the edit list stay in ignored `build/media/`.
Only the finished teaser, optimized images and application icons are committed
under `media/`. `dev/site_files.json` is the explicit deployment allowlist.
