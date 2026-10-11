#include "ww_smw.h"
#include "ww_targets.h"
#include "ww_title.h"
#include "ww_score.h"
#include "fight.h"
#include "draw.h"
#include "audio.h"
#include "assets.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <stdlib.h>
#include <string.h>

static FcFight fight;
static bool initialized,chosen,was_level;
static int menu,choice,pause_choice;
static uint32_t previous;
static bool death_prompt,continue_pending;
static int death_ticks,retry_ticks,clear_ticks;
static bool fighter_drawn;
void WW_OriginalPlayerDraw(void);

void ww_init(void) {
  fc_init(&fight,FC_RYU);fight.meter=100;initialized=true;
  chosen=was_level=false;menu=choice=pause_choice=0;previous=0;
  death_prompt=continue_pending=fighter_drawn=false;death_ticks=retry_ticks=clear_ticks=0;
  ww_targets_reset();ww_title_reset();
  const char *sprites=getenv("WW_SPRITES");
  fc_assets_load(sprites?sprites:"sf2/sprites.dat");
  const char *audio=getenv("WW_AUDIO_DIR");
  fc_audio_init(audio?audio:"sf2/audio");
  fc_audio_selection(false);
}
const FcFight *ww_fight_state(void) { return &fight; }
bool ww_enabled(void) { return initialized && chosen && misc_game_mode==0x14; }
bool ww_hide_native_hud(void) {
  return initialized && chosen && misc_game_mode>=0x0f && misc_game_mode<=0x17;
}
void ww_begin_frame(void) {
  /* Message boxes/native pause reuse the previous frame's OAM. */
  if(misc_game_mode!=0x14 || !(misc_display_message || flag_pause))fighter_drawn=false;
}
static uint32_t decode(uint32_t b) {
  return ((b&(1u<<6))?FC_LEFT:0)|((b&(1u<<7))?FC_RIGHT:0)|
    ((b&(1u<<4))?FC_UP:0)|((b&(1u<<5))?FC_DOWN:0)|
    ((b&1)?FC_JUMP:0)|((b&2)?FC_PUNCH:0)|((b&(1u<<8))?FC_KICK:0)|
    ((b&(1u<<9))?FC_SPECIAL:0)|((b&(1u<<11))?FC_SUPER:0)|
    ((b&(1u<<10))?FC_GUARD:0)|((b&8)?FC_START:0)|((b&4)?FC_BACK:0);
}
static FcBody body(void) {
  /* Keep SMW's compact terrain collision hull; the combat sprite is larger.
   * Using the engine's clipping function also follows ducking and Yoshi. */
  CollInfo ci={0};StandardSpriteToSpriteCollisionChecks_GetMarioClipping(&ci);
  FcBody b={{(float)(int16)(ci.r0|(ci.r8<<8)),(float)(int16)(ci.r1|(ci.r9<<8)),ci.r2,ci.r3},
    player_facing_direction?1:-1,!player_in_air_flag};
  return b;
}
static void quit(void) { SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e); }
bool ww_before_frame(uint32_t *inputs) {
  if(!initialized)return true;
  ww_title_tick();
  uint32_t b=decode(*inputs),edge=b&~previous;previous=b;
  bool in_level=misc_game_mode==0x14;
  if(death_prompt) {
    ++death_ticks;
    if(death_ticks>30 && ((edge&(FC_JUMP|FC_START|FC_PUNCH|FC_BACK)) || death_ticks>=630)) {
      continue_pending=death_ticks<630 && !(edge&FC_BACK);
      death_prompt=false;retry_ticks=0;fc_audio_music(FC_MUSIC_NONE);
      fc_audio_play(FC_SND_SELECT);
    }
    *inputs=0;b=0;edge=0;
    if(death_prompt && !in_level)return false;
  }
  /* The original file menu finishes in mode 0b for both new and saved games.
   * Freeze there so the fighter is chosen before the map/first level loads. */
  if(!chosen && menu!=1 && (misc_game_mode==0x0b || misc_game_mode==0x0e || in_level)) {
    menu=1;edge=0;
  }
  if(menu==1) {
    fc_audio_music(FC_MUSIC_SELECT);++fight.tick;
    if(edge&(FC_LEFT|FC_RIGHT|FC_UP|FC_DOWN)) {
      int delta=(edge&FC_LEFT)?3:(edge&FC_RIGHT)?1:2;
      choice=(choice+delta)%FC_ROSTER_COUNT;fc_audio_play(FC_SND_SELECT);
    }
    if(edge&(FC_JUMP|FC_START|FC_PUNCH)) {
      int hp=fight.hp*fc_roster[choice].vitality/fc_roster[fight.character].vitality;
      int meter=fight.meter;fc_init(&fight,choice);fight.hp=hp;fight.meter=meter;
      fight.previous=b;chosen=true;menu=0;fc_audio_context(&fight);fc_audio_play(FC_SND_SELECT);
      fc_audio_music(FC_MUSIC_NONE);
    }
    if(edge&FC_BACK){if(chosen){menu=2;fc_audio_music(FC_MUSIC_NONE);}else quit();}
    *inputs=0;return false;
  }
  if(!in_level) {
    if(was_level){fc_clear_actions(&fight);ww_targets_reset();}
    was_level=false;menu=0;
    if(!death_prompt)fc_audio_music(FC_MUSIC_NONE);
    if(continue_pending) {
      /* Re-enter the same overworld node through the original loader. It
       * restores the midpoint, level objects and exits just like a retry. */
      *inputs=misc_game_mode==0x0e && ++retry_ticks%30==0?1:0;
      return true;
    }
    /* The crossover is one player; keep the native save-file flow. */
    if(misc_game_mode==0x0a){misc_blinking_cursor_pos=0;*inputs&=~0xf0u;}
    if((b&(FC_START|FC_BACK))==(FC_START|FC_BACK))quit();
    return true;
  }
  if(!was_level) {
    was_level=true;
    continue_pending=false;clear_ticks=0;
    int meter=fight.hp>0?fight.meter:100;
    fc_init(&fight,choice);fight.meter=meter;
  }
  if(menu==2) {
    if(edge&(FC_UP|FC_DOWN))pause_choice=(pause_choice+((edge&FC_DOWN)?1:2))%3;
    if(edge&FC_BACK)menu=0;
    if(edge&(FC_START|FC_JUMP|FC_PUNCH)) {
      if(pause_choice==2)quit();else menu=pause_choice==1?1:0;
    }
    fight.previous=b;return false;
  }
  if((edge&(FC_START|FC_BACK)) && player_current_state==0 && !timer_end_level &&
     !misc_intro_level_flag && !misc_display_message && !flag_sprites_locked) {
    menu=2;pause_choice=0;return false;
  }
  if(!fight.hp && player_current_state==9) {
    ++fight.tick;++fight.ko_timer;
    return true;
  }
  if(timer_end_level || misc_display_message || player_current_state!=0 || flag_pause || flag_sprites_locked) {
    if(timer_end_level)++clear_ticks;
    fight.previous=b;
    return true;
  }
  /* No cape/fire form; pickups and the underlying level logic still execute. */
  player_current_power_up=1;
  FcTarget targets[WW_TARGET_COUNT];ww_targets_collect(&fight,targets);
  FcBody player=body();
  if(!fc_busy(&fight) && !!(b&FC_LEFT)!=!!(b&FC_RIGHT)) {
    player.facing=(b&FC_RIGHT)?1:-1;player_facing_direction=player.facing>0;
  }
  fc_tick(&fight,b,player,targets,WW_TARGET_COUNT);
  ww_targets_apply(&fight,targets);
  ww_scenery_hits(&fight);
  fc_audio_context(&fight);
  fc_audio_play(fight.events);
  /* Reserve combat buttons; keep B jump and all directions. Automatic run
   * preserves SMW's long jumps without a second held face button. */
  *inputs&=~(2u|4u|8u|(1u<<8)|(1u<<9)|(1u<<10)|(1u<<11));
  if(b&(FC_LEFT|FC_RIGHT))*inputs|=2;
  if(fight.override_x || fight.guarding)*inputs&=~(0xc0u|2u);
  if(fight.crouching_attack && player.grounded)*inputs|=1u<<5;
  /* Held jump still controls SMW's variable jump arc during air attacks. */
  if(fight.stun)*inputs&=~1u;
  return !fight.frozen;
}
bool ww_damage(void) {
  if(!ww_enabled())return false;
  if(player_current_state || timer_end_level || timer_star_power || timer_player_hurt)return true;
  int direction=-fight.facing;
  if(spr_current_slotid<12 && spr_current_status[spr_current_slotid])
    direction=GetSprXPos(spr_current_slotid)>player_xpos?1:-1;
  fight.body=body();fight.events=0;
  if(fc_hurt(&fight,12,direction)) {
    timer_player_hurt=(uint8)fight.invuln;
    if(!fight.hp)DamagePlayer_Kill();
  }
  fc_audio_play(fight.events);fight.events=0;
  return true;
}
void ww_on_death(void) {
  if(!ww_enabled())return;
  fight.body=body();fc_ko(&fight);fc_audio_play(fight.events);fight.events=0;
  death_prompt=true;death_ticks=0;fc_audio_music(FC_MUSIC_CONTINUE);
  /* Arcade continues are unlimited. Keep vanilla game-over bookkeeping
   * from discarding the current node before the player makes a choice. */
  if((int8)player_current_life_count<1)player_current_life_count=1;
}
void ww_velocity(void) {
  if(!ww_enabled() || player_current_state || timer_end_level || menu)return;
  if(fight.override_x)player_xspeed=(uint8)(int8)(fight.velocity_x*16);
  if(fight.override_y) {
    player_yspeed=(uint8)(int8)(fight.velocity_y*16);
    if(fight.velocity_y<0)player_in_air_flag=11;
    fight.override_y=false;
  }
}
void ww_player_draw(void) {
  if(!ww_enabled()){WW_OriginalPlayerDraw();return;}
  /* PlayerDraw also draws Yoshi. Keep that OAM before replacing Mario.
   * Platforms call this early, then set hide=ff to skip a second draw. */
  if(player_hide_player_tile_flags!=0xff) {
    fighter_drawn=true;
    PlayerGFXRt_01EA70();
  }
  uint8 old_oam[512],old_sizes[128];
  memcpy(old_oam,oam_buf,sizeof(old_oam));memcpy(old_sizes,sprites_oamtile_size_buffer,sizeof(old_sizes));
  WW_OriginalPlayerDraw();
  memcpy(oam_buf,old_oam,sizeof(old_oam));memcpy(sprites_oamtile_size_buffer,old_sizes,sizeof(old_sizes));
}
void ww_render(uint8_t *pixels,size_t pitch,int width,int height) {
  if(!initialized)return;
  FcCanvas c={(uint32_t*)pixels,width,height,(int)(pitch/4)};
  if(ww_title_render(c,ww_high_score()))return;
  if(death_prompt && death_ticks>=30){fc_continue(c,fight.character,death_ticks);return;}
  if(menu==1){fc_selection(c,choice,fight.tick,true);return;}
  if(misc_game_mode!=0x14) {
    return;
  }
  if(!chosen)return;
  int x=(int16)(player_xpos-mirror_current_layer1_xpos)+8;
  int feet=(int16)(player_ypos-mirror_current_layer1_ypos)+32;
  if(player_riding_yoshi_flag)feet+=(int8)player_relative_yposition_during_screen_shake-8;
  if(fighter_drawn) {
    FcFight drawing=fight;
    drawing.body.grounded=!player_in_air_flag;
    if(timer_end_level) {
      drawing.move=FC_IDLE;drawing.stun=drawing.invuln=0;
      drawing.moving=player_xspeed!=0;drawing.walk_distance=clear_ticks*2;
      drawing.victory=flag_show_victory_pose_during_level_end!=0;
      drawing.move_tick=clear_ticks;
    }
    fc_fighter(c,&drawing,x,feet,1,0);
  }
  fc_effects(c,&fight,mirror_current_layer1_xpos,mirror_current_layer1_ypos);
  fc_battle_hud(c,&fight,ww_opponent());
  if(fight.hp>0 && !ww_opponent()->ko_timer)
    fc_score_hud(c,ww_score(),ww_high_score(),ww_opponent()->active);
  if(timer_end_level && !misc_currently_active_boss_end_cutscene)fc_level_clear(c,fight.character);
  if(menu==2)fc_pause(c,pause_choice);
}
