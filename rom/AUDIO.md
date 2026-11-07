# Native SNES audio

This track keeps Super Mario World's SPC700 music engine and adds one streamed
Street Fighter voice/effect channel. It does not replace the music driver and
does not use SMW's blocking gameplay-time sample upload routine.

## Files and build

- `rom/make_audio.py` — standard-library WAV reader, mono downmix, integer
  resampler, BRR encoder/reference decoder, source-hash manifest, native SPC
  block extractor, and unit self-tests.
- `rom/spc700.asm` — native-engine poll hook, port relays, and streaming driver.
- `rom/audio.asm` — SPC upload image, 65c816 transport/API, sound IDs,
  descriptors, and generated BRR includes.
- `rom/AUDIO.md` — this document.

After `scripts/import_community_sf2.py` has produced the pinned WAVs:

```sh
python3 -I rom/make_audio.py
python3 -I rom/make_audio.py --self-test
python3 -I rom/make_audio.py --check       # optional reproducibility check
```

The default input is `local/sf2/audio`; output is
`local/sf2/native-audio`. The converter writes 41 `.brr` files,
`manifest.json`, `audio_data.asm`, `audio_descriptors.asm`, and three stock SPC
blocks extracted from the verified USA SMW ROM. Generated `incbin` ranges split
at LoROM boundaries without adding or dropping stream bytes. `--base` selects
another copy of the same verified ROM; `--no-native` converts audio without
extracting those blocks.

The ROM integration owner should add this after the other expanded-ROM code:

```asm
incsrc "rom/audio.asm"
```

No edit to `world_warrior.asm` is included in this track. `audio.asm` also
assembles standalone against a scratch copy of the verified base ROM:

```sh
asar --error-limit=60 rom/audio.asm /tmp/audio-test.sfc
```

Asar 1.91 is required. Native extraction checks SMW's SHA-1 and all four exact
SPC hook/relay byte signatures before applying patches.

## 65c816 API

Public routines return with `RTL`; call them with `JSL`. They preserve X/Y, the
incoming M/X/I flags, and scratch `$00-$02`. Persistent state is WRAM
`$7F0E00-$7F0E0F`.

```asm
; Main-loop context, not NMI:
LDA.B #!WW_SND_HADOUKEN
JSL WW_AudioRequest             ; A = 0 accepted, 1 invalid, 2 transport error

; Once per gameplay/main-loop frame while the clip is active:
JSL WW_AudioUpdate              ; A = 0 OK/full/idle, 2 transport error

; Optional early cancellation, required before a native music-bank upload:
JSL WW_AudioStop
```

- `WW_AudioRequest` restarts a one-shot stream and offers one initial block.
  The regular update is deferred to the next frame, avoiding a ten-block spike
  when combat requests a sound. Further uploads are scheduled by
  `WW_AudioUpdate`, which keeps input responsive. Long streams key on after 64
  queued blocks (128 ms). A short stream keys on when its source end block
  arrives, provided at least 12 blocks are queued.
- `WW_AudioUpdate` offers at most nine blocks (81 source bytes) per call while
  gameplay is idle, one block while buttons are held, and one block in custom
  menus. A full ring or a busy native engine is normal retry flow, not an error.
- `WW_AudioStop` keys off voice 4 only when the custom stream is active and
  returns native APU0/APU1 handling to SMW. Stop before any native music-bank
  upload; do not start such an upload while fighter audio is active.
- `WW_AudioLastStatus` (`$7F0E09`) retains the latest transport result.
  `$7F0E05` is the remaining upload-block count; zero means all source bytes
  were uploaded, not necessarily that playback has finished.

Do not call these routines from NMI. Each block uses a bounded transaction with
IRQ and NMI masked while automatic joypad reading remains enabled. The code
restores NMI/joypad enable bits on return, releases APU ports, and restores
`$00-$02`. Command waits use bounded outer/inner timeout windows; byte waits
have an 8-bit timeout. A begin/byte protocol failure returns 2 rather than
waiting forever. A block-ready timeout returns 1 so the next update retries.

### Sound IDs

IDs are an append-only ABI and match `manifest.json` exactly.

| ID | Define | Name | ID | Define | Name |
|---:|---|---|---:|---|---|
| 0 | `!WW_SND_SWING` | `swing` | 21 | `!WW_SND_ELECTRIC` | `electric` |
| 1 | `!WW_SND_HIT` | `hit` | 22 | `!WW_SND_ZANGIEF` | `zangief` |
| 2 | `!WW_SND_HURT` | `hurt` | 23 | `!WW_SND_YOGA_FIRE` | `yoga_fire` |
| 3 | `!WW_SND_KO` | `ko` | 24 | `!WW_SND_YOGA_FLAME` | `yoga_flame` |
| 4 | `!WW_SND_SPECIAL` | `special` | 25 | `!WW_SND_VEGA` | `vega` |
| 5 | `!WW_SND_SUPER` | `super` | 26 | `!WW_SND_TIGER` | `tiger` |
| 6 | `!WW_SND_BLOCK` | `block` | 27 | `!WW_SND_TIGER_UPPERCUT` | `tiger_uppercut` |
| 7 | `!WW_SND_SELECT` | `select` | 28 | `!WW_SND_PSYCHO` | `psycho` |
| 8 | `!WW_SND_HADOUKEN` | `hadouken` | 29 | `!WW_SND_LIGHTNING_KICK` | `lightning_kick` |
| 9 | `!WW_SND_SHORYUKEN` | `shoryuken` | 30 | `!WW_SND_CHUN_ATTACK` | `chun_attack` |
| 10 | `!WW_SND_TATSUMAKI` | `tatsumaki` | 31 | `!WW_SND_KIKOKEN` | `kikoken` |
| 11 | `!WW_SND_HONDA` | `honda` | 32 | `!WW_SND_BOSS_KO` | `boss_ko` |
| 12 | `!WW_SND_SONIC` | `sonic` | 33 | `!WW_SND_ALPHA_CHARGE` | `alpha_charge` |
| 13 | `!WW_SND_BIRD` | `bird` | 34 | `!WW_SND_AKUMA_SUPER` | `akuma_super` |
| 14 | `!WW_SND_CHUN_HURT` | `chun_hurt` | 35 | `!WW_SND_AKUMA_WARP` | `akuma_warp` |
| 15 | `!WW_SND_CHUN_KO` | `chun_ko` | 36 | `!WW_SND_PSYCHO_SUPER` | `psycho_super` |
| 16 | `!WW_SND_CONFIRM` | `confirm` | 37 | `!WW_SND_SHINKU` | `shinku` |
| 17 | `!WW_SND_YOU` | `you` | 38 | `!WW_SND_ALPHA_HADOUKEN` | `alpha_hadouken` |
| 18 | `!WW_SND_LOSE` | `lose` | 39 | `!WW_SND_PLAYER_SELECT` | `player_select` |
| 19 | `!WW_SND_PERFECT` | `perfect` | 40 | `!WW_SND_CONTINUE` | `continue` |
| 20 | `!WW_SND_BLANKA` | `blanka` | | | |

The two menu recordings are included and streamed in full. They are one-shots,
not looping replacements for SMW songs. Starting one does not change `$1DFB`;
menu integration must make any desired native music request separately.

## Conversion and reproducibility

Input may be uncompressed 8/16/24/32-bit PCM RIFF/WAV with any positive channel
count. Eight-bit PCM is unsigned; wider PCM is signed little-endian. Channels
are averaged with integer arithmetic. Integer linear resampling produces mono
8 kHz signed 16-bit PCM.

BRR uses 16 samples per nine-byte block. For every block the encoder tests all
four S-DSP filters and shifts 0 through 12, chooses minimum squared error with
filter/shift as deterministic tie-breakers, and marks only the final block with
the end flag. Production conversion decodes every generated stream as a
validity check. There are no external codecs, random seeds, floating-point
resampling steps, or platform libraries.

`manifest.json` records each source WAV's SHA-256, source/output rates and frame
counts, BRR byte/block counts, output SHA-256, native ARAM layout, and base-ROM
SHA-1. `--check` re-encodes every input and compares generated files byte for
byte without writing.

## ARAM and native-engine integration

`WW_SPCImage` contains the stock engine/SFX/music blocks plus the extension.
Only the initial engine-image pointer at CPU `$0080E8` is redirected. Native
sample uploads and subsequent level/overworld/credits uploads otherwise retain
their original code. The SPC image is at `$118000`, roster tables start at
`$128000`, and generated BRR streams start at `$6B0000`; these ranges do not
overlap the fighter action graphics or the ending pages at `$608000-$6A7FFF`.

| ARAM | Use |
|---|---|
| `$0000-$04FF` | Stock variables/stack; `$E6-$EF` direct page holds extension state |
| `$0500-$133D` | Stock SPC engine; `$0549` changes `MOV Y,$00FD` to `CALL $FC00` |
| `$055B/$0563/$056B` | Native APU0/APU1/APU3 handler calls relay to `$FC06/$FC09/$FC03` |
| `$1360-$555D` | Native music banks (maximum stock extent) |
| `$5570-$5FDA` | Stock SFX DSP settings and phrase streams |
| `$6000-$7FFF` | Native echo region, unchanged |
| `$8000-$804F` | Native 20-entry sample directory, unchanged |
| `$8050-$8053` | Extension directory entry 20, start=loop=`$F400` |
| `$8100-$F01F` | Stock BRR samples, unchanged |
| `$F400-$F7FA` | 227-block (2,043-byte) circular stream ring; five pad bytes follow |
| `$FC00-$FFBF` | Extension code and free space; assembly asserts no IPL overlap |
| `$FFC0-$FFFF` | SPC700 IPL ROM, never written |

`WW_SPC700_Poll` returns the native timer-counter value in Y, preserving stock
tempo. The same 2 ms timer credits one consumed 8 kHz BRR block per tick. CPU
writes are accepted only when credit says those bytes have been consumed. The
last physical ring block gets BRR end+loop only when it is not the source's
final block. Ring writes wrap before `$F800`, including the pad-safe boundary
case, so they cannot overwrite the driver. Underrun keys off voice 4 instead of
repeating stale ring data.

Custom port-3 commands are `$A5` begin, `$A6` block, `$A7` byte, and `$A8` stop.
Replies are `$5A` command ack, `$5B` block ready, `$60+index` byte ack, and `$81`
protocol error. A full ring receives no reply; CPU ready timeout means retry.
The SPC reads the native `SPCInEdge+3` input directly and can resume an `$A7`
byte transaction after returning to the native poll. Every acknowledgment uses
a four-phase release: CPU clears its input, SPC clears its output, and the CPU
waits for output zero before issuing another command. Committed blocks return
to the native sequencer between transfers.

APU2 is not used by the transport: pitch is fixed at `$0400`, and byte acks are
entirely on APU3. Native music port traffic and `$0006` music output are left
untouched. APU0/APU1 handlers remain active so native SFX and music-bank uploads
continue between custom blocks; voice 4 is reserved for the custom stream.

## Music/SFX behavior and limitations

- DSP voice 4 uses `$4C/$5C` key-on/off, pitch `$0400`, centered volume, and
  native-style ADSR. The native music sequencer and APU2 continue running.
  Voice 4 is software-muted from music DSP writes only while fighter audio owns
  it, then released, like a stock `$1DF9` effect.
- One custom stream plays at a time; a new request replaces the old one. Native
  `$1DF9/$1DFA` effects are suppressed while that stream is active, not mixed
  with it. Other native music channels continue. Integrations that require
  simultaneous native SFX must arbitrate them with `WW_AudioStop`.
- Native music-bank uploads are forbidden while custom audio is active. Call
  `WW_AudioStop` first, allow the native upload to finish, then request new
  fighter audio. This is required for level/music transitions.
- This is intentionally an 8 kHz mono, lossy track. Stereo width and
  high-frequency content are not preserved.
- Long clips reside in ROM, not ARAM, and need regular `WW_AudioUpdate` calls.
  Pausing updates can cause an underrun and terminate playback.
- Real-console audio and exact handshake timing on hardware remain unverified.
  Post-stream native music reinitialization is an integration responsibility,
  not a hardware-compatibility claim of this track.

## Verification performed

```sh
python3 -I rom/make_audio.py --self-test
python3 -I rom/make_audio.py --check
asar --error-limit=60 rom/audio.asm /tmp/audio-test.sfc
```

The six generated-PCM tests cover 8-bit stereo/16-bit mono WAV input, integer
resampling, silence and block layout, deterministic encoding/decode quality,
malformed BRR rejection, and manifest reproducibility. Production conversion
contains 41 streams, 57,058 BRR blocks, and 513,522 BRR bytes. Every embedded
BRR file is also compared byte for byte with its assembled ROM range.

A combined temporary assembly of `world_warrior.asm` plus `audio.asm` was used
for build checking, and the native 600-frame Snes9x title boot rendered the
normal title screen with non-silent native stereo audio. Temporary out-of-tree
reset-stage hooks additionally exercised nine-block updates through all 700
`hadouken` blocks and all 19,192 `player_select` blocks, including LoROM and
ring crossings. Both uploads returned status 0 with remaining count 0; final
ARAM checks showed all 227 credits restored and no driver overwrite. Captured
PCM contains non-silent custom playback. These tests do not constitute a
real-console or full gameplay audio-mix validation.
