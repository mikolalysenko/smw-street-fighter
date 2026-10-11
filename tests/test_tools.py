import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'scripts' / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class AssetChecks(unittest.TestCase):
    def test_cps1_byte_and_word_lanes(self):
        tool = load('prepare_sf2')
        self.assertEqual(tool.interleave([b'ab', b'12'], 1), b'a1b2')
        self.assertEqual(tool.interleave([b'abcd', b'1234', b'WXYZ', b'!@#$'], 2), b'ab12WX!@cd34YZ#$')
        for parts, width in (([], 1), ([b'a', b'ab'], 1), ([b'abc'], 2), ([b'aa'], 0)):
            with self.assertRaises(ValueError):
                tool.interleave(parts, width)

    def test_rejects_wrong_region_or_modified_rom(self):
        tool = load('import_smw')
        for data in (b'', bytes(524288), bytes(524800), bytes(1048576)):
            with self.assertRaises(ValueError):
                tool.validate(data)

    def test_hook_refuses_ambiguous_or_missing_upstream_match(self):
        tool = load('prepare_smw')
        with self.assertRaises(RuntimeError):
            tool.replace_once('foo foo', 'foo', 'bar')
        with self.assertRaises(RuntimeError):
            tool.replace_once('foo', 'missing', 'bar')
        self.assertEqual(tool.replace_once('a foo b', 'foo', 'bar'), 'a bar b')
