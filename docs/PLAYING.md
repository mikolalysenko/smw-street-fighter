# Player guide

Start a one-player game using Enter or controller Start. Pick your fighter at
character select after choosing a save file, before the intro or overworld loads.
This also applies when resuming an existing save. Use the normal SMW overworld to
enter a level. Each life starts
with one super bar. Hits refill it; supers spend one to three complete bars.
Health refills on level entry and meter carries between levels. Start opens
Resume / Quit Game during play. On death, the SF2 continue
screen shows the defeated fighter and a ten-second countdown. Start/Jump opens
character select, then returns to the world map with the chosen fighter at full
health and one super bar. Enter a level when ready. Back or letting the countdown
expire returns directly to the map. Continues are unlimited.
Character selection plays the original SF2 selection music. The title combines
SMW's original lettering, scenery, demo and music with SF2's gold/red II and
blue “The World Warrior” subtitle. Course clear shows the selected fighter.
Clearing without taking damage displays PERFECT with the original announcer voice.
Defeated Akuma flips upside down, pops upward, then falls offscreen before the
original Peach rescue. Each fighter gets a distinct ending screen after the
rescue; Start/Jump continues into the original credits. Seven endings have new
pixel-art illustrations; Ryu, Chun-Li, Guile, Ken and Vega currently use original
arcade sprites in custom pixel scenery.

**Debug shortcuts:** open the browser demo with `?debug=1`, or launch the native
game with `./scripts/play-smw.sh --debug` (combine with `--fullscreen` for TV play).
Only debug mode adds **PLAYER SELECT** and **AKUMA BATTLE** to the pause menu;
normal starting/continuing character selection always remains available.

**Jump to the final fight in debug mode:** enter any level, press Enter/Start, choose
**AKUMA BATTLE**, then confirm. You start with full health and three super bars.
Bowser makes his entrance wearing Bison's hat before Akuma takes over the clown
car. Continue opens character select and returns to the world map here too.
Practice does not write game saves or records; returning to the map restores
the score and lives from before practice.

The title and gameplay HUD show your high score. SMW's normal points from
enemies, pickups and the course-clear tally count; combat kills award the native
combo points too. Consecutive combat hits award **200, 400, 800, 1000, 2000,
4000, 8000, 1UP, 2UP, then 3UP**. Further hits keep awarding 3UP, up to SMW's
99-life limit. Damage or a 90-frame gap ends the chain; Bison's continuous level
3 flight keeps it going. New records save immediately, survive new games and continues,
and travel with Export Save in the same `smw.srm` as level progress. The title
demo cannot set a record. Desktop builds keep it in your user account; see [Saves](#saves) below.

The transparent SF2 HUD keeps Mario's counters: **1P** is the current score,
**HI** is the saved record, and the three-digit clock beneath **K.O.** is SMW's
actual timer. The fighter portrait has lives underneath the name; the five
round markers track Dragon Coins. The right bar fills toward a 100-coin **1UP**,
then becomes the opponent's health bar in a boss room. Coins and bonus stars
remain visible during boss fights. Bowser/Akuma
has an unlimited clock, displayed as infinity. The bottom gauge shows super
levels, and hit combos show their current points or extra-life award.

## Controls

Punches and kicks bonk enemies, flip shells and activate item/turn blocks next to you.
Cement and used blocks stay solid against normal attacks. Shinku Hadouken shatters
turn, used and cement blocks in its path, while item blocks release their contents.
Aerial Down + Punch/Kick bumps blocks below you. Crouching attacks hit low blocks; Dhalsim's extended limbs reach
farther. Pipes, doors and level terrain remain usable. Down + Guard holds a low
block; a fresh guard can parry, while sustained guard takes reduced damage.
Bison's level-three Psycho Crusher damages enemies touching its visible beam.
At the goal he leaves the beam and jumps down continuously to the finish floor.

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
| Crouch block | Down + LB | Down + D |
| Pause / confirm | Start | Enter |
| Back / pause | Back | Escape |
| Fullscreen | Launcher option | F11 |

SMW's introduction and message boxes accept Jump. On the overworld,
Start + Back together quits. Controller labels use SDL's Xbox button positions;
Steam Input can map other controllers to this layout.

Tap **Guard** just before a frontal hit for a parry: no damage and a quarter-bar
of super meter. Holding Guard gives normal blocking with chip damage. A parry
keeps a PERFECT run intact. Grounded landings use a brief crouch that attacks
and movement can interrupt immediately; walking off a ledge uses a falling
pose. A small gold glint at the feet signals that the charged high jump is ready.

## Fighters and moves

| Fighter | Special | Secondary | Super |
| --- | --- | --- | --- |
| Ryu | Hadouken | Shoryuken | Shinku Hadouken |
| E. Honda | Sumo Headbutt | Hundred Hand Slap | Super Oni Musou |
| Chun-Li | Kikoken | Spinning Bird Kick | Senretsukyaku |
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
repeated kicks trigger rapid attacks. Keep tapping to sustain them indefinitely;
Blanka also accepts repeated Down + Special for electricity. Chun-Li uses her
own Kikoken projectile and voice, and Lightning Legs has a repeating kick swish.
Honda’s headbutt
flies horizontally, and Bison’s crouching kick slides forward. Dhalsim uses his
long punches, including the crouching stretch, and his original low slide kick.
Landed normals can cancel into specials; landed attacks can
cancel into supers. These are forgiving platformer adaptations, not arcade
frame data. Sonic Hurricane is a later-series move adapted to this prototype.

Bison's **three-bar Psycho Crusher** is intentionally ridiculous: invulnerable
flight, 999-damage hits, an original Alpha spin and blue-white wake, and a
combo that lasts until he lands. It flies to the actual goal or end of the room.
Up/Down steer in horizontal rooms; Left/Right steer in vertical rooms. **Guard
lands early**. Doors, pipes, exits and essential platforms keep working; a room
without a goal still needs its normal exit. The camera and sprite loader continue
running, so the native goal, score tally and overworld event finish normally.
Forced scrolling accelerates during the flight and returns to normal on landing.
Boss rooms retain their arena boundaries and damage rules.

Mobility specials share **one use per jump**. Land, swim, or grab a climbable
surface to reset it; attacking or hitting a wall does not grant another use.
Ordinary punches, kicks and fireballs remain usable in the air.

**High jump:** hold Down for about three quarters of a second, then Jump.
Keep Jump held for height and steer in the air. Every fighter can reach Donut
Plains 1's secret platform without the green switch blocks.
Walk into a key or shell to pick it up, or hold Guard while touching it.
It stays held when you stop and attack. **Guard + Punch** releases it; add
Down to drop or Up to throw upward. Keys still activate the original keyholes.

Zangief's **Up + Special** grabs nearby enemies and pipes. A successful grab
launches, spins the held object upside down, and damages it only on landing.
A whiff only reaches. **Super** can also grab Akuma with the entire clown car.
Required objects and pipe entrances remain usable after their slam.
Triangle blocks rotate the fighter into the wall run, with a walking cycle.

Enemy contact causes health loss, hitstop, hit stun, knockback and SF2 hurt
sounds. Death uses ripped fighter screams. Punches, kicks, specials and supers
also work in the air, including pressing jump and attack together.
Holding Down gives each fighter original crouching punch/kick poses and low
hitboxes. Moving platforms preserve fighter visibility, and the original Yoshi
renders both on foot and while ridden. Health and super gauges float over the
level, without an opaque HUD strip.
Mounted Hadoukens pass Yoshi. Punch activates his tongue; automatic running does
not. Mushrooms add one super bar, Fire Flowers add two, and Feathers fill all
three. Pickups are consumed immediately; reserve items are disabled. They do
not heal or change the fighter.

Ryu and Ken’s fireballs originate at their palms, also in the air and on Yoshi.
Shinku Hadouken uses the original Alpha “Shinku… Hadouken” voice sequence.
Level one and two fireballs are larger; level three becomes a sustained beam
that blasts across the stage. Small enemies still use forgiving hit detection.

Specials can hit all normal sprite IDs and extended/cluster hazards, including
Chucks, Thwomps, wooden castle plungers and the layer-three crusher. Enemies
explode when hit by specials; normal punches and kicks use Mario-style hit reactions. Keys, exits, switches and essential platforms take a visual hit but
retain their function. Terrain shows a single impact without erasing routes.
Boss rooms have opposing health bars, a KO announcement and their original
defeat/exit sequences. In the finale, Akuma assassinates Bowser, throws him
upside down offscreen, and pilots the original clown car. The fight has three
health phases: purple air fireballs and moving volleys, then roof-level car
sweeps and telegraphed ground slams, then charged red barrages and warp feints.
Uppercuts mix into the earlier phases. Each wave ends with a clear recovery
opening; leftover hostile projectiles disperse, and health phase changes give
Peach a quiet mushroom throw.

Punch or kick an incoming fireball to send it back. A timed guard returns it
too; a successful return staggers Akuma and earns extra meter. Jump down onto
the clown car's rim to bop it and bounce into an aerial follow-up. Zangief can
still piledrive the whole car. Damage is capped at six per hit and twelve per
wave, with health thresholds at two thirds and one third; Akuma visibly guards
during recovery when the wave's damage limit is reached. Defeating Akuma starts
the original rescue and ending.


## Saves

The file `smw.srm` contains normal SMW progress and the crossover high score.
These are save-point saves, not instant snapshots of the room you are in.

| Desktop system | Location |
| --- | --- |
| macOS | `~/Library/Application Support/WorldWarrior/SuperMarioWorldII/smw.srm` |
| Linux | `~/.local/share/WorldWarrior/SuperMarioWorldII/smw.srm` (or your `XDG_DATA_HOME`) |
| Windows | `%APPDATA%\WorldWarrior\SuperMarioWorldII\smw.srm` |

Close the game before replacing a save. Keep a backup of the current file.
To move from an older portable build, copy its `saves/smw.srm` to the location
above. An existing local `saves/smw.srm` is also read automatically when no
user-account save exists. The first new save writes to your user account.

In the browser, use **Export Save** to download `smw.srm`. It can be copied to
one of the native locations above. Browser storage is separate for each site
and device; clearing site data removes its save. Browser save import is not
currently exposed in the player UI.

## Troubleshooting

- **No sound on iPhone:** tap Play or Resume after switching back from another app. Check the device volume.
- **No touch buttons on a tablet:** touch controls are phone-only. Use a gamepad or keyboard on tablets.
- **Missing game data / DLL:** extract the entire release archive and keep its files together.
- **macOS blocks opening:** review the app in System Settings → Privacy & Security. The community release is not notarized.
- **Controller is silent in the browser:** press a button after clicking Play; use a standard mapping or Steam Input.
- **Want to test Akuma directly?** Browser: add `?debug=1` to the URL. Desktop: launch with `--debug`. In a level, pause and select Akuma Battle. Practice does not overwrite progress or records.
