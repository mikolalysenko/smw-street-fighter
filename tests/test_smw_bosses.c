/* Real boss rooms, loaded through SMW's pipe transition and original assets.
 * Only test positioning/meter are assisted; attacks and defeat sequences are
 * the same ones used by the native game and the WASM build. */
#include "ww_smw.h"
#include "ww_targets.h"
#include "ww_finalboss.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int tick,map_ticks,base_frames,room_frames,level,warped,entered;
static int hadouken_hits,super_hits,ko_seen,end_seen,last_mode=-1,last_hp;
static int rise_hits,phases,peach_mushrooms,demon_shots;
static int patterns,attack_mask,last_serial,last_attack=-1,quiet_frames,downward_shots;
int ww_upstream_main(int argc,char **argv);
uint32_t ww_test_input(void) {
  ++tick;FcFight *f=(FcFight*)ww_fight_state();const FcOpponent *boss=ww_opponent();
  if(last_mode!=misc_game_mode) {
    printf("frame %d mode %02x boss %03x hp %d\n",tick,misc_game_mode,level,boss->hp);
    last_mode=misc_game_mode;
  }
  if(f->events&FC_SND_BOSS_KO)ko_seen++;
  if(entered && (timer_end_level || spra0_bowser_end_of_battle_flag))end_seen=1;
  if(tick>11000 || end_seen) {
    SDL_Event event={0};event.type=SDL_QUIT;SDL_PushEvent(&event);return 0;
  }
  if(!warped) {
    if(misc_game_mode==0x0e) {
      ++map_ticks;return map_ticks<240?(map_ticks%30<20?1u<<7:0):map_ticks%40==0?1:0;
    }
    if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
    if(misc_game_mode==0x14 && !misc_intro_level_flag && ++base_frames>45) {
      if(getenv("WW_TEST_PRACTICE")) {
        if(base_frames==100 || base_frames==106)return 8;
        if(base_frames==102 || base_frames==104)return 32;
        return 0;
      }
      for(int i=0;i<32;i++)misc_subscreen_exit_entrance_number_lo[i]=(uint8)level;
      ow_players_map[0]=(level>>8)!=0;flag_use_secondary_entrance=0;
      flag_show_player_start=0;player_current_state=0;timer_end_level=0;
      counter_sublevels_entered=1;misc_game_mode=0x0f;warped=1;
      return 0;
    }
    if(getenv("WW_TEST_PRACTICE") && ww_practicing()) {
      assert(misc_game_mode==0x0f && ww_fight_state()->meter==300);warped=1;return 0;
    }
    if(misc_game_mode==0x14)return 0;
    return tick%45==0?8:tick%45==22?1:0;
  }
  if(misc_game_mode!=0x14)return 0;
  ++room_frames;entered=1;
  if(level==0x1c7) {
    int phase=ww_final_phase();if(phase>=0)phases|=1<<phase;
    for(int k=0;k<12;k++)if(spr_current_status[k]>=8 && spr_spriteid[k]==0x74)peach_mushrooms=1;
    for(int k=0;k<12;k++){FcTarget shot;if(ww_final_target(k,&shot))demon_shots=1;}
    if(patterns && phase==WW_FINAL_FIGHT && ww_final_tick()<5600) {
      timer_star_power=timer_player_hurt=32;player_xpos=64;player_ypos=160;player_yspeed=0;
      int a=ww_final_attack(),at=ww_final_attack_tick();attack_mask|=1<<a;
      int newest=last_serial;
      for(int j=0;j<12;j++){FcTarget t;int id=ww_final_target(j,&t);if(id>newest)newest=id;}
      if((a==WW_AKUMA_RETREAT || a==WW_AKUMA_PAUSE || a==WW_AKUMA_RECOVER || a==WW_AKUMA_PHASE_SHIFT) && a==last_attack && at>1) {
        assert(newest==last_serial);quiet_frames++;
      }
      if(a==WW_AKUMA_AIR && newest>last_serial)downward_shots++;
      last_serial=newest;last_attack=a;
      int desired=ww_final_tick()<1800?72:ww_final_tick()<3600?48:24;
      if(boss->hp>desired && ww_final_vulnerable()) {
        int before=boss->hp;
        FcTarget t[WW_TARGET_COUNT];ww_targets_collect(f,t);
        for(int j=0;j<12;j++)if(ww_final_controls(j)) {t[j].hit=true;t[j].hp=desired;}
        ww_targets_apply(f,t);
        assert(before-boss->hp<=6 && boss->hp>=desired);
      }
      return 0;
    }
  }
  timer_star_power=32;timer_player_hurt=32;
  if(boss->active && last_hp>boss->hp) {
    if(f->move==FC_SUPER_MOVE)super_hits++;else if(f->move==FC_RISING)rise_hits++;else hadouken_hits++;
  }
  last_hp=boss->hp;
  if(room_frames%120==0)printf("room %d hp %d/%d mode %d locked %d state %d msg %d move %d\n",
    room_frames,boss->hp,boss->max_hp,misc_game_mode,flag_sprites_locked,player_current_state,misc_display_message,f->move);
  if(boss->active && boss->hp==0) {
    player_xpos=mirror_current_layer1_xpos+48;player_ypos=mirror_current_layer1_ypos+64;player_yspeed=0;player_in_air_flag=1;return 0;
  }
  if(level==0x1c7 && ww_final_phase()==WW_FINAL_FIGHT && ww_final_tick()<480) {
    player_xpos=24;player_ypos=160;player_yspeed=0;
    return 0; /* Let the actual projectile pattern and Peach's assistance run. */
  }
  if(room_frames>35) {
    FcTarget targets[WW_TARGET_COUNT];ww_targets_collect(f,targets);
    for(int k=0;k<12;k++)if(targets[k].active &&
        (spr_spriteid[k]==0x29 || spr_spriteid[k]==0xa0 || spr_spriteid[k]==0xa9 || spr_spriteid[k]==0xc5)) {
      FcBox b=targets[k].box;
      bool from_left=b.x-(int16)mirror_current_layer1_xpos>70;
      player_xpos=(uint16)(from_left?b.x-(rise_hits?36:16):b.x+b.w+(rise_hits?20:-6));
      player_ypos=(uint16)(b.y+(rise_hits?-18:12));
      player_xspeed=player_yspeed=0;player_in_air_flag=1;player_facing_direction=from_left;
      break;
    }
    if(room_frames%60==0 && !flag_sprites_locked && !player_current_state) {
      /* This fixture repositions next to each boss instead of traversing
       * its arena. Supply a landing between attacks as well; real airtime
       * limits are exercised separately with unassisted native physics. */
      if(f->move==FC_IDLE)player_in_air_flag=0;
      if(!rise_hits)return (1u<<9)|(1u<<4);
      if(hadouken_hits<2)return 1u<<9;
      f->meter=300;return 1u<<11;
    }
  }
  return 0;
}
void ww_test_render(uint8_t *pixels,size_t pitch,int w,int h) {
  if(patterns && ww_final_phase()==WW_FINAL_FIGHT && (ww_final_attack_tick()==20 || ww_final_attack_tick()==40 || ww_final_attack_tick()==55)) {
    char name[80];snprintf(name,sizeof(name),"akuma-stage-%d-wave-%d-%d.bmp",ww_final_stage(),ww_final_attack(),ww_final_attack_tick());
    SDL_Surface *s=SDL_CreateRGBSurfaceFrom(pixels,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
    assert(s);assert(!SDL_SaveBMP(s,name));SDL_FreeSurface(s);
  }
  if(level==0x1c7 && ((ww_final_phase()==WW_FINAL_INTRO && room_frames==700) ||
      (ww_final_phase()>WW_FINAL_INTRO && ww_final_tick()==30) ||
      (ww_final_phase()==WW_FINAL_DEMON && ww_final_tick()==95) ||
      (ww_final_phase()==WW_FINAL_FIGHT && ww_final_tick()==420))) {
    char name[80];snprintf(name,sizeof(name),"akuma-%d-%d.bmp",ww_final_phase(),ww_final_tick());
    SDL_Surface *s=SDL_CreateRGBSurfaceFrom(pixels,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
    assert(s);assert(!SDL_SaveBMP(s,name));SDL_FreeSurface(s);
  }
  if(room_frames==170 || (ww_opponent()->ko_timer==180 && ko_seen==0)) {
    char name[80];snprintf(name,sizeof(name),"boss-%03x-%s.bmp",level,room_frames==170?"fight":"ko");
    SDL_Surface *surface=SDL_CreateRGBSurfaceFrom(pixels,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
    assert(surface);assert(!SDL_SaveBMP(surface,name));SDL_FreeSurface(surface);
  }
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);
  level=(int)strtol(getenv("WW_TEST_BOSS")?getenv("WW_TEST_BOSS"):"1f6",NULL,16);
  patterns=getenv("WW_TEST_PATTERNS")!=NULL;
  char *args[]={"test-smw-bosses","--config","test-play.ini",NULL};
  int result=ww_upstream_main(3,args);
  printf("Boss %03x: uppercut hits %d, hadouken hits %d, super hits %d, KO %d, native ending %d\n",level,rise_hits,hadouken_hits,super_hits,ko_seen,end_seen);
  assert(entered && rise_hits && hadouken_hits && super_hits && ko_seen==1 && end_seen);
  if(level==0x1c7)assert((phases&31)==31 && peach_mushrooms && demon_shots);
  if(patterns){printf("Akuma waves %x, quiet frames %d, air volleys %d\n",attack_mask,quiet_frames,downward_shots);assert((attack_mask&2047)==2047 && quiet_frames>1800 && downward_shots>2);}
  return result;
}
