# Development

Player information is in the [main README](../README.md) and [player guide](../docs/PLAYING.md).

- [Build from source](BUILDING.md): native and WebAssembly toolchains, asset preparation and engine layout.
- [Tests](TESTING.md): combat, original-game replays, audio, animations and phone browser checks.
- [Release and Pages publishing](PUBLISHING.md): reproducible packages, release assets and deployment.
- [Asset provenance and credits](ASSETS.md): original game art/audio, community rips and new ending illustrations.
- [Engine research](RESEARCH.md): pinned upstream projects and integration notes.
- [Coverage and limitations](LIMITATIONS.md): what is and is not verified.
- [Gameplay media](media/README.md): teaser scene guide, capture and editing tools.

Run commands from the repository root. Generated builds stay in ignored `build/`,
prepared game data in `local/`, and distributable packages in `dist/`.
Native replays and WASM builds share a generated engine tree; run them serially.
