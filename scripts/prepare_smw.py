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
    for file in ('fight.c', 'fight.h', 'draw.c', 'draw.h', 'audio.c', 'audio.h', 'assets.c', 'assets.h', 'ending_text.h'):
        # Upstream Make does not track header dependencies. Refresh shared
        # source timestamps too, so new pose IDs cannot leave a stale reader.
        shutil.copyfile(ROOT / 'src' / file, dest / 'src' / file)
    for file in ('ww_smw.c', 'ww_smw.h', 'ww_targets.c', 'ww_targets.h',
                 'ww_title.c', 'ww_title.h', 'ww_score.c', 'ww_score.h',
                 'ww_hud.c', 'ww_hud.h',
                 'ww_finalboss.c', 'ww_finalboss.h', 'ww_ending.c', 'ww_ending.h'):
        shutil.copyfile(ROOT / 'adapters/smw' / file, dest / 'src' / file)
    def patch(file, changes):
        path = dest / file
        text = path.read_text()
        for old, new in changes:
            text = replace_once(text, old, new)
        path.write_text(text)
    patch('src/main.c', [
        ('static const char kWindowTitle[] = "SMW";',
         'static const char kWindowTitle[] = "Super Mario World II - The World Warrior";'),
        ('int main(int argc, char** argv) {',
         'int main(int argc, char** argv) {\n'
         '  char *ww_base = SDL_GetBasePath();\n'
         '  if (ww_base) { chdir(ww_base); SDL_free(ww_base); }'),
        ('#include <stdio.h>', '#include "ww_smw.h"\n#include "audio.h"\n#include "assets.h"\n#include <stdio.h>\nstatic int ww_smoke_frames;'),
        ('  argc--, argv++;',
         '  // Strip our option before the original --config/ROM parser.\n'
         '  for (int i=1;i<argc;) {\n'
         '    if (!strcmp(argv[i], "--smoke-test")) ww_smoke_frames=360;\n'
         '    else if (!strcmp(argv[i], "--debug")) ww_set_debug(true);\n'
         '    else { ++i; continue; }\n'
         '    memmove(argv+i,argv+i+1,(argc-i)*sizeof(*argv)); --argc;\n'
         '  }\n'
         '  argc--, argv++;'),
        ('static uint32 g_input_state;', 'static uint32 g_input_state, ww_pending_buttons;'),
        ('  ParseConfigFile(config_file);',
         '  ParseConfigFile(config_file);\n'
         '  if (ww_smoke_frames) { g_config.disable_frame_delay=true; g_config.fullscreen=0;\n'
         '    g_config.output_method=kOutputMethod_SDLSoftware; g_config.window_scale=1; }'),
        ('    uint32 m = 1 << kKbdRemap[j - kKeys_Controls];',
         '    uint32 m = 1 << kKbdRemap[j - kKeys_Controls];\n    if (pressed) ww_pending_buttons |= m;'),
        ('    uint32 inputs = g_input_state | g_gamepad[0].axis_buttons | g_gamepad[1].axis_buttons << 12;',
         '    uint32 inputs = g_input_state | ww_pending_buttons | g_gamepad[0].axis_buttons | g_gamepad[1].axis_buttons << 12;\n'
         '    ww_pending_buttons = 0;'),
        ('  bool custom_size =', '  fc_audio_use_shared_device();\n  ww_init();\n  if (!fc_assets_ready()) Die("Missing fighter assets: keep the complete release folder together.");\n  bool custom_size ='),
        ('    uint8 is_replay = RtlRunFrame(inputs | GetActiveControllers());',
         '    uint8 is_replay = 0;\n    if (ww_before_frame(&inputs))\n      is_replay = RtlRunFrame(inputs | GetActiveControllers());'),
        ('  g_renderer_funcs.EndDraw();',
         '  ww_render(pixel_buffer, pitch, g_snes_width, g_snes_height);\n  g_renderer_funcs.EndDraw();'),
        ('  SDL_DestroyWindow(window);', '  fc_audio_shutdown();\n  SDL_DestroyWindow(window);'),
        ('    if (!g_snes->disableRender && !g_config.disable_frame_delay) {',
         '    if (ww_smoke_frames && !--ww_smoke_frames) {\n'
         '      RtlWriteSram(); puts("Package smoke: original title, fighter assets, audio and save write passed."); running=false;\n'
         '    }\n'
         '    if (!g_snes->disableRender && !g_config.disable_frame_delay) {'),
        ('static void SDLCALL AudioCallback(void *userdata, Uint8 *stream, int len) {',
         'static void SDLCALL AudioCallback(void *userdata, Uint8 *stream, int len) {\n'
         '  Uint8 *ww_output=stream; int ww_frames=len/4;\n'
         '  bool ww_music=fc_audio_music_active();'),
        ('  SDL_UnlockMutex(g_audio_mutex);\n}\n\n\n// State for sdl renderer',
         '  SDL_UnlockMutex(g_audio_mutex);\n'
         '  fc_audio_mix_s16((int16*)ww_output,ww_frames,ww_music?0.0f:0.65f);\n}\n\n\n// State for sdl renderer'),
        ('    want.freq = g_config.audio_freq;', '    want.freq = 48000; // Shared with the 48 kHz fighter voices.'),
        ('static void HandleCommand(uint32 j, bool pressed) {',
         'static void HandleCommand(uint32 j, bool pressed) {\n'
         '  // Legacy snapshots/replays omit combat state; do not create corrupt saves.\n'
         '  if ((j >= kKeys_Load && j <= kKeys_ReplayRef_Last) || j == kKeys_ToggleWhichFrame) return;'),
    ])
    patch('src/common_cpu_infra.c', [
        ('uint8 g_runmode = RM_BOTH;', 'uint8 g_runmode = RM_MINE; // Combat deliberately diverges from the original ROM.')
    ])
    patch('src/common_rtl.c', [
        ('#include "common_rtl.h"', '#include "common_rtl.h"\n#include "ww_score.h"'),
        ('  char filename[64];\n  snprintf(filename, sizeof(filename), "saves/%s.srm", g_rtl_game_info->title);',
         '  char filename[4096];\n  snprintf(filename, sizeof(filename), "%s/%s.srm", ww_save_directory(), g_rtl_game_info->title);'),
        ('  FILE *f = fopen(filename, "rb");\n  if (f) {\n    if (fread(g_sram, 1, g_sram_size, f)',
         '  FILE *f = fopen(filename, "rb");\n'
         '  if (!f && !getenv("WW_SAVE_DIR")) {\n'
         '    snprintf(filename,sizeof(filename),"saves/%s.srm",g_rtl_game_info->title);\n'
         '    f=fopen(filename,"rb"); /* Import a legacy portable save once. */\n'
         '  }\n'
         '  if (f) {\n    if (fread(g_sram, 1, g_sram_size, f)'),
        ('  char filename[64], filename_bak[64];\n'
         '  snprintf(filename, sizeof(filename), "saves/%s.srm", g_rtl_game_info->title);\n'
         '  snprintf(filename_bak, sizeof(filename_bak), "saves/%s.srm.bak", g_rtl_game_info->title);',
         '  char filename[4096], filename_bak[4096];\n'
         '  snprintf(filename, sizeof(filename), "%s/%s.srm", ww_save_directory(), g_rtl_game_info->title);\n'
         '  snprintf(filename_bak, sizeof(filename_bak), "%s/%s.srm.bak", ww_save_directory(), g_rtl_game_info->title);'),
    ])
    patch('src/smw_00.c', [
        ('#include "consts.h"', '#include "ww_smw.h"\n#include "ww_ending.h"\n#include "consts.h"'),
        ('  if (misc_currently_active_boss_end_cutscene) {\n    uint8 v2 = 8;',
         '  if (ww_ending_finish_level()) j = 24;\n'
         '  else if (misc_currently_active_boss_end_cutscene) {\n    uint8 v2 = 8;'),
        ('void SmwRunOneFrameOfGame_Internal() {',
         'void SmwRunOneFrameOfGame_Internal() {\n  ww_begin_frame();'),
        ('void DamagePlayer_Hurt() {  // 00f5b7', 'void DamagePlayer_Hurt() {  // 00f5b7\n  if (ww_damage()) return;'),
        ('void SaveGame() {  // 009bc9', 'void SaveGame() {  // 009bc9\n  if (ww_practicing()) return;'),
        ('void DamagePlayer_PitFall() {  // 00f60a\n  io_music_ch1 = 9;',
         'void DamagePlayer_PitFall() {  // 00f60a\n  ww_on_death();\n  io_music_ch1 = ww_enabled() ? 0 : 9;'),
        ('void UpdatePlayerSpritePosition() {  // 00dc2d',
         'void UpdatePlayerSpritePosition() {  // 00dc2d\n  ww_velocity();\n  if (ww_psycho_position()) return;'),
        ('  player_yspeed = tmp8a;\n}', '  player_yspeed = tmp8a;\n  ww_position_end();\n}'),
        ('void HandlePlayerLevelCollision() {  // 00e92b',
         'void HandlePlayerLevelCollision() {  // 00e92b\n  if (ww_psycho_collision()) { ResetPlayerLevelCollisionRAM(); return; }'),
        ('void PlayerDraw() {  // 00e2bd',
         'void PlayerDraw() { ww_player_draw(); }\n\nvoid WW_OriginalPlayerDraw() {  // 00e2bd'),
        ('  if (player_hide_player_tile_flags != 0xFF)\n    PlayerGFXRt_01EA70();',
         '  if (!ww_enabled() && player_hide_player_tile_flags != 0xFF)\n    PlayerGFXRt_01EA70();'),
        ('void UpdateStatusBarCounters_DrawItemBoxItem() {  // 009079',
         'void UpdateStatusBarCounters_DrawItemBoxItem() {  // 009079\n  if (ww_hide_native_hud()) return;'),
        ('void CheckPowerUpSpecificPlayerAttacks() {  // 00d062',
         'void CheckPowerUpSpecificPlayerAttacks() {  // 00d062\n  if (ww_enabled()) return;'),
    ])
    patch('src/smw_rtl.c', [
        ('#include "smw_rtl.h"', '#include "smw_rtl.h"\n#include "ww_smw.h"\n#include "ww_title.h"\n#include "ww_score.h"'),
        ('  for (int i = 0; i <= 224; i++) {',
         '  ww_title_begin(g_ppu);\n  for (int i = 0; i <= 224; i++) {'),
        ('  }\n}\n\nvoid SmwRunOneFrameOfGame(void)',
         '  }\n  ww_title_end(g_ppu);\n}\n\nvoid SmwRunOneFrameOfGame(void)'),
        ('  SmwVectorNMI();\n}', '  SmwVectorNMI();\n  ww_update_high_score();\n}'),
        ('    ppu_runLine(g_ppu, i);',
         '    // Only the status-bar scanlines: preserve layer-3 tides, smashers and messages.\n'
         '    uint8 ww_main = g_ppu->screenEnabled[0], ww_sub = g_ppu->screenEnabled[1];\n'
         '    if (ww_hide_native_hud() && i <= 40) {\n'
         '      g_ppu->screenEnabled[0] &= ~4; g_ppu->screenEnabled[1] &= ~4;\n'
         '    }\n'
         '    ppu_runLine(g_ppu, i);\n'
         '    g_ppu->screenEnabled[0] = ww_main; g_ppu->screenEnabled[1] = ww_sub;'),
    ])
    patch('src/smw_07.c', [
        ('#include "consts.h"', '#include "ww_targets.h"\n#include "consts.h"'),
        ('void InitializeNormalSpriteRAMTables(uint8 k) {',
         'void InitializeNormalSpriteRAMTables(uint8 k) {\n  ww_target_spawned(k);'),
    ])
    patch('src/smw_01.c', [
        ('#include "consts.h"', '#include "ww_smw.h"\n#include "ww_targets.h"\n#include "consts.h"'),
        ('void ProcessNormalSprites_HandleSprite(uint8 k) {  // 018127',
         'void WW_OriginalHandleSprite(uint8 k);\n'
         'void ProcessNormalSprites_HandleSprite(uint8 k) { if (!ww_grapple_sprite(k)) WW_OriginalHandleSprite(k); }\n\n'
         'void WW_OriginalHandleSprite(uint8 k) {  // 018127'),
        ('  SetSprYPos(k, py + v4);', '  SetSprYPos(k, py + v4);\n  ww_carried_position(k);'),
        ('if (StandardSpriteToSpriteCollisionChecks_CheckContact(&ci) && !spr_decrementing_table154c[k]) {',
         'if ((StandardSpriteToSpriteCollisionChecks_CheckContact(&ci) || ww_keyhole_contact(k)) && !spr_decrementing_table154c[k]) {'),
        ('spr_table00c2[k] == 1 && (io_controller_press1 & 0x40) != 0',
         'spr_table00c2[k] == 1 && (ww_enabled() ? ww_yoshi_tongue_pressed() : (io_controller_press1 & 0x40) != 0)'),
        ('void SprXXX_PowerUps_01C538(uint8 k, uint8 a) {  // 01c538',
         'void SprXXX_PowerUps_01C538(uint8 k, uint8 a) {  // 01c538\n'
         '  if (ww_powerup(a)) { SprXXX_PowerUps_01C56F(k); return; }'),
        ('uint8 SolidSpriteBlock(uint8 k) {  // 01b457',
         'uint8 SolidSpriteBlock(uint8 k) {  // 01b457\n  if (ww_psycho_collision()) return 0;'),
    ])
    patch('src/smw_03.c', [
        ('#include "consts.h"', '#include "ww_finalboss.h"\n#include "ww_smw.h"\n#include "consts.h"'),
        ('void Spr0A0_ActivateBowserBattle_Sub(uint8 k) {  // 03a279',
         'void Spr0A0_ActivateBowserBattle_Sub(uint8 k) {  // 03a279\n  if (ww_final_step(k)) return;'),
        ('  UpdateMode7SpriteAnimations(k);\n  if (bowserbattle_var14b5)',
         '  UpdateMode7SpriteAnimations(k);\n  ww_final_tiles(k);\n  if (bowserbattle_var14b5)'),
        ('void Spr0A0_ActivateBowserBattle_DrawItemBox() {  // 03b44f',
         'void Spr0A0_ActivateBowserBattle_DrawItemBox() {  // 03b44f\n  if (ww_hide_native_hud()) return;'),
    ])
    patch('src/smw_05.c', [
        ('#include "consts.h"', '#include "ww_smw.h"\n#include "consts.h"'),
        ('  ProcessScrollSprites_Layer1();\n  ProcessScrollSprites_Layer2();',
         '  if (!ww_fast_scroll()) {\n    ProcessScrollSprites_Layer1();\n    ProcessScrollSprites_Layer2();\n  }'),
        ('  uint16 v7 = 2;\n  LOBYTE(v4) = stripe_image_upload + 4;',
         '  if (ww_enabled()) {\n'
         '    for (int i = 4; i < 14; i += 2) stripe_image_upload_data[stripe_image_upload + i] = 0xfc;\n'
         '    for (int i = 18; i < 44; i += 2) stripe_image_upload_data[stripe_image_upload + i] = 0xfc;\n'
         '  }\n'
         '  uint16 v7 = 2;\n  LOBYTE(v4) = stripe_image_upload + 4;'),
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
