# SNES hardware performance check

`make rom-perf` builds a separate LoROM diagnostic image and IPS:

- `local/smw-world-warrior-perf.sfc`
- `local/smw-world-warrior-perf.ips`
- `local/smw-world-warrior-perf.sym`

The ordinary `make rom` image does **not** contain the profiler and remains
byte-identical to the uninstrumented build.

## What it measures

At GM14 entry the diagnostic build sets CGRAM color zero to bright red. After
native sprite simulation and player drawing it restores black. The resulting
red raster band is a direct visible measurement of that frame's CPU workload,
including time spent in interrupts. It is not an emulator overlay.

It also latches the PPU VCounter at both points and keeps the worst elapsed
scanline count in WRAM:

| Address | Value |
| --- | --- |
| `$7F0D70-$7F0D71` | Start VCounter |
| `$7F0D72-$7F0D73` | End VCounter |
| `$7F0D74-$7F0D75` | Maximum elapsed scanlines |
| `$7F0D76-$7F0D77` | Audio start VCounter |
| `$7F0D78-$7F0D79` | Maximum audio request/update scanlines |
| `$7F0D7A-$7F0D7B` | Audio end VCounter |

The elapsed calculation wraps across the 262-line NTSC frame. SMW commonly
enters GM14 around scanline 247 and finishes after the wrap, so use elapsed
counters rather than comparing the absolute start/end positions. Counters reset
while the fighter is uninitialized. The red band and black transparent backdrop
are intentional diagnostic effects; use the normal ROM for ordinary play.

## Flash-cart procedure

1. Use an NTSC SNES/SNES-compatible console and a flash cart that supports an
   expanded, headerless LoROM without an enhancement chip.
2. Copy only `smw-world-warrior-perf.sfc` to the cart. Do not add a copier header.
3. Enter a normal level and confirm a bounded red band, responsive controls, and
   full-speed animation. The band can cross VBlank and continue near the top of
   the next frame; that wrap is normal for SMW's GM14 schedule.
4. Exercise all four projectile slots and repeated supers. Then use
   **Start+Select** to enter the Akuma practice finale and test both phases,
   reflected shots, and the final KO.
5. Capture at least 60 seconds of the worst case with a 240p-capable capture
   device or a high-frame-rate camera. A flash-cart/emulator debug interface can
   additionally read the WRAM counters above.

Elapsed values approaching the 262-line NTSC limit or repeated native lag frames
need investigation. A physical test report should record console model/revision,
flash-cart firmware, scene, maximum total/audio scanlines, and a video capture.
Emulator timings alone do not establish hardware speed.

## Recorded emulator baseline

The diagnostic build produced these Snes9x results; they are measurements for
comparison, not proof of real-console performance:

- Ordinary captured gameplay: approximately 83-116 elapsed scanlines and a
  188-line steady-combat maximum after resetting the counters at fight start.
- Akuma arena setup reaches a 257-line one-time maximum. After resetting the
  counters at fight start, scripted combat peaks at 237 lines, leaving 25 lines
  of NTSC headroom. Audio request/update peaks at 178 lines in that same fight.

The fight has measured emulator headroom, while arena setup remains the worst
transient. New-sound updates are deferred by one frame and Akuma frame resolution
runs once per frame. Protocol and sample-rate experiments that did not improve
this result or broke timing regressions were reverted; the normal ROM retains
the verified 8 kHz transport.

After running the normal `play.scene` and the profiler `play.scene`/`finale.scene`,
this command checks the 200-line ordinary and 245-line Akuma regression budgets:

```sh
python3 -I rom/verify_headless.py --performance
```

No SNES or flash-cart interface is attached to the development machine used for
this port, so this procedure and instrumentation are provided for the required
physical run rather than presented as completed hardware evidence.
