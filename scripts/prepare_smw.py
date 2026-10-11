#!/usr/bin/env python3
"""Create a reproducible patched build tree; never edit the pinned submodule."""
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PIN = 'eae20c65c58930c8b62c76188d259579ad4130f1'


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError(f'Hook must match exactly once: {old[:100]!r}')
    return text.replace(old, new, 1)


def prepare():
    source, dest = ROOT / 'references/smw', ROOT / 'build/smw'
    if not (source / 'src/main.c').exists():
        raise SystemExit('Run git submodule update --init references/smw first.')
    revision = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != PIN:
        raise SystemExit(f'Expected SMW {PIN}; got {revision}. Review hooks before updating the pin.')
    # Copy only tracked files. No ROMs, personal config, build products or saves.
    files = subprocess.check_output(['git', '-C', str(source), 'ls-files', '-z']).decode().split('\0')
    for name in filter(None, files):
        target = dest / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / name, target)
    for file in ('fight.c', 'fight.h', 'draw.c', 'draw.h', 'audio.c', 'audio.h', 'assets.c', 'assets.h'):
        shutil.copy2(ROOT / 'src' / file, dest / 'src' / file)
    for file in ('ww_smw.c', 'ww_smw.h'):
        shutil.copy2(ROOT / 'adapters/smw' / file, dest / 'src' / file)
    def patch(file, changes):
        path = dest / file
        text = path.read_text()
        for old, new in changes:
            text = replace_once(text, old, new)
        path.write_text(text)
    patch('src/main.c', [
        ('int main(int argc, char** argv) {',
         'int main(int argc, char** argv) {\n'
         '  char *ww_base = SDL_GetBasePath();\n'
         '  if (ww_base) { chdir(ww_base); SDL_free(ww_base); }'),
        ('#include <stdio.h>', '#include "ww_smw.h"\n#include "audio.h"\n#include <stdio.h>'),
        ('static uint32 g_input_state;', 'static uint32 g_input_state, ww_pending_buttons;'),
        ('    uint32 m = 1 << kKbdRemap[j - kKeys_Controls];',
         '    uint32 m = 1 << kKbdRemap[j - kKeys_Controls];\n    if (pressed) ww_pending_buttons |= m;'),
        ('    uint32 inputs = g_input_state | g_gamepad[0].axis_buttons | g_gamepad[1].axis_buttons << 12;',
         '    uint32 inputs = g_input_state | ww_pending_buttons | g_gamepad[0].axis_buttons | g_gamepad[1].axis_buttons << 12;\n'
         '    ww_pending_buttons = 0;'),
        ('  bool custom_size =', '  ww_init();\n  bool custom_size ='),
        ('    uint8 is_replay = RtlRunFrame(inputs | GetActiveControllers());',
         '    uint8 is_replay = 0;\n    if (ww_before_frame(&inputs))\n      is_replay = RtlRunFrame(inputs | GetActiveControllers());'),
        ('  g_renderer_funcs.EndDraw();',
         '  ww_render(pixel_buffer, pitch, g_snes_width, g_snes_height);\n  g_renderer_funcs.EndDraw();'),
        ('  SDL_DestroyWindow(window);', '  fc_audio_shutdown();\n  SDL_DestroyWindow(window);'),
        ('static void HandleCommand(uint32 j, bool pressed) {',
         'static void HandleCommand(uint32 j, bool pressed) {\n'
         '  // Legacy snapshots/replays omit combat state; do not create corrupt saves.\n'
         '  if ((j >= kKeys_Load && j <= kKeys_ReplayRef_Last) || j == kKeys_ToggleWhichFrame) return;'),
    ])
    patch('src/common_cpu_infra.c', [
        ('uint8 g_runmode = RM_BOTH;', 'uint8 g_runmode = RM_MINE; // Combat deliberately diverges from the original ROM.')
    ])
    patch('src/smw_00.c', [
        ('#include "consts.h"', '#include "ww_smw.h"\n#include "consts.h"'),
        ('void DamagePlayer_Hurt() {  // 00f5b7', 'void DamagePlayer_Hurt() {  // 00f5b7\n  if (ww_damage()) return;'),
        ('void DamagePlayer_PitFall() {  // 00f60a\n  io_music_ch1 = 9;',
         'void DamagePlayer_PitFall() {  // 00f60a\n  ww_on_death();\n  io_music_ch1 = ww_enabled() ? 0 : 9;'),
        ('void UpdatePlayerSpritePosition() {  // 00dc2d',
         'void UpdatePlayerSpritePosition() {  // 00dc2d\n  ww_velocity();'),
        ('void PlayerDraw() {  // 00e2bd',
         'void PlayerDraw() { ww_player_draw(); }\n\nvoid WW_OriginalPlayerDraw() {  // 00e2bd'),
        ('void CheckPowerUpSpecificPlayerAttacks() {  // 00d062',
         'void CheckPowerUpSpecificPlayerAttacks() {  // 00d062\n  if (ww_enabled()) return;'),
    ])
    for config in ('smw.ini', 'tv.ini'):
        shutil.copy2(ROOT / 'config' / config, dest / config)
    assets = ROOT / 'local/smw_assets.dat'
    if assets.exists():
        shutil.copy2(assets, dest / 'smw_assets.dat')
    if (ROOT / 'local/sf2/sprites.dat').exists():
        shutil.copytree(ROOT / 'local/sf2', dest / 'sf2', dirs_exist_ok=True)
    print(f'Prepared {dest} from SMW {revision}')


if __name__ == '__main__':
    prepare()
