#!/usr/bin/env python3
"""Build-independent checks for the SNES artifacts and IPS round trip."""
import hashlib
from pathlib import Path
import unittest

from ips import apply_ips, create_ips

ROOT = Path(__file__).resolve().parents[1]


class GraphicsTests(unittest.TestCase):
    def test_exact_native_asset_sizes(self):
        expected = {"fighter.chr": 5120, "projectile.chr": 512,
                    "hud.chr": 128, "palette.bin": 32}
        for name, size in expected.items():
            with self.subTest(name=name):
                self.assertEqual(len((ROOT / "rom/generated" / name).read_bytes()), size)

    def test_palette_is_bgr15(self):
        data = (ROOT / "rom/generated/palette.bin").read_bytes()
        colors = [int.from_bytes(data[i:i+2], "little") for i in range(0, 32, 2)]
        self.assertEqual(len(colors), 16)
        self.assertTrue(all(0 <= color < 0x8000 for color in colors))
        self.assertEqual(colors[2], 30 | (30 << 5) | (29 << 10))  # White gi.


class IPSTests(unittest.TestCase):
    def test_expansion_round_trip(self):
        source = bytes(range(256)) * 2
        target = source + b"new SNES code\0" * 20
        patch = create_ips(source, target)
        self.assertEqual(apply_ips(source, patch), target)

    def test_scattered_changes_and_coalescing(self):
        source = bytearray(10000)
        target = bytearray(source)
        for offset in (0, 1, 12, 4096, 5000, 9999):
            target[offset] = 0xA5
        source, target = bytes(source), bytes(target)
        self.assertEqual(apply_ips(source, create_ips(source, target)), target)

    def test_identity(self):
        source = b"unchanged"
        self.assertEqual(apply_ips(source, create_ips(source, source)), source)


class ROMTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "local/smw-rebuilt.sfc").read_bytes()
        cls.target = (ROOT / "local/smw-world-warrior.sfc").read_bytes()

    def test_verified_base(self):
        self.assertEqual(hashlib.sha1(self.source).hexdigest(),
                         "6b47bb75d16514b6a476aa0c73a683a2a4c18765")

    def test_native_hooks(self):
        for offset, opcode in {0x21DA: 0x5C, 0x22F3: 0x5C, 0x62BD: 0x5C,
                               0x75B7: 0x5C, 0x2300: 0x5C}.items():
            self.assertEqual(self.target[offset], opcode, hex(offset))

    def test_only_hooks_and_header_change_in_vanilla_area(self):
        allowed = set()
        for start, size in ((0x21DA, 4), (0x22F3, 4), (0x62BD, 4), (0x75B7, 4), (0x2300, 4),
                            (0x0246, 5), (0x05D2, 7), (0x1C64, 6), (0x1F6F, 5),
                           (0x1A279, 4), (0x9FE0, 4), (0xAA42, 4), (0xC538, 4), (0xE6CE, 5),
                            (0xE8, 18), (0x80E8, 18), (0x7FD5, 1), (0x7FD7, 1), (0x7FDC, 4)):
            allowed.update(range(start, start + size))
        unexpected = [i for i in range(len(self.source))
                      if self.source[i] != self.target[i] and i not in allowed]
        self.assertEqual(unexpected[:20], [])

    def test_profiler_excluded_from_normal_build(self):
        symbols = (ROOT / "local/smw-world-warrior.sym").read_text()
        self.assertNotIn("WW_PerfStart", symbols)
        self.assertNotIn("WW_PerfAudioStart", symbols)

    def test_expansion_contains_generated_art(self):
        target = self.target
        fighter = (ROOT / "rom/generated/roster_action.chr").read_bytes()
        projectile = (ROOT / "rom/generated/roster_projectiles.chr").read_bytes()
        self.assertIn(fighter[:0x4000], target[len(self.source):])
        self.assertIn(projectile, target[len(self.source):])


if __name__ == "__main__":
    unittest.main()
