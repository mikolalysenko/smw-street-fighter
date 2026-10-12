#include "ww_smw.h"
#include "ww_targets.h"
#include "ww_title.h"
#include "ww_score.h"
#include "ww_hud.h"
#include "ww_finalboss.h"
#include "ww_ending.h"
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
static bool debug_enabled;
static int menu,choice,pause_choice;
static uint32_t previous;
static bool death_prompt,continue_pending,death_transition,map_confirm_held;
static int death_ticks,clear_ticks;
static bool fighter_drawn;
static bool level_hurt,perfect_announced;
static bool tongue_pressed;
static bool psycho_was_active,psycho_safe;
static uint16 psycho_safe_x,psycho_safe_y;
static int psycho_exit_tick,psycho_exit_from_x,psycho_exit_from_y,psycho_exit_x,psycho_exit_y;
static bool boss_practice;
static uint8 practice_map,practice_lives;
static uint32_t practice_score;
void WW_OriginalPlayerDraw(void);

void ww_set_debug(bool enabled) { debug_enabled=enabled;pause_choice=0; }
bool ww_debug_enabled(void) { return debug_enabled; }
int ww_menu_state(void) { return menu; }

void ww_init(void) {
  fc_init(&fight,FC_RYU);fight.meter=100;initialized=true;
  chosen=was_level=false;menu=choice=pause_choice=0;previous=0;
  death_prompt=continue_pending=death_transition=map_confirm_held=fighter_drawn=false;death_ticks=clear_ticks=0;
  level_hurt=perfect_announced=tongue_pressed=false;
  psycho_was_active=psycho_safe=false;
  psycho_exit_tick=0;
  boss_practice=false;
  ww_targets_reset();ww_title_reset();ww_final_reset();ww_hud_reset();ww_ending_reset();
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
  ww_ending_reset();
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
  psycho_exit_tick=0;
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
    player_facing_direction?1:-1,!player_in_air_flag,player_swimming_flag!=0,player_climbing_flag!=0};
  return b;
}
static void quit(void) { SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e); }
bool ww_before_frame(uint32_t *inputs) {
  tongue_pressed=false;
  if(!initialized)return true;
  ww_title_tick();
  uint32_t b=decode(*inputs),edge=b&~previous;previous=b;
  if(ww_ending_tick(edge)){*inputs=0;return false;}
  bool in_level=misc_game_mode==0x14;
  if(chosen)player_current_item_box=players_item_box[0]=players_item_box[1]=0;
  if(death_prompt) {
    ++death_ticks;
    if(death_ticks>30 && ((edge&(FC_JUMP|FC_START|FC_PUNCH|FC_BACK)) || death_ticks>=630)) {
      continue_pending=death_ticks<630 && !(edge&FC_BACK);
      death_prompt=false;death_transition=true;
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
      bool air_move_used=in_level && fight.air_move_used;
      int meter=fight.meter;fc_init(&fight,choice);fight.hp=hp;fight.meter=meter;
      fight.air_move_used=air_move_used;
      fight.previous=b;chosen=true;menu=0;fc_audio_context(&fight);fc_audio_play(FC_SND_SELECT);
      fc_audio_music(FC_MUSIC_NONE);
      if(misc_game_mode==0x0e)map_confirm_held=true;
    }
    if(edge&FC_BACK){if(chosen){menu=in_level?2:0;fc_audio_music(FC_MUSIC_NONE);}else quit();}
    *inputs=0;return false;
  }
  if(!in_level) {
    if(was_level){fc_clear_actions(&fight);ww_targets_reset();ww_final_reset();}
    was_level=false;menu=0;
    if(boss_practice && misc_game_mode==0x0e) {
      boss_practice=false;ow_players_map[0]=practice_map;
      player_current_life_count=practice_lives;misc_1up_handler=0;
      player_mario_score_lo=practice_score;player_mario_score_mid=practice_score>>8;
      player_mario_score_hi=practice_score>>16;
    }
    if(death_transition && misc_game_mode==0x0e) {
      death_transition=false;
      if(continue_pending) {
        /* Native death has restored the overworld node. Choose a healthy
         * fighter while the map is covered, then let the player pick a level. */
        continue_pending=false;choice=fight.character;
        fc_init(&fight,choice);fight.meter=100;menu=1;
        fc_audio_music(FC_MUSIC_SELECT);*inputs=0;return false;
      }
    }
    if(!death_prompt && !death_transition)fc_audio_music(FC_MUSIC_NONE);
    if(death_transition)*inputs=0;
    /* A held selection-confirm button must not immediately enter a level. */
    if(map_confirm_held) {
      if(!(*inputs&0x0bu))map_confirm_held=false;
      *inputs&=~0x0bu;
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
    psycho_exit_tick=0;
    int meter=fight.hp>0?fight.meter:100;
    fc_init(&fight,choice);fight.meter=meter;
    if(boss_practice){ow_players_map[0]=practice_map;fight.meter=300;}
  }
  if(menu==2) {
    int count=debug_enabled?4:2;
    if(edge&(FC_UP|FC_DOWN))pause_choice=(pause_choice+((edge&FC_DOWN)?1:count-1))%count;
    if(edge&FC_BACK)menu=0;
    if(edge&(FC_START|FC_JUMP|FC_PUNCH)) {
      if(pause_choice==count-1)quit();else if(debug_enabled && pause_choice==2)start_akuma();
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
    *inputs=0;fight.previous=b;fight.events=0;timer_player_hurt=2;return true;
  }
  if(psycho_exit_tick) {
    fc_tick(&fight,0,body(),NULL,0);fight.previous=b;
    *inputs=0;return true;
  }
  if(player_current_state==0x0a) {
    /* Entrance scenes drive Mario themselves. Animate their real movement
     * without requiring a direction button from the player. */
    uint32_t walking=(int8)player_xspeed>0?FC_RIGHT:(int8)player_xspeed<0?FC_LEFT:0;
    fc_tick(&fight,walking,body(),NULL,0);return true;
  }
  if(timer_end_level || misc_display_message || player_current_state!=0 || flag_pause || flag_sprites_locked) {
    if(timer_end_level && ++clear_ticks==60 && !level_hurt && !perfect_announced) {
      perfect_announced=true;fc_audio_play(FC_SND_PERFECT);
    }
    fight.previous=b;
    return true;
  }
  /* Keep the terrain hull; power-up pickups now refill the super meter. */
  player_current_power_up=1;
  bool carrying=false;
  for(int k=0;k<12;k++)if(spr_current_status[k]==11 && spr_spriteid[k]!=0x7d)carrying=true;
  bool release=carrying && (b&FC_GUARD) && (edge&FC_PUNCH);
  if(release){b&=~FC_PUNCH;edge&=~FC_PUNCH;}
  ww_grapple_sync(&fight);
  FcTarget targets[WW_TARGET_COUNT];ww_targets_collect(&fight,targets);
  FcBody player=body();
  /* Boss arenas wrap positions above the screen through $ffff. Keep the
   * muzzle in the same signed world coordinates as the combat hull. */
  int feet=(int16)player_ypos+32;
  if(player_riding_yoshi_flag)feet+=(int8)player_relative_yposition_during_screen_shake-8;
  fight.sprite_y_offset=feet-player.box.y-player.box.h;
  if(!fc_busy(&fight) && !!(b&FC_LEFT)!=!!(b&FC_RIGHT)) {
    player.facing=(b&FC_RIGHT)?1:-1;player_facing_direction=player.facing>0;
  }
  fc_tick(&fight,b,player,targets,WW_TARGET_COUNT);
  ww_combat_score(&fight);
  ww_targets_apply(&fight,targets);
  ww_grapple_sync(&fight);
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
  if(fight.override_x || (fight.guarding && !release))*inputs&=~(0xc0u|2u);
  /* Auto-run used to drop keys whenever the player stopped or attacked.
   * Guard can grab an item, and carried items stay held until Guard+Punch. */
  if(carrying || (b&FC_GUARD))*inputs|=2u;
  if(release)*inputs&=~2u;
  if((fight.crouching_attack || (fight.guarding && fight.crouching_guard)) && player.grounded)*inputs|=1u<<5;
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
static bool teleport_clear(int x,int y) {
  /* Teleport travels six pixels per frame. Check its entire swept hull,
   * rather than waiting for SMW's next one-pixel wall depenetration. */
  for(int dx=2;dx<=13;dx+=11)for(int dy=7;dy<=31;dy+=8) {
    if(!(misc_level_layout_flags&0x40) && floor_tile(level_tile(x+dx,y+dy)))return false;
    if(misc_level_layout_flags&0x80) {
      int px=x+dx+(int16)misc_second_level_layer_xpos,py=y+dy+(int16)misc_second_level_layer_ypos;
      int screens=misc_screens_in_lvl;
      if(px<0 || py<0)continue;
      unsigned o;
      if(misc_level_layout_flags&2) {
        if(px>=512 || (py>>8)>=screens || (py>>8)>=14)continue;
        o=GetLevelLayoutPtr_Vertical((py>>8)+14)+(py&0xf0)+((px&255)>>4)+((px>>8)<<8);
      } else {
        if(py>=432 || (px>>8)>=screens || (px>>8)>=16)continue;
        o=GetLevelLayoutPtr_Horizontal((px>>8)+16)+(py&0xf0)+((px&255)>>4)+((py>>8)<<8);
      }
      if(o<0x10000 && floor_tile(g_ram[o]+256*g_ram[o+0x10000]))return false;
    }
  }
  return true;
}
static bool psycho_scrolling(void) { return l1_l2_scroll_spr_spriteid[0]!=0; }
bool ww_fast_scroll(void) {
  if(!fc_psycho_flight(&fight) || !ww_psycho_collision() || !psycho_scrolling() || flag_sprites_locked)return false;
  PointU16 origin=misc_layer1_pos[0];
  bool linked=l1_l2_scroll_spr_spriteid[1]==1;
  /* Advance the original path controller in small steps. Its stop points,
   * turns and room bounds stay authoritative; never skip tile uploads by
   * moving the viewport more than roughly Bison's seven pixels per frame. */
  for(int i=0;i<32;i++) {
    ProcessScrollSprites_Layer1();
    if(linked)ProcessScrollSprites_Layer2();
    if(abs((int16)(misc_layer1_pos[0].x-origin.x))>=7 ||
       abs((int16)(misc_layer1_pos[0].y-origin.y))>=7)break;
  }
  if(!linked)ProcessScrollSprites_Layer2();
  return true;
}
static bool psycho_visible(int x,int y) {
  return x>=(int)mirror_current_layer1_xpos+16 && x<=(int)mirror_current_layer1_xpos+216 &&
    y>=(int)mirror_current_layer1_ypos+32 && y<=(int)mirror_current_layer1_ypos+192;
}
static bool remember_landing(int x) {
  int bottom=(misc_level_layout_flags&1)?misc_screens_in_lvl*256:432;
  int top=(misc_level_layout_flags&1)?(int16)player_ypos-64:48;
  if(top<32)top=32;
  for(int y=(top+15)&~15;y<bottom;y+=16) {
    if(psycho_scrolling() && !psycho_visible(x,y-32))continue;
    bool clear=true;
    for(int dx=2;dx<=13;dx+=11) {
      unsigned ground=level_tile(x+dx,y);
      if(!floor_tile(ground) || ground==0x12f ||
          !((ground<0x159 || ground>=0x16a) || (misc_level_tileset_setting!=1 &&
            misc_level_tileset_setting!=5 && misc_level_tileset_setting!=13)))clear=false;
      for(int dy=16;dy<=32;dy+=16)if(floor_tile(level_tile(x+dx,y-dy)))clear=false;
    }
    if(clear) {
      psycho_safe_x=(uint16)x;psycho_safe_y=(uint16)(y-32);psycho_safe=true;return true;
    }
  }
  return false;
}
static bool psycho_land(void) {
  int x=player_xpos;
  /* A platform behind the forced camera is no longer a safe fallback. */
  if(psycho_scrolling())psycho_safe=false;
  for(int back=0;back<=160;back+=16)if(remember_landing(x-back))break;
  if(psycho_scrolling() && !psycho_safe) {
    for(int ahead=16;ahead<=160;ahead+=16)if(remember_landing(x+ahead))break;
    if(!psycho_safe) {
      /* Keep flying over a gap until a visible floor arrives. Never cancel
       * into a pit or rewind the camera/room to a stale entry platform. */
      fight.move=FC_SUPER_MOVE;fight.super_level=3;fight.move_tick=1198;
      return false;
    }
  }
  if(psycho_safe){player_xpos=psycho_safe_x;player_ypos=psycho_safe_y;}
  fc_clear_actions(&fight);fight.invuln=90;timer_player_hurt=90;
  player_xspeed=player_yspeed=0;player_in_air_flag=11;
  psycho_was_active=false;
  return true;
}
bool ww_psycho_collision(void) {
  return ww_enabled() && (psycho_exit_tick ||
    (fc_psycho_flight(&fight) && !fight.flash && !ww_opponent()->active && !timer_end_level));
}
bool ww_psycho_position(void) {
  if(psycho_exit_tick) {
    /* Leave the beam at the exact flight position, then jump to the goal's
     * floor. Cross the tape only on the last frame of the continuous arc. */
    float t=psycho_exit_tick/32.0f;
    player_xpos=(uint16)(psycho_exit_from_x+(psycho_exit_x-psycho_exit_from_x)*t);
    player_ypos=(uint16)(psycho_exit_from_y+(psycho_exit_y-psycho_exit_from_y)*t-96*t*(1-t));
    player_in_air_flag=1;player_blocked_flags=0;player_xspeed=player_yspeed=0;
    fight.override_x=fight.override_y=false;
    if(++psycho_exit_tick>32){psycho_exit_tick=0;player_in_air_flag=0;}
    return true;
  }
  if(!ww_psycho_collision()) {
    if(!psycho_was_active || timer_end_level || player_current_state || psycho_land())return false;
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
  if(psycho_scrolling()) {
    int cx=mirror_current_layer1_xpos,cy=mirror_current_layer1_ypos;
    if(x<cx+24)x=cx+24;if(x>cx+208)x=cx+208;
    if(y<cy+48)y=cy+48;if(y>cy+176)y=cy+176;
    /* A slow autoscroller can outlast the move's ordinary travel timeout. */
    if(fight.move_tick>1190)fight.move_tick=1190;
  }
  player_xpos=(uint16)x;player_ypos=(uint16)y;
  player_in_air_flag=11;player_blocked_flags=0;
  if(fight.move_tick%12==0)remember_landing(x);
  /* Stop on the actual goal trigger; normal SMW code handles the score,
   * exit type, overworld event and save prompt on the next sprite update. */
  for(int k=0;k<12;k++)if(spr_current_status[k]>=8 && spr_spriteid[k]==0x7b && !spr_table1602[k]) {
    int goal=spr_table00c2[k]+256*spr_table151c[k];
    if(x>=goal-32 && x<=goal+48) {
      int bottom=spr_table1528[k]+256*(spr_table1534[k]&1);
      psycho_safe=false;remember_landing(goal+1);
      psycho_exit_from_x=x;psycho_exit_from_y=y;psycho_exit_x=goal+1;
      psycho_exit_y=psycho_safe?psycho_safe_y:bottom-32;
      psycho_exit_tick=1;psycho_was_active=false;
      fc_clear_actions(&fight);fight.invuln=90;timer_player_hurt=90;
      player_xspeed=player_yspeed=0;player_in_air_flag=1;
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
  int previous_hp=fight.hp;
  bool hurt=fc_hurt(&fight,amount,direction);
  if(hurt) {
    if(fight.hp<previous_hp)level_hurt=true;
    timer_player_hurt=(uint8)fight.invuln;
    if(!fight.hp)DamagePlayer_Kill();
  }
  fc_audio_play(fight.events);fight.events=0;
  return hurt;
}
bool ww_perfect_clear(void) { return ww_enabled() && timer_end_level && !level_hurt; }
bool ww_yoshi_tongue_pressed(void) { return tongue_pressed; }
bool ww_powerup(uint8_t sprite) {
  if(!ww_enabled())return false;
  int bars=sprite==0x74?1:sprite==0x75?2:sprite==0x77?3:0;
  if(!bars)return false; /* Stars and 1UPs retain their native effects. */
  if(fight.hp>0)fc_meter(&fight,bars*100);
  player_current_item_box=players_item_box[0]=players_item_box[1]=0;
  player_current_power_up=1;
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
  if(fight.override_x) {
    int speed=(int)(fight.velocity_x*16);
    if(fight.move==FC_TELEPORT && speed) {
      int direction=speed>0?1:-1,allowed=0;
      for(int step=1;step<=(abs(speed)+15)/16;step++) {
        if(!teleport_clear((int16)player_xpos+direction*step,(int16)player_ypos))break;
        allowed=step;
      }
      if(allowed*16<abs(speed)){speed=direction*allowed*16;player_sub_xpos=0;}
    }
    /* Native terrain collision stops at a wall and depenetrates one pixel.
     * Reapplying a four-pixel dash through that stop embeds the whole hull
     * in a pipe, so the next frame's crush test kills the player. */
    if(!ww_psycho_collision() && ((speed>0 && (player_blocked_flags&1)) ||
        (speed<0 && (player_blocked_flags&2))))speed=0;
    player_xspeed=(uint8)(int8)speed;
  }
  if(fight.override_y) {
    if(fight.jump_boost && (player_blocked_flags&8)) {
      fight.jump_boost=0;fight.velocity_y=0;
    }
    player_yspeed=(uint8)(int8)(fight.velocity_y*16);
    if(fight.velocity_y<0)player_in_air_flag=11;
    fight.override_y=false;
  }
}
void ww_position_end(void) {
  if(!ww_enabled() || player_current_state || timer_end_level || menu ||
      ww_psycho_collision() || flag_side_exits)return;
  /* Fast rolls can cross SMW's one-pixel camera barrier in a single tick.
   * Clamp the position before its offscreen/crush check, including momentum
   * during recovery. This does not alter normal pit or hazard collisions. */
  int x=(int16)player_xpos,cx=mirror_current_layer1_xpos;
  int clamped=x<cx+8?cx+8:x>cx+232?cx+232:x;
  if(clamped!=x) {
    player_xpos=(uint16)clamped;player_xspeed=player_sub_xpos=0;
    player_on_screen_pos_x=clamped-cx;
  }
}
void ww_carried_position(uint8_t slot) {
  if(!ww_enabled() || player_current_state || spr_spriteid[slot]==0x7d)return;
  /* Hold the original object at the fighter's hands, outside the larger
   * torso overlay, so keys and shells remain visible while carried. */
  SetSprXPos(slot,player_xpos+(fight.facing>0?36:-36));
  SetSprYPos(slot,player_ypos+4);
}
bool ww_keyhole_contact(uint8_t slot) {
  if(!ww_enabled())return false;
  /* The carried key is drawn at the fighter's higher, wider hands. Keep
   * the original one-pixel keyhole usable across that visual offset. */
  for(int i=0;i<12;i++)if(spr_spriteid[i]==0x80 && spr_current_status[i]==11) {
    int dx=(int16)(GetSprXPos(i)-GetSprXPos(slot));
    int dy=(int16)(GetSprYPos(i)-GetSprYPos(slot));
    if(dx>=-20 && dx<=16 && dy>=-32 && dy<=16)return true;
  }
  return false;
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
  if(ww_ending_render(c))return;
  if(ww_title_render(c,ww_high_score()))return;
  if((death_prompt && death_ticks>=30) || death_transition){fc_continue(c,fight.character,death_ticks);return;}
  if(menu==1){fc_selection(c,choice,fight.tick,true);return;}
  if(misc_game_mode!=0x14) {
    return;
  }
  if(!chosen)return;
  ww_final_render(c);
  ww_grapple_render(c,false);
  int x=(int16)(player_xpos-mirror_current_layer1_xpos)+8;
  int feet=(int16)(player_ypos-mirror_current_layer1_ypos)+32;
  if(player_riding_yoshi_flag)feet+=(int8)player_relative_yposition_during_screen_shake-8;
  if(fighter_drawn) {
    FcFight drawing=fight;
    drawing.body.grounded=!player_in_air_flag;
    if(drawing.body.grounded && !fight.body.grounded && fight.air_tick>=4) {
      /* Physics can land after the combat tick. Show the landing this
       * frame instead of flashing an idle/air-strike pose for one frame. */
      drawing.landing_tick=6;
      if(drawing.air_attack && (drawing.move==FC_JAB || drawing.move==FC_ROUNDHOUSE))
        drawing.move=FC_IDLE;
    }
    if(timer_end_level) {
      drawing.move=FC_IDLE;drawing.stun=drawing.invuln=0;
      drawing.moving=player_xspeed!=0;drawing.walk_distance=clear_ticks*2;
      drawing.victory=flag_show_victory_pose_during_level_end!=0;
      drawing.move_tick=clear_ticks;
    }
    if(player_current_state==0x0a && (misc_level_tileset_setting==1 ||
        misc_level_tileset_setting==5 || misc_level_tileset_setting==13) &&
        (player_timer_pipe_warping==14 || player_timer_pipe_warping>=37)) {
      /* The original house foreground occludes Mario's OAM. The replacement
       * fighter must also disappear through its open doorway, not slide
       * over the wall to the right or draw his head across the lintel. */
      bool castle=misc_level_tileset_setting==1;
      int left=(uint8)(184-mirror_current_layer1_xpos),top=(uint8)((castle?80:96)-mirror_current_layer1_ypos);
      int dw=castle?48:32,dh=castle?64:48;
      FcCanvas outside=c;outside.w=left<c.w?left:c.w;
      fc_fighter(outside,&drawing,x,feet,1,0);
      if(left+dw<=c.w && top+dh<=c.h) {
        FcCanvas door={c.pixels+top*c.pitch+left,dw,dh,c.pitch};
        fc_fighter(door,&drawing,x-left,feet-top,1,0);
      }
    } else if(player_wall_walk_status && !player_in_air_flag) {
      int direction=player_wall_walk_status&1?1:-1;
      bool vertical=player_wall_walk_status>=6;
      fc_fighter_rotated(c,&drawing,x+(vertical?direction*8:0),feet-(vertical?16:0),-direction*(vertical?90:45));
    } else fc_fighter(c,&drawing,x,feet,1,0);
  }
  fc_effects(c,&fight,mirror_current_layer1_xpos,mirror_current_layer1_ypos);
  ww_grapple_render(c,true);
  FcStageHud hud=ww_stage_hud();
  fc_battle_hud(c,&fight,ww_opponent(),&hud);
  if(timer_end_level && !misc_currently_active_boss_end_cutscene)fc_level_clear(c,fight.character,ww_perfect_clear());
  if(menu==2)fc_pause(c,pause_choice,debug_enabled?4:2);
}
