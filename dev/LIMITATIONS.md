# Test coverage and known limits

## Current limits

The main SMW integration was built and visually checked on Apple Silicon.
An automated replay boots the original game, reaches Yoshi's Island 2, and
checks an attack, super, Goomba contact damage and KO using deterministic enemy
fixtures. This is not a full-game completion test.

General foreground occlusion and dedicated swimming/climbing/riding attack poses
remain. Pipe rims and castle/ghost-house doorways clip the replacement sprite.
Yoshi's original rendering and mounting work; the fighter keeps Mario's
compact terrain collision box. Animation
timings and anchors are adapted for platforming; jumps use separate airborne
strike clips and walking advances with actual movement. Projectiles pass
through background scenery; supers break destructible blocks, while essential
objects survive explosions to preserve routes.
The original title/overworld still show Mario. Snapshot/replay hotkeys are
disabled because they omit combat state.

Real-engine replays verify Hadoukens, supers, KO and native endings in all seven
Koopaling rooms, Reznor, Big Boo and Bowser. Movement tests cover ten jump/attack
combinations, all Continue choices and goal-tape clear. Presentation tests cover
platform redraws, Yoshi standing/riding, message boxes, and character selection
before both new and saved games. A Node test boots the shipped WASM module and
checks original menus/music, movement, crouching attacks, air specials, supers,
audio and save RAM. These checks are not a full-game completion test.

Linux CI runs core, sanitizer and SMW bridge checks. Native release jobs test
animation/audio data and relocated desktop packages; package smoke tests use
headless SDL drivers. Browser checks exercise WebKit and Chromium with emulated
phones, tablets and desktops. The physical iPhone, Bazzite machine, controller
and downstairs TV have not been tested.

