#!/usr/bin/env python3
"""Assert WRAM results captured by the optional headless SNES scenes."""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def wram(folder: Path, tag: str) -> bytes:
    path = folder / f"{tag}.wram.bin"
    if not path.exists():
        raise SystemExit(f"Missing capture: {path}; see rom/HEADLESS.md")
    data = path.read_bytes()
    if len(data) != 131072:
        raise SystemExit(f"Unexpected WRAM size in {path}: {len(data)}")
    return data


def score(data: bytes) -> int:
    return int.from_bytes(data[0x0F34:0x0F37], "little")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"Headless verification failed: {message}")
    print(f"ok: {message}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--all", action="store_true",
                        help="also check combat fixtures and test-only damage captures")
    parser.add_argument("--roster", action="store_true",
                        help="also check Chun-Li selection, moves, audio, and buffered super")
    parser.add_argument("--all-roster", action="store_true",
                        help="check every fighter's neutral/up/down/super move matrix")
    parser.add_argument("--endings", action="store_true",
                        help="compare every ending page, tilemap, and palette with VRAM/CGRAM")
    parser.add_argument("--finale", action="store_true",
                        help="check Akuma combat, boss defeat, and ending transition")
    parser.add_argument("--carry", action="store_true",
                        help="check native carrying, power-up meter, and Map16 attacks")
    parser.add_argument("--performance", action="store_true",
                        help="check profiler captures against steady-combat scanline budgets")
    args = parser.parse_args()
    if not args.carry:
        play = ROOT / "local/rom-play"
        for tag, move in (("ryu-punch", 1), ("hadouken", 3), ("shoryuken", 5),
                          ("hurricane", 6), ("shinku", 4)):
            data = wram(play, tag)
            if tag == "hadouken":
                data = wram(play, "hadouken-start")
                require(data[0x10D80] == 1 and data[0x10D81] == 0,
                        "Hadouken projectile is active")
            else:
                require(data[0x10D03] == move, f"{tag}: native move state {move}")
        data = wram(play, "shinku")
        require(data[0x10D80] == 1 and data[0x10D81] == 1 and
                int.from_bytes(data[0x10D01:0x10D03], "little") == 200,
                "Shinku is active, has the super type, and leaves 200 meter after level 3")
    if args.roster:
        roster = ROOT / "local/rom-roster"
        data = wram(roster, "chunli-ready")
        require(data[0x10D13] == 2 and data[0x10D15] == 0 and data[0x10D16] == 1,
                "Roster confirms Chun-Li and resumes gameplay")
        data = wram(roster, "spinning-bird")
        require(data[0x10D03] == 6 and data[0x10E09] == 0,
                "Chun-Li Spinning Bird and audio transport are active")
        data = wram(roster, "lightning-legs")
        require(data[0x10D03] == 8 and data[0x10E09] == 0,
                "Chun-Li Lightning Legs and audio transport are active")
        data = wram(roster, "chunli-super")
        require(data[0x10D03] == 4 and
                int.from_bytes(data[0x10D01:0x10D03], "little") >= 200,
                "Buffered level-3 super starts and spends 100 meter")
    if args.all_roster:
        folder = ROOT / "local/rom-all-roster"
        names = ("ryu", "honda", "chunli", "guile", "ken", "blanka",
                 "zangief", "dhalsim", "balrog", "vega", "sagat", "bison")
        neutral = (3, 7, 3, 3, 3, 7, 6, 3, 7, 7, 3, 7)
        up = (5, 8, 6, 5, 5, 5, 9, 12, 5, 13, 5, 13)
        down = (6, 8, 8, 5, 6, 11, 6, 10, 7, 13, 3, 13)
        for index, name in enumerate(names):
            for kind, expected in (("special", neutral[index]), ("up", up[index]),
                                   ("down", down[index]), ("super", 4)):
                data = wram(folder, f"{name}-{kind}")
                require(data[0x10D13] == index and data[0x10D03] == expected and
                        data[0x10E09] == 0,
                        f"{name} {kind}: character {index}, move {expected}, audio OK")
            data = wram(folder, f"{name}-super")
            require(int.from_bytes(data[0x10D01:0x10D03], "little") == 200,
                    f"{name} super spends 100 meter")
    if args.finale:
        folder = ROOT / "local/rom-finale"
        data = wram(folder, "akuma")
        require(data[0x010B] == 0xC7 and data[0x10D15] == 4 and data[0x10D64] == 1,
                "C7 contains an active custom Akuma fight")
        data = wram(folder, "boss-super")
        require(data[0x10D03] != 0 and data[0x10E09] == 0,
                "Boss combat executes with valid audio transport")
        data = wram(folder, "ending")
        require(data[0x10D15] == 3 and data[0x10D64] == 0 and
                int.from_bytes(data[0x10D29:0x10D2B], "little") == 0,
                "Akuma defeat enters the native ending")
    if args.endings:
        folder = ROOT / "local/rom-endings"
        names = ("ryu", "honda", "chunli", "guile", "ken", "blanka",
                 "zangief", "dhalsim", "balrog", "vega", "sagat", "bison")
        pages = (ROOT / "rom/generated/ending_pages.chr").read_bytes()
        tilemap = (ROOT / "rom/generated/ending_tilemap.bin").read_bytes()
        palettes = (ROOT / "rom/generated/ending_pages.pal").read_bytes()
        for index, name in enumerate(names):
            vram = (folder / f"{name}-ending.vram.bin").read_bytes()
            cgram = (folder / f"{name}-ending.cgram.bin").read_bytes()
            require(vram[0x8000:0x8000 + 28672] == pages[index * 28672:(index + 1) * 28672] and
                    vram[0xF000:0xF000 + len(tilemap)] == tilemap and
                    cgram[:32] == palettes[index * 32:(index + 1) * 32],
                    f"{name} ending: exact native tiles, tilemap, and palette")
    if args.performance:
        for tag in ("ryu-punch", "hadouken-start", "shinku"):
            data = wram(ROOT / "local/rom-perf-play", tag)
            elapsed = int.from_bytes(data[0x10D74:0x10D76], "little")
            require(0 < elapsed <= 200,
                    f"{tag}: steady ordinary combat stays within 200 scanlines")
        data = wram(ROOT / "local/rom-perf-finale", "boss-super")
        elapsed = int.from_bytes(data[0x10D74:0x10D76], "little")
        audio = int.from_bytes(data[0x10D78:0x10D7A], "little")
        require(0 < elapsed <= 245 and 0 < audio <= 200,
                "Akuma super stays within 245 total and 200 audio scanlines")
    if args.carry:
        folder = ROOT / "local/rom-carry"
        data = wram(folder, "carrying")
        require(data[0x14D1] == 0x0B and data[0x1470] == 1 and data[0x148F] == 1,
                "Guard+Punch picks up and keeps the native sprite in carried state")
        require(data[0x10D69] == 0x40 and data[0x10D6A] == 0x20,
                "Native carry uses raw Y/L input without animation or sprite-lock corruption")
        data = wram(folder, "powerup")
        require(data[0x14D1] == 0 and data[0x10D00] == 100 and
                int.from_bytes(data[0x10D01:0x10D03], "little") == 150,
                "Native mushroom collection erases slot 9 and charges meter 100 to 150")

        folder = ROOT / "local/rom-map16"
        before = wram(folder, "map16-before")
        start = wram(folder, "map16-start")
        hit = wram(folder, "map16-hit")
        after = wram(folder, "map16-after")
        require(before[0xDC7B] == 0x2B and before[0x1DC7B] == 0,
                "Yoshi's Island 1 contains native Map16 tile $002B at X=$0CB0 Y=$0030")
        require(start[0x10D03] == 1 and start[0x10D21] == 1 and
                start[0x10D66] == 0 and start[0x1699:0x169D] != bytes(4) and
                (start[0xDC7B] | start[0x1DC7B] << 8) != 0x002B,
                "Melee lookup returns through the native trampoline and activates a bounce sprite")
        require((hit[0xDC7B] | hit[0x1DC7B] << 8) == 0x012B and
                hit[0x10D66] == 0,
                "Native Map16 handling converts the attack target to tile $012B")
        require(after[0x0100] == 0x14 and after[0x10D03] == 0 and
                after[0x10D66] == 0,
                "Ordinary gameplay resumes after the Map16 attack")
    if not args.all:
        return
    combat = ROOT / "local/rom-combat"
    data = wram(combat, "projectile-hit")
    require(data[0x10D01] >= 12 and score(data) >= 200,
            "Projectile hit awards score and meter")
    data = wram(combat, "melee-hit")
    require(data[0x10D01] >= 24 and score(data) >= 400,
            "Melee collision connects during active frames")
    damage = ROOT / "local/rom-damage"
    data = wram(damage, "direct-unblocked")
    require(data[0x10D00] == 80 and data[0x10D01] == 8,
            "Unblocked hit deals 20 damage and charges 8 meter")
    data = wram(damage, "direct-guarded")
    require(data[0x10D00] < 100 and data[0x10D01] >= 24,
            "Guard sequence includes reduced chip damage")
    data = wram(damage, "direct-parry")
    require(data[0x10D00] == 100 and data[0x10D01] == 24,
            "Fresh guard parries without damage and charges 24 meter")


if __name__ == "__main__":
    main()
