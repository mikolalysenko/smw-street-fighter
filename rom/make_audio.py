#!/usr/bin/env python3
"""Deterministic, standard-library WAV -> SNES BRR conversion for World Warrior.

Run ``python3 rom/make_audio.py`` after the pinned SF2 import.  Generated files
live in local/sf2/native-audio and are consumed by rom/audio.asm.  Use
``--check`` for a byte-for-byte reproducibility check or ``--self-test`` to run
unit tests with generated PCM (no imported assets required).
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import tempfile
import unittest
import wave

ROOT = Path(__file__).resolve().parents[1]
FORMAT_VERSION = 1
TARGET_RATE = 8000  # One 16-sample BRR block per SMW's 2 ms SPC timer tick.
SMW_SHA1 = "6b47bb75d16514b6a476aa0c73a683a2a4c18765"
SPC_HOOK_ROM_OFFSET = 0x7004D
SPC_HOOK_ORIGINAL = bytes.fromhex("ecfd00f0fb6de838")
SPC_HOOK_CALL = bytes.fromhex("3f00fc")  # CALL $FC00
SPC_NATIVE3_ROM_OFFSET = 0x7006F
SPC_NATIVE3_ORIGINAL = bytes.fromhex("3f1608cd03")
SPC_NATIVE3_CALL = bytes.fromhex("3f03fc")  # CALL $FC03 relay
SPC_NATIVE0_ROM_OFFSET = 0x7005F
SPC_NATIVE0_ORIGINAL = bytes.fromhex("3fae06cd00")
SPC_NATIVE0_CALL = bytes.fromhex("3f06fc")
SPC_NATIVE1_ROM_OFFSET = 0x70067
SPC_NATIVE1_ORIGINAL = bytes.fromhex("3fe509cd01")
SPC_NATIVE1_CALL = bytes.fromhex("3f09fc")

# IDs are part of the 65c816 ABI.  Append only; never reorder existing IDs.
SOUNDS = (
    "swing", "hit", "hurt", "ko", "special", "super", "block", "select",
    "hadouken", "shoryuken", "tatsumaki", "honda", "sonic", "bird",
    "chun_hurt", "chun_ko", "confirm", "you", "lose", "perfect",
    "blanka", "electric", "zangief", "yoga_fire", "yoga_flame", "vega",
    "tiger", "tiger_uppercut", "psycho", "lightning_kick", "chun_attack",
    "kikoken", "boss_ko", "alpha_charge", "akuma_super", "akuma_warp",
    "psycho_super", "shinku", "alpha_hadouken", "player_select", "continue",
)
MENU_SOUNDS = frozenset(("player_select", "continue"))


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _signed_round_div(value: int, divisor: int) -> int:
    """Round to nearest, with exact halves rounded away from zero."""
    if value < 0:
        return -((-value + divisor // 2) // divisor)
    return (value + divisor // 2) // divisor


def read_wav(path: Path) -> tuple[int, list[int]]:
    """Read an uncompressed RIFF WAV and deterministically downmix to s16 mono."""
    with wave.open(str(path), "rb") as source:
        channels = source.getnchannels()
        width = source.getsampwidth()
        rate = source.getframerate()
        frames = source.getnframes()
        compression = source.getcomptype()
        raw = source.readframes(frames)
    if channels < 1 or width not in (1, 2, 3, 4) or compression != "NONE":
        raise ValueError(f"{path}: expected uncompressed 8/16/24/32-bit PCM WAV")
    if not rate or len(raw) != frames * channels * width:
        raise ValueError(f"{path}: truncated or invalid PCM data")

    samples: list[int] = []
    step = channels * width
    for offset in range(0, len(raw), step):
        mixed = 0
        for channel in range(channels):
            at = offset + channel * width
            if width == 1:
                value = (raw[at] - 128) << 8
            elif width == 2:
                value = struct.unpack_from("<h", raw, at)[0]
            elif width == 3:
                value = int.from_bytes(raw[at:at + 3], "little", signed=False)
                if value & 0x800000:
                    value -= 0x1000000
                value >>= 8
            else:
                value = struct.unpack_from("<i", raw, at)[0] >> 16
            mixed += value
        samples.append(max(-32768, min(32767, mixed // channels)))
    return rate, samples


def resample(samples: list[int], source_rate: int, target_rate: int = TARGET_RATE) -> list[int]:
    """Integer linear resampling; no floating point or platform-dependent codecs."""
    if source_rate <= 0 or target_rate <= 0:
        raise ValueError("sample rates must be positive")
    if not samples:
        return []
    if source_rate == target_rate:
        return list(samples)
    output_frames = (len(samples) * target_rate + source_rate - 1) // source_rate
    result = []
    for index in range(output_frames):
        position = index * source_rate
        left = min(position // target_rate, len(samples) - 1)
        right = min(left + 1, len(samples) - 1)
        fraction = position % target_rate
        value = ((samples[left] * (target_rate - fraction)
                  + samples[right] * fraction) // target_rate)
        result.append(max(-32768, min(32767, value)))
    return result


def _prediction(filter_number: int, previous: int, older: int) -> int:
    # These are the S-DSP BRR reconstruction filters, expressed with arithmetic
    # shifts exactly as integer operations (negative shifts floor in Python).
    if filter_number == 0:
        return 0
    if filter_number == 1:
        return previous - (previous >> 4)
    if filter_number == 2:
        return 2 * previous - ((3 * previous) >> 5) - older + (older >> 4)
    return 2 * previous - ((13 * previous) >> 6) - older + ((3 * older) >> 4)


def _clip16(value: int) -> int:
    return max(-32768, min(32767, value))


def _decode_nibble(nibble: int, shift: int, prediction: int) -> int:
    signed = nibble - 16 if nibble & 8 else nibble
    # The right shift is the S-DSP's guard bit, not an optional volume scale.
    return _clip16(((signed << shift) >> 1) + prediction)


def encode_brr(samples: list[int]) -> bytes:
    """Encode one-shot BRR, selecting filter/shift by deterministic minimum MSE."""
    output = bytearray()
    previous = 0
    older = 0
    final_block = (len(samples) + 15) // 16
    for block_index in range(final_block):
        source = samples[block_index * 16:block_index * 16 + 16]
        source = source + [0] * (16 - len(source))
        best: tuple[int, int, int, bytes, int, int] | None = None
        for filter_number in range(4):
            for shift in range(13):
                nibbles = []
                decoded_previous = previous
                decoded_older = older
                error = 0
                for wanted in source:
                    prediction = _prediction(filter_number, decoded_previous, decoded_older)
                    quantum = 1 << shift
                    encoded = _signed_round_div(2 * (wanted - prediction), quantum)
                    encoded = max(-8, min(7, encoded))
                    decoded = _decode_nibble(encoded & 15, shift, prediction)
                    delta = wanted - decoded
                    error += delta * delta
                    nibbles.append(encoded & 15)
                    decoded_older = decoded_previous
                    decoded_previous = decoded
                packed = bytes((nibbles[i] << 4) | nibbles[i + 1]
                               for i in range(0, 16, 2))
                candidate = (error, filter_number, shift, packed,
                             decoded_previous, decoded_older)
                if best is None or candidate[:3] < best[:3]:
                    best = candidate
        assert best is not None
        _, filter_number, shift, packed, previous, older = best
        end = int(block_index + 1 == final_block)
        output.append((shift << 4) | (filter_number << 2) | end)
        output.extend(packed)
    return bytes(output)


def decode_brr(data: bytes) -> list[int]:
    """Reference decoder used by self-tests and to sanity-check generated assets."""
    if not data or len(data) % 9:
        raise ValueError("BRR must contain one or more complete 9-byte blocks")
    samples = []
    previous = 0
    older = 0
    ended = False
    for offset in range(0, len(data), 9):
        if ended:
            raise ValueError("data follows the BRR end block")
        header = data[offset]
        shift = header >> 4
        filter_number = (header >> 2) & 3
        if shift > 12:
            raise ValueError("reserved BRR shift value")
        for index in range(16):
            packed = data[offset + 1 + index // 2]
            nibble = packed >> 4 if not index & 1 else packed & 15
            value = _decode_nibble(nibble, shift, _prediction(filter_number, previous, older))
            samples.append(value)
            older, previous = previous, value
        if header & 1:
            ended = True
    if not ended:
        raise ValueError("BRR has no end block")
    return samples


def convert_wav(path: Path) -> tuple[bytes, dict]:
    source = path.read_bytes()
    source_rate, pcm = read_wav(path)
    converted = resample(pcm, source_rate)
    brr = encode_brr(converted)
    # Decode every result during production, not just in tests.  This catches
    # malformed blocks and accidental non-terminating streams before assembly.
    decode_brr(brr)
    info = {
        "source_sha256": sha256(source),
        "source_rate": source_rate,
        "source_frames": len(pcm),
        "output_rate": TARGET_RATE,
        "output_frames": len(converted),
        "blocks": len(brr) // 9,
        "brr_bytes": len(brr),
        "brr_sha256": sha256(brr),
    }
    return brr, info


def _native_blocks(base_path: Path) -> dict[str, bytes]:
    base = base_path.read_bytes()
    if len(base) == 524800:
        base = base[512:]
    if len(base) != 524288 or hashlib.sha1(base).hexdigest() != SMW_SHA1:
        raise ValueError("native SPC extraction requires the verified unmodified USA SMW ROM")
    if base[SPC_HOOK_ROM_OFFSET:SPC_HOOK_ROM_OFFSET + len(SPC_HOOK_ORIGINAL)] != SPC_HOOK_ORIGINAL:
        raise ValueError("SMW SPC main-loop hook signature not found")
    if base[SPC_NATIVE3_ROM_OFFSET:SPC_NATIVE3_ROM_OFFSET + len(SPC_NATIVE3_ORIGINAL)] != SPC_NATIVE3_ORIGINAL:
        raise ValueError("SMW SPC port-3 relay signature not found")
    if base[SPC_NATIVE0_ROM_OFFSET:SPC_NATIVE0_ROM_OFFSET + len(SPC_NATIVE0_ORIGINAL)] != SPC_NATIVE0_ORIGINAL:
        raise ValueError("SMW SPC port-0 relay signature not found")
    if base[SPC_NATIVE1_ROM_OFFSET:SPC_NATIVE1_ROM_OFFSET + len(SPC_NATIVE1_ORIGINAL)] != SPC_NATIVE1_ORIGINAL:
        raise ValueError("SMW SPC port-1 relay signature not found")
    engine = bytearray(base[0x70004:0x70004 + 3646])
    hook = SPC_HOOK_ROM_OFFSET - 0x70004
    engine[hook:hook + 3] = SPC_HOOK_CALL
    relay = SPC_NATIVE3_ROM_OFFSET - 0x70004
    engine[relay:relay + 3] = SPC_NATIVE3_CALL
    relay0 = SPC_NATIVE0_ROM_OFFSET - 0x70004
    engine[relay0:relay0 + 3] = SPC_NATIVE0_CALL
    relay1 = SPC_NATIVE1_ROM_OFFSET - 0x70004
    engine[relay1:relay1 + 3] = SPC_NATIVE1_CALL
    return {
        "spc_engine.bin": bytes(engine),
        "spc_sfx.bin": base[0x70E46:0x70E46 + 2667],
        "spc_music1.bin": base[0x718B5:0x718B5 + 5661],
    }


def _assembly_include(output_dir: Path, outputs: dict[str, bytes]) -> bytes:
    """Split incbins at LoROM borders without changing the linear SNES layout."""
    lines = ["; Generated by rom/make_audio.py; do not edit.",
             "; Ranges are end-exclusive. BRR bytes stay contiguous across banks."]
    address = 0x6B0000
    for name in SOUNDS:
        path = (output_dir / (name + ".brr")).resolve().as_posix()
        size = len(outputs[name + ".brr"])
        lines.append(f"WW_BRR_{name}_Blocks = {size // 9}")
        lines.append(f"WW_BRR_{name}:")
        offset = 0
        while offset < size:
            chunk = min(size - offset, 0x10000 - (address & 0xFFFF))
            lines.append(f'    incbin "{path}":({offset})..({offset + chunk})')
            address += chunk
            if address & 0xFFFF == 0:
                address += 0x8000         # next LoROM bank starts at $xx8000
                lines.append(f"    org ${address:06X}")
            offset += chunk
        lines.append(f"WW_BRR_{name}_End:")
    return ("\n".join(lines) + "\n").encode()


def _descriptor_include() -> bytes:
    lines = ["; Generated numeric lengths remain valid across LoROM org changes."]
    for name in SOUNDS:
        lines.append(f"    dl WW_BRR_{name} : dw WW_BRR_{name}_Blocks : db $70,$00,$00")
    return ("\n".join(lines) + "\n").encode()


def build_assets(input_dir: Path, output_dir: Path, base_path: Path | None,
                 check: bool = False) -> dict:
    missing = [name + ".wav" for name in SOUNDS if not (input_dir / (name + ".wav")).is_file()]
    if missing:
        raise FileNotFoundError("missing imported sounds: " + ", ".join(missing))
    outputs: dict[str, bytes] = {}
    records = []
    for sound_id, name in enumerate(SOUNDS):
        brr, info = convert_wav(input_dir / (name + ".wav"))
        info.update(id=sound_id, name=name, file=name + ".brr",
                    kind="menu" if name in MENU_SOUNDS else "voice/effect")
        records.append(info)
        outputs[name + ".brr"] = brr
    if base_path is not None:
        outputs.update(_native_blocks(base_path))
    outputs["audio_data.asm"] = _assembly_include(output_dir, outputs)
    outputs["audio_descriptors.asm"] = _descriptor_include()
    manifest = {
        "format": FORMAT_VERSION,
        "codec": "SNES BRR, exhaustive filter/shift minimum-MSE, integer arithmetic",
        "source_dir": "local/sf2/audio",
        "target_rate": TARGET_RATE,
        "sound_ids": list(SOUNDS),
        "sounds": records,
        "native_spc": {
            "base_sha1": SMW_SHA1 if base_path is not None else None,
            "main_loop_aram": 0x0549,
            "native_port3_call_aram": 0x056B,
            "native_port0_call_aram": 0x055B,
            "native_port1_call_aram": 0x0563,
            "driver_aram": 0xFC00,
            "port3_relay_aram": 0xFC03,
            "port0_relay_aram": 0xFC06,
            "port1_relay_aram": 0xFC09,
            "ring_aram": 0xF400,
            "ring_bytes": 2048,
            "sample_directory_entry": 20,
        },
    }
    outputs["manifest.json"] = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()
    if check:
        mismatches = [name for name, data in outputs.items()
                      if not (output_dir / name).is_file() or (output_dir / name).read_bytes() != data]
        if mismatches:
            raise ValueError("stale or missing generated audio: " + ", ".join(mismatches))
    else:
        output_dir.mkdir(parents=True, exist_ok=True)
        for name, data in outputs.items():
            path = output_dir / name
            if not path.exists() or path.read_bytes() != data:
                path.write_bytes(data)
    return manifest


def write_test_wav(path: Path, samples: list[int], rate: int = 8000,
                   channels: int = 1, width: int = 2) -> None:
    raw = bytearray()
    for sample in samples:
        for _ in range(channels):
            if width == 1:
                raw.append((sample >> 8) + 128)
            elif width == 2:
                raw.extend(struct.pack("<h", sample))
            else:
                raise ValueError("test writer supports 8/16-bit PCM")
    with wave.open(str(path), "wb") as output:
        output.setnchannels(channels)
        output.setsampwidth(width)
        output.setframerate(rate)
        output.writeframes(raw)


class AudioTests(unittest.TestCase):
    def test_wav_pcm8_stereo_and_pcm16(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "pcm.wav"
            write_test_wav(path, [-32768, 0, 32512], channels=2, width=1)
            rate, samples = read_wav(path)
            self.assertEqual(rate, 8000)
            self.assertEqual(samples, [-32768, 0, 32512])
            write_test_wav(path, [-12345, 0, 12345], width=2)
            self.assertEqual(read_wav(path)[1], [-12345, 0, 12345])

    def test_integer_resampling(self) -> None:
        self.assertEqual(resample([1000, 2000], 2, 4), [1000, 1500, 2000, 2000])
        self.assertEqual(resample([], 48000), [])
        self.assertEqual(resample([1, -1], 8000), [1, -1])

    def test_brr_silence_and_layout(self) -> None:
        encoded = encode_brr([0] * 33)
        self.assertEqual(len(encoded), 27)
        self.assertEqual(encoded, b"\0" * 18 + b"\x01" + b"\0" * 8)
        self.assertEqual(decode_brr(encoded), [0] * 48)

    def test_brr_deterministic_and_decodable(self) -> None:
        pcm = [int(12000 * math.sin(index * math.tau * 220 / 8000)) for index in range(800)]
        first = encode_brr(pcm)
        self.assertEqual(first, encode_brr(pcm))
        self.assertEqual(first[-9] & 1, 1)
        self.assertTrue(all((first[offset] & 1) == 0 for offset in range(0, len(first) - 9, 9)))
        decoded = decode_brr(first)
        signal = sum(value * value for value in pcm)
        error = sum((wanted - got) ** 2 for wanted, got in zip(pcm, decoded))
        self.assertGreater(signal / max(1, error), 20.0)

    def test_invalid_brr_rejected(self) -> None:
        for data in (b"", b"\0" * 8, b"\0" * 9, b"\x01" + b"\0" * 8 + b"\0" * 9):
            with self.assertRaises(ValueError):
                decode_brr(data)

    def test_manifest_reproducible(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source = root / "source"
            source.mkdir()
            for name in SOUNDS:
                write_test_wav(source / (name + ".wav"), [0, 1000, -1000, 0])
            first = build_assets(source, root / "out", None)
            build_assets(source, root / "out", None, check=True)
            self.assertEqual(first, json.loads((root / "out/manifest.json").read_text()))
            for record in first["sounds"]:
                data = (source / (record["name"] + ".wav")).read_bytes()
                self.assertEqual(record["source_sha256"], sha256(data))
                self.assertEqual(record["blocks"], 1)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=ROOT / "local/sf2/audio")
    parser.add_argument("--output", type=Path, default=ROOT / "local/sf2/native-audio")
    parser.add_argument("--base", type=Path, default=ROOT / "local/smw-rebuilt.sfc")
    parser.add_argument("--no-native", action="store_true", help="do not extract the stock SPC upload blocks")
    parser.add_argument("--check", action="store_true", help="verify generated bytes without writing")
    parser.add_argument("--self-test", action="store_true", help="run generated-PCM unit tests")
    args = parser.parse_args()
    if args.self_test:
        unittest.main(argv=[__file__, "-v"])
        return
    manifest = build_assets(args.input.expanduser().resolve(), args.output.expanduser().resolve(),
                            None if args.no_native else args.base.expanduser().resolve(), args.check)
    total = sum(record["brr_bytes"] for record in manifest["sounds"])
    action = "Verified" if args.check else "Generated"
    print(f"{action} {len(SOUNDS)} BRR streams ({total:,} bytes) in {args.output}")


if __name__ == "__main__":
    main()
