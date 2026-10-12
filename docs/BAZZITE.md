# Play on Bazzite or a TV

Download the [Linux desktop package](https://github.com/mikolalysenko/smw-street-fighter/releases/latest/download/super-mario-world-ii-linux-x86_64.tar.gz)
and extract it in Desktop Mode. Keep the whole folder together: it includes the
executable, original game data, SDL and support libraries.

1. Open the extracted folder and run `./play.sh --fullscreen`.
2. In Steam, choose **Add a Non-Steam Game** and select `play.sh` from that folder.
3. Set **Launch Options** to `--fullscreen` and use Steam’s **Gamepad** controller layout.
4. Launch it from Gaming Mode. No Proton compatibility setting is needed for the native Linux build.

If the file manager does not run scripts, open a terminal in the folder and use
`./play.sh --fullscreen`. F11 toggles fullscreen from a keyboard. Start pauses;
Start + Back on the world map quits back to Steam.

For a quick alternative, open the [browser game](https://mikolalysenko.github.io/smw-street-fighter/),
click Play and Fullscreen. A standard gamepad works there too. Browser and native
saves are separate; see the [player guide](PLAYING.md#saves).

Use the TV’s Game Mode and disable overscan so the edges of the HUD stay visible.
Check your selected audio output and controller bindings in Desktop Mode before
switching to Gaming Mode. The downstairs TV and physical Bazzite setup have not
been tested; the Linux package is built and boot-tested on a Linux runner.

[Controls and moves](../README.md#controls) · [Building on Linux](../dev/BUILDING.md)
