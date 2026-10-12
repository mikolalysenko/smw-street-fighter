# Downstairs TV / Bazzite

The game has native C/SDL2 and WASM browser builds with controller controls.
It has been built on this Mac; Linux and the downstairs hardware still need a
first playtest. Prepared builds do not need the NAS at runtime.

## Browser build

Open the [hosted demo](https://mikolalysenko.github.io/smw-street-fighter/) on the
Bazzite machine, or download `super-mario-world-ii-web.zip` from
[Releases](https://github.com/mikolalysenko/smw-street-fighter/releases) and unzip it.
For an offline local server, use the complete `web/` folder from that download
(or copy `dist/web/` from this Mac).
In that directory, run:

```sh
python3 -m http.server 8765 --bind 127.0.0.1
```

Open `http://localhost:8765` in a browser, click Play, then Fullscreen. The
browser uses WebGPU when available and falls back to Canvas 2D. WASM runs the
same original SMW game code in either case. Serve it over localhost or HTTPS
for browser GPU and controller APIs. Opening `index.html` directly is not
supported. The page has keyboard and Xbox-position gamepad controls.

The same folder works across CPU architectures. Browser save points persist
per browser/site; Export Save downloads the original 2 KiB SMW SRAM file.
The downstairs browser, controller, audio output and TV latency still need
a physical playtest.

## Build in a Distrobox

Bazzite supports Distrobox for development packages; see its
[official guide](https://docs.bazzite.gg/Installing_and_Managing_Software/Distrobox/).
Copy this repository (including `.git` and the reference submodules) to the
Bazzite machine, or clone `https://github.com/mikolalysenko/smw-street-fighter.git`. In Desktop Mode:

```sh
distrobox create --name world-warrior-dev --image docker.io/library/ubuntu:24.04
distrobox enter world-warrior-dev
sudo apt-get update
sudo apt-get install -y build-essential libsdl2-dev python3 python3-venv cmake git ffmpeg libarchive-tools
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
[README](../README.md). Start opens Resume and Quit Game. Add `--debug` to the launch options to include
Character Select and Akuma Battle shortcuts.
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

`make package` creates `dist/super-mario-world-ii-linux/` on Linux, including a
fullscreen `play.sh`, data and source notices. This is currently a native local
bundle that uses system SDL2, not a self-contained AppImage or Flatpak. A portable
release and Steam artwork can follow the first hardware playtest.
