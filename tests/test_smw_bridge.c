/* Link against the actual patched SMW objects. These tests exercise real RAM,
 * clipping, damage/death routines and sprite status transitions without assets.
 * They do not claim to boot a complete level or validate the PPU output. */
#include "ww_smw.h"
#include "audio.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool frame(uint32_t buttons) { return ww_before_frame(&buttons); }
static void enter(void) {
  memset(g_ram,0,0x20000);ww_init();misc_game_mode=0x14;
  player_xpos=100;player_ypos=100;player_facing_direction=1;
  assert(!frame(0));assert(!frame(8)); /* Actual select + confirm paths. */
  assert(frame(0));assert(ww_enabled());
}
static void enemy(int slot,int id) {
  spr_spriteid[slot]=(uint8)id;spr_current_status[slot]=8;
  SetSprXYPos((uint8)slot,118,114);
}
int main(void) {
  assert(SDL_Init(SDL_INIT_AUDIO)==0);
  enter();enemy(0,0x0f);enemy(1,0x80);enemy(2,0x35);enemy(3,0x2f);enemy(4,0x7b);
  frame(2);for(int i=0;i<5;i++)frame(0);
  assert(spr_current_status[0]==2); /* Goomba dies from the shared jab. */
  for(int i=1;i<5;i++)assert(spr_current_status[i]==8); /* Key, Yoshi, platform, goal. */
  assert(ww_fight_state()->meter==112);
  for(int i=0;i<30;i++)frame(0);
  enemy(0,0x0f);frame(2);for(int i=0;i<5;i++)frame(0);
  assert(spr_current_status[0]==2 && ww_fight_state()->meter==124); /* Recycled slot. */

  enter();enemy(0,0x0f);spr_current_slotid=0;
  DamagePlayer_Hurt();assert(player_current_state==0 && player_current_power_up==1);
  assert(ww_fight_state()->hp==88 && timer_player_hurt==60);
  DamagePlayer_Hurt();assert(ww_fight_state()->hp==88);
  for(int i=0;i<8;i++)frame(0);
  ww_velocity();assert((int8)player_xspeed<0 && (int8)player_yspeed<0);
  /* Entering a pipe must freeze combat without suppressing SMW's transition. */
  player_current_state=5;int tick=(int)ww_fight_state()->tick;
  assert(frame(1u<<11));assert((int)ww_fight_state()->tick==tick);
  player_current_state=0;
  DamagePlayer_Kill();assert(player_current_state==9 && ww_fight_state()->hp==0);
  assert(io_music_ch1==0); /* Original Mario death jingle is replaced. */
  misc_game_mode=0x0e;assert(frame(0));
  player_current_state=0;flag_sprites_locked=0;timer_player_hurt=0;
  misc_game_mode=0x14;assert(frame(0));
  assert(ww_fight_state()->hp==100 && ww_fight_state()->meter==100);
  assert(!frame(8));assert(!frame(0));assert(!frame(8)); /* Controller pause/resume. */
  assert(frame(0));
  fc_audio_shutdown();SDL_Quit();puts("SMW bridge: clipping, enemy whitelist, slot reuse, hurt, knockback, transitions, KO, respawn and pause passed.");
  return 0;
}
