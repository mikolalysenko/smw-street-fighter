#!/usr/bin/env python3
"""Build the actual SNES ROM hack and a redistributable IPS patch."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SHA1 = "6b47bb75d16514b6a476aa0c73a683a2a4c18765"
sys.path.insert(0, str(ROOT / "rom"))
from ips import apply_ips, create_ips  # noqa: E402


def read_base(path: Path) -> bytes:
    data = path.read_bytes()
    if len(data) == 524800:
        data = data[512:]
    if len(data) != 524288 or hashlib.sha1(data).hexdigest() != SHA1:
        raise SystemExit(
            "Expected the unmodified USA SMW ROM (512 KiB, optional 512-byte "
            "copier header; SHA-1 6b47bb75d16514b6a476aa0c73a683a2a4c18765)."
        )
    return data


def build(base_path: Path, output_path: Path, patch_path: Path,
          performance: bool = False) -> None:
    source = read_base(base_path)
    assembler = shutil.which("asar")
    if not assembler:
        raise SystemExit("Asar 1.91 is required (https://github.com/RPGHacker/asar).")
    subprocess.run([sys.executable, "-I", str(ROOT / "rom/make_graphics.py")],
                   cwd=ROOT, check=True)
    sprites = ROOT / "local/sf2/sprites.dat"
    if not sprites.exists():
        raise SystemExit("Full roster art is missing. Run scripts/import_community_sf2.py "
                         "with the asset-tools Python environment first.")
    asset_python = ROOT / "build/asset-tools/bin/python3"
    subprocess.run([str(asset_python if asset_python.exists() else sys.executable),
                    "-I", "rom/make_roster_graphics.py", "--skip-previews"],
                   cwd=ROOT, check=True)
    subprocess.run([str(asset_python if asset_python.exists() else sys.executable),
                    "-I", "rom/make_select_assets.py"], cwd=ROOT, check=True)
    subprocess.run([str(asset_python if asset_python.exists() else sys.executable),
                    "-I", "rom/make_ending_pages.py"], cwd=ROOT, check=True)
    subprocess.run([sys.executable, "-I", "rom/make_audio.py"], cwd=ROOT, check=True)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    patch_path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="world-warrior-rom-") as folder:
        candidate = Path(folder) / "world-warrior.sfc"
        candidate.write_bytes(source)
        symbols = output_path.with_suffix(".sym")
        command = [
            assembler, "--error-limit=60", "--symbols=wla",
            f"--symbols-path={symbols}"
        ]
        if performance:
            command.append("-DWW_PERFORMANCE=1")
        command.extend(["rom/world_warrior.asm", str(candidate)])
        subprocess.run(command, cwd=ROOT, check=True)
        target = candidate.read_bytes()

    expected_hooks = {0x21DA: 0x5C, 0x22F3: 0x5C, 0x62BD: 0x5C, 0x75B7: 0x5C, 0x2300: 0x5C}
    for offset, opcode in expected_hooks.items():
        if target[offset] != opcode:
            raise SystemExit(f"Hook verification failed at PC ${offset:06X}")
    if len(target) <= len(source):
        raise SystemExit("Asar did not emit the expanded code/data bank")
    patch = create_ips(source, target)
    if apply_ips(source, patch) != target:
        raise SystemExit("Internal IPS round-trip verification failed")
    output_path.write_bytes(target)
    patch_path.write_bytes(patch)
    print(f"ROM:   {output_path} ({len(target):,} bytes)")
    print(f"Patch: {patch_path} ({len(patch):,} bytes)")
    print(f"SHA-256: {hashlib.sha256(target).hexdigest()}")
    print("IPS round-trip and native hook checks passed.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, default=ROOT / "local/smw-rebuilt.sfc",
                        help="unmodified USA SMW ROM")
    parser.add_argument("--output", type=Path, default=ROOT / "local/smw-world-warrior.sfc")
    parser.add_argument("--patch", type=Path, default=ROOT / "local/smw-world-warrior.ips")
    parser.add_argument("--performance", action="store_true",
                        help="include the raster-band and VCounter frame-time profiler")
    args = parser.parse_args()
    if not args.base.exists():
        raise SystemExit(f"Base ROM not found: {args.base}\n"
                         "Pass --base /path/to/'Super Mario World (USA).sfc', or run make original-assets.")
    build(args.base.expanduser().resolve(), args.output.resolve(), args.patch.resolve(),
          performance=args.performance)


if __name__ == "__main__":
    main()
