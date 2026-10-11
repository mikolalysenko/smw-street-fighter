# Downstairs TV / Bazzite

The main game is native C/SDL2 with controller controls and fullscreen launch.
It has been built on this Mac; Linux and the downstairs hardware still need a
first playtest. There is no dependency on a browser or the NAS at runtime.

## Build in a Distrobox

Bazzite supports Distrobox for development packages; see its
[official guide](https://docs.bazzite.gg/Installing_and_Managing_Software/Distrobox/).
Copy this repository (including `.git` and the reference submodules) to the
Bazzite machine, or clone it once a Git remote exists. In Desktop Mode:

```sh
distrobox create --name world-warrior-dev --image docker.io/library/ubuntu:24.04
distrobox enter world-warrior-dev
sudo apt-get update
sudo apt-get install -y build-essential libsdl2-dev python3 python3-venv cmake git
cd /absolute/path/to/smw-street-fighter
git submodule update --init
make original-assets
make smw test test-smw test-play
./scripts/play-smw.sh --fullscreen
```

You can instead copy `local/smw_assets.dat` and `local/sf2/` from this Mac, then
skip `make original-assets`. Those data files work on either platform. The
executable must be compiled for the Linux machine's architecture.

## Launch from Steam Gaming Mode

After confirming fullscreen play in Desktop Mode, add a non-Steam game in Steam
with target `/usr/bin/distrobox` and launch options:

```text
enter --name world-warrior-dev -- /absolute/path/to/smw-street-fighter/scripts/play-smw.sh --fullscreen
```

Quote the script path if it contains spaces. Keep execution in the Distrobox so
it finds the SDL library used for compilation. Use Steam's Gamepad layout and
Steam Input for controllers that require it. Button assignments are in the
[README](../README.md). Start opens Resume, Character Select and Quit Game.
On the overworld, Start + Back quits.

The original 256×224 SMW framebuffer is scaled with nearest-neighbor filtering
and preserved aspect ratio. TV Game Mode and disabled overscan are recommended.
F11 toggles fullscreen from a keyboard; the launcher sets it automatically.

## Couch playtest

Check D-pad/stick diagonals, all four face buttons, shoulders, Start/Back,
controller reconnect, audio output and quitting back to Steam. Try Ryu's
Hadouken, Honda's headbutt, Guile's charge move and a super. Walk into an enemy
for the hurt reaction, then test a pit KO and re-entry. Check input/audio latency
at the TV's actual refresh rate. These hardware checks have not been performed.

`make package` creates `dist/world-warrior-smw-linux/` on Linux, including a
fullscreen `play.sh`, data and source notices. This is currently a native local
bundle that uses system SDL2, not a self-contained AppImage or Flatpak. A portable
release and Steam artwork can follow the first hardware playtest.
