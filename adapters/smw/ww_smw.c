#include "ww_smw.h"
#include "ww_targets.h"
#include "ww_title.h"
#include "ww_score.h"
#include "ww_hud.h"
#include "ww_finalboss.h"
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
static bool death_prompt,continue_pending,death_transition;
static int death_ticks,retry_ticks,clear_ticks;
static bool fighter_drawn;
static bool level_hurt,perfect_announced;
static bool tongue_pressed;
static bool psycho_was_active,psycho_safe;
static uint16 psycho_safe_x,psycho_safe_y;
static bool boss_practice;
static uint8 practice_map,practice_lives;
static uint32_t practice_score;
void WW_OriginalPlayerDraw(void);

void ww_init(void) {
  fc_init(&fight,FC_RYU);fight.meter=100;initialized=true;
  chosen=was_level=false;menu=choice=pause_choice=0;previous=0;
  death_prompt=continue_pending=death_transition=fighter_drawn=false;death_ticks=retry_ticks=clear_ticks=0;
  level_hurt=perfect_announced=tongue_pressed=false;
  psycho_was_active=psycho_safe=false;
  boss_practice=false;
  ww_targets_reset();ww_title_reset();ww_final_reset();ww_hud_reset();
  const char *sprites=getenv("WW_SPRITES");
  fc_assets_load(sprites?sprites:"sf2/sprites.dat");
  const char *audio=getenv("WW_AUDIO_DIR");
  fc_audio_init(audio?audio:"sf2/audio");
  fc_audio_selection(false);
}
const FcFight *ww_fight_state(void) { return &fight; }
bool ww_enabled(void) { return initialized && chosen && misc_game_mode==0x14; }
bool ww_practicing(void) { return boss_practice; }
static void start_akuma(void) {
  if(!boss_practice) {
    practice_map=ow_players_map[0];practice_lives=player_current_life_count;
    practice_score=ww_score()/10;boss_practice=true;
  }
  /* Use the original room loader, including Mode 7 graphics and Bowser's
   * entrance. It is also used when retrying this practice fight. */
  for(int i=0;i<32;i++)misc_subscreen_exit_entrance_number_lo[i]=0xc7;
  ow_players_map[0]=1;flag_use_secondary_entrance=flag_show_player_start=0;
  player_current_state=timer_end_level=0;counter_sublevels_entered=1;
  misc_game_mode=0x0f;menu=0;was_level=false;
  fc_clear_actions(&fight);fight.hp=fc_roster[fight.character].vitality;fight.meter=300;
  ww_targets_reset();ww_final_reset();
}
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
  tongue_pressed=false;
  if(!initialized)return true;
  ww_title_tick();
  uint32_t b=decode(*inputs),edge=b&~previous;previous=b;
  bool in_level=misc_game_mode==0x14;
  if(death_prompt) {
    ++death_ticks;
    if(death_ticks>30 && ((edge&(FC_JUMP|FC_START|FC_PUNCH|FC_BACK)) || death_ticks>=630)) {
      continue_pending=death_ticks<630 && !(edge&FC_BACK);
      death_prompt=false;death_transition=true;retry_ticks=0;
      /* Finish the native life/midpoint bookkeeping behind the curtain.
       * Never expose the old level after the arcade continue is accepted. */
      if(in_level && player_current_state==9)player_anim_timer=1;
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
      int delta=(edge&FC_LEFT)?FC_ROSTER_COUNT-1:(edge&FC_RIGHT)?1:(edge&FC_UP)?FC_ROSTER_COUNT-6:6;
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
    if(was_level){fc_clear_actions(&fight);ww_targets_reset();ww_final_reset();}
    was_level=false;menu=0;
    if(boss_practice && misc_game_mode==0x0e) {
      if(continue_pending){start_akuma();*inputs=0;return true;}
      boss_practice=false;ow_players_map[0]=practice_map;
      player_current_life_count=practice_lives;misc_1up_handler=0;
      player_mario_score_lo=practice_score;player_mario_score_mid=practice_score>>8;
      player_mario_score_hi=practice_score>>16;
    }
    if(death_transition && !continue_pending && misc_game_mode==0x0e)death_transition=false;
    if(!death_prompt && !death_transition)fc_audio_music(FC_MUSIC_NONE);
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
    continue_pending=death_transition=false;clear_ticks=0;fc_audio_music(FC_MUSIC_NONE);
    level_hurt=perfect_announced=false;
    psycho_was_active=psycho_safe=false;
    int meter=fight.hp>0?fight.meter:100;
    fc_init(&fight,choice);fight.meter=meter;
    if(boss_practice){ow_players_map[0]=practice_map;fight.meter=300;}
  }
  if(menu==2) {
    if(edge&(FC_UP|FC_DOWN))pause_choice=(pause_choice+((edge&FC_DOWN)?1:3))%4;
    if(edge&FC_BACK)menu=0;
    if(edge&(FC_START|FC_JUMP|FC_PUNCH)) {
      if(pause_choice==3)quit();else if(pause_choice==2)start_akuma();
      else menu=pause_choice==1?1:0;
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
  if(ww_final_cinematic()) {
    *inputs=0;fight.previous=b;timer_player_hurt=2;return true;
  }
  if(timer_end_level || misc_display_message || player_current_state!=0 || flag_pause || flag_sprites_locked) {
    if(timer_end_level && ++clear_ticks==60 && !level_hurt && !perfect_announced) {
      perfect_announced=true;fc_audio_play(FC_SND_PERFECT);
    }
    fight.previous=b;
    return true;
  }
  /* No cape/fire form; pickups and the underlying level logic still execute. */
  player_current_power_up=1;
  if((b&FC_GUARD) && (edge&FC_PUNCH) && player_current_item_box) {
    /* Select is now Back/Pause. Keep the visible reserve usable on a pad,
     * without letting the native full-slot fallback overwrite a boss. */
    for(int k=11;k>=0;k--)if(!spr_current_status[k]) { DropReservedItem();break; }
    b&=~FC_PUNCH;edge&=~FC_PUNCH;
  }
  FcTarget targets[WW_TARGET_COUNT];ww_targets_collect(&fight,targets);
  FcBody player=body();
  if(!fc_busy(&fight) && !!(b&FC_LEFT)!=!!(b&FC_RIGHT)) {
    player.facing=(b&FC_RIGHT)?1:-1;player_facing_direction=player.facing>0;
  }
  fc_tick(&fight,b,player,targets,WW_TARGET_COUNT);
  ww_combat_score(&fight);
  ww_targets_apply(&fight,targets);
  if(fc_psycho_flight(&fight) && ww_opponent()->active && !fight.flash && fight.move_tick>0)
    fight.move_tick=1199;
  ww_scenery_hits(&fight);
  fc_audio_context(&fight);
  fc_audio_play(fight.events);
  tongue_pressed=player_riding_yoshi_flag && (edge&FC_PUNCH);
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
static unsigned level_tile(int x,int y) {
  if(x<0 || y<0)return 0x25;
  unsigned offset;
  if(misc_level_layout_flags&1) {
    if(x>=512 || (y>>8)>=misc_screens_in_lvl || (y>>8)>=14)return 0x25;
    offset=GetLevelLayoutPtr_Vertical(y>>8)+(y&0xf0)+((x&255)>>4)+((x>>8)<<8);
  } else {
    if(y>=432 || (x>>8)>=misc_screens_in_lvl || (x>>8)>=32)return 0x25;
    offset=GetLevelLayoutPtr_Horizontal(x>>8)+(y&0xf0)+((x&255)>>4)+((y>>8)<<8);
  }
  if(offset>=0x10000)return 0x25;
  return g_ram[offset]+256*g_ram[offset+0x10000];
}
static bool floor_tile(unsigned t) { return t>=0x100 && t<0x1d8; }
static bool remember_landing(int x) {
  int bottom=(misc_level_layout_flags&1)?misc_screens_in_lvl*256:432;
  int top=(misc_level_layout_flags&1)?(int16)player_ypos-64:48;
  if(top<32)top=32;
  for(int y=(top+15)&~15;y<bottom;y+=16) {
    if(floor_tile(level_tile(x+8,y)) && !floor_tile(level_tile(x+8,y-16)) &&
       !floor_tile(level_tile(x+8,y-32))) {
      psycho_safe_x=(uint16)x;psycho_safe_y=(uint16)(y-32);psycho_safe=true;return true;
    }
  }
  return false;
}
static void psycho_land(void) {
  int x=player_xpos;
  for(int back=0;back<=160;back+=16)if(remember_landing(x-back))break;
  if(psycho_safe){player_xpos=psycho_safe_x;player_ypos=psycho_safe_y;}
  fc_clear_actions(&fight);fight.invuln=90;timer_player_hurt=90;
  player_xspeed=player_yspeed=0;player_in_air_flag=11;
  psycho_was_active=false;
}
bool ww_psycho_collision(void) {
  return ww_enabled() && fc_psycho_flight(&fight) && !fight.flash && !ww_opponent()->active && !timer_end_level;
}
bool ww_psycho_position(void) {
  if(!ww_psycho_collision()) {
    if(psycho_was_active && !timer_end_level && !player_current_state)psycho_land();
    return false;
  }
  bool vertical=misc_level_layout_flags&1;
  if(!psycho_was_active) {
    psycho_was_active=true;psycho_safe_x=player_xpos;psycho_safe_y=player_ypos;psycho_safe=true;
    remember_landing(player_xpos);player_ypos-=player_ypos>64?24:0;
  }
  /* The whole level is the punchline. Keep the engine's sprite loader and
   * camera advancing normally, so exits and events are never skipped. */
  fight.facing=1;player_facing_direction=1;player_xspeed=120;player_yspeed=0;
  int x=player_xpos,y=player_ypos;
  if(vertical) {
    y-=7;x+=(fight.previous&FC_RIGHT)?3:(fight.previous&FC_LEFT)?-3:0;
    if(x<16)x=16;if(x>480)x=480;
  } else {
    x+=7;y+=(fight.previous&FC_DOWN)?3:(fight.previous&FC_UP)?-3:0;
    if(y<40)y=40;if(y>336)y=336;
  }
  player_xpos=(uint16)x;player_ypos=(uint16)y;
  player_in_air_flag=11;player_blocked_flags=0;
  if(fight.move_tick%12==0)remember_landing(x);
  /* Stop on the actual goal trigger; normal SMW code handles the score,
   * exit type, overworld event and save prompt on the next sprite update. */
  for(int k=0;k<12;k++)if(spr_current_status[k]>=8 && spr_spriteid[k]==0x7b && !spr_table1602[k]) {
    int goal=spr_table00c2[k]+256*spr_table151c[k];
    if(x>=goal-32 && x<=goal+48) {
      psycho_land();player_xpos=(uint16)(goal+1);
      int bottom=spr_table1528[k]+256*(spr_table1534[k]&1);
      if(player_ypos>=bottom)player_ypos=(uint16)(bottom-32);
      return true;
    }
  }
  int end=misc_screens_in_lvl*256-48;
  if((vertical && y<=32) || (!vertical && x>=end) || (fight.previous&FC_GUARD)) {
    psycho_land();return true;
  }
  /* Destructible foreground blocks shatter, while doors, switches, pipe
   * entrances and the floor remain usable after the spectacle. */
  for(int dy=-16;dy<=16;dy+=16) {
    int bx=(x+24)&~15,by=(y+dy)&~15;unsigned tile=level_tile(bx,by);
    if(tile!=0x130 && tile!=0x132)continue;
    uint16 old_x=blocks_xpos,old_y=blocks_ypos,old_layer=misc_current_layer_being_processed;
    uint8 old_tile=blocks_map16_to_generate;
    blocks_xpos=bx;blocks_ypos=by;misc_current_layer_being_processed=0;blocks_map16_to_generate=2;
    GenerateTile();fc_explode(&fight,bx+8,by+8,true);
    blocks_xpos=old_x;blocks_ypos=old_y;misc_current_layer_being_processed=old_layer;blocks_map16_to_generate=old_tile;
  }
  return true;
}
bool ww_damage(void) {
  if(!ww_enabled())return false;
  if(player_current_state || timer_end_level || timer_star_power || timer_player_hurt)return true;
  int direction=-fight.facing;
  if(spr_current_slotid<12 && spr_current_status[spr_current_slotid])
    direction=GetSprXPos(spr_current_slotid)>player_xpos?1:-1;
  ww_take_damage(12,direction);
  return true;
}
bool ww_take_damage(int amount,int direction) {
  if(!ww_enabled() || player_current_state || timer_end_level || timer_star_power || timer_player_hurt)return false;
  fight.body=body();fight.events=0;
  bool hurt=fc_hurt(&fight,amount,direction);
  if(hurt) {
    level_hurt=true;
    timer_player_hurt=(uint8)fight.invuln;
    if(!fight.hp)DamagePlayer_Kill();
  }
  fc_audio_play(fight.events);fight.events=0;
  return hurt;
}
bool ww_perfect_clear(void) { return ww_enabled() && timer_end_level && !level_hurt; }
bool ww_yoshi_tongue_pressed(void) { return tongue_pressed; }
void ww_mushroom(void) {
  if(!ww_enabled() || fight.hp<=0)return;
  int maximum=fc_roster[fight.character].vitality;
  fight.hp+=25;if(fight.hp>maximum)fight.hp=maximum;
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
  if((death_prompt && death_ticks>=30) || death_transition){fc_continue(c,fight.character,death_ticks);return;}
  if(menu==1){fc_selection(c,choice,fight.tick,true);return;}
  if(misc_game_mode!=0x14) {
    return;
  }
  if(!chosen)return;
  ww_final_render(c);
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
  FcStageHud hud=ww_stage_hud();
  fc_battle_hud(c,&fight,ww_opponent(),&hud);
  if(timer_end_level && !misc_currently_active_boss_end_cutscene)fc_level_clear(c,fight.character,ww_perfect_clear());
  if(menu==2)fc_pause(c,pause_choice,true);
}
