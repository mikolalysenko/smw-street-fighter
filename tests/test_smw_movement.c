/* Exercise inputs through the actual SMW frame loop and level collision. */
#include "ww_smw.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static int tick,map_ticks,level_frames,phase=-1,phase_tick,home_x,home_y,peak;
static int moving,attack_seen,shot_seen,rise_seen,death_ticks,continued;
static int capture_walk,capture_air,capture_continue,capture_clear;
static const uint32_t attacks[]={0,2,1u<<8,1u<<9,(1u<<9)|(1u<<4),2,1u<<8,1u<<9,(1u<<9)|(1u<<4),1u<<11};
static const int moves[]={FC_IDLE,FC_JAB,FC_ROUNDHOUSE,FC_PROJECTILE,FC_RISING,FC_JAB,FC_ROUNDHOUSE,FC_PROJECTILE,FC_RISING,FC_SUPER_MOVE};
int ww_upstream_main(int,char**);
uint32_t ww_test_input(void) {
  ++tick;FcFight *f=(FcFight*)ww_fight_state();
  if(tick>12000 || continued>160) {
    SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;
  }
  if(phase==10) {
    ++death_ticks;
    if(ww_enabled() && player_current_state==0 && f->hp>0) {
      if(!continued) {assert(f->character==FC_RYU && f->hp==100);puts("Continue returned to the same fighter and level.");}
      ++continued;
      if(continued==50) {
        /* Goal tape fixture in the original loaded level: native contact
         * starts the actual time bonus and victory-pose sequence. */
        spr_spriteid[10]=0x7b;spr_current_status[10]=1;
        SetSprXYPos(10,player_xpos+12,player_ypos);
      }
      return continued<60?1u<<7:0;
    }
    return death_ticks==50?8:0;
  }
  if(misc_game_mode==0x0e) {
    ++map_ticks;return map_ticks<240?(map_ticks%30<20?1u<<7:0):map_ticks%40==0?1:0;
  }
  if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
  if(misc_game_mode==0x14 && !misc_intro_level_flag) {
    ++level_frames;
    if(!ww_enabled())return level_frames%30==0?8:0;
    if(level_frames<80)return 0;
    timer_player_hurt=80;
    for(int k=0;k<12;k++)spr_current_status[k]=0;
    if(phase<0) {
      if(player_in_air_flag)return 0;
      home_x=player_xpos;home_y=player_ypos;
      printf("Movement start: %d,%d\n",home_x,home_y);phase=0;phase_tick=0;
    }
    if(phase_tick==0) {
      fc_clear_actions(f);f->meter=300;
      player_xpos=home_x;player_ypos=home_y;player_xspeed=player_yspeed=0;
      player_in_air_flag=player_ducking_flag=0;player_facing_direction=1;
      peak=home_y;attack_seen=shot_seen=rise_seen=0;
    }
    if((int16)player_ypos<peak)peak=(int16)player_ypos;
    if(f->move==moves[phase])attack_seen=1;
    if(f->moving)moving=1;
    for(int k=0;k<FC_MAX_SHOTS;k++)if(f->shots[k].ttl && !f->body.grounded)shot_seen=1;
    if(f->move==FC_RISING && (int8)player_yspeed<=-70)rise_seen=1;
    if(phase_tick++==100) {
      printf("Jump case %d: height %d, attack %d, airborne shot %d, rising impulse %d\n",phase,home_y-peak,attack_seen,shot_seen,rise_seen);
      assert(home_y-peak>=20 && attack_seen);
      if(phase==3 || phase==7 || phase==9)assert(shot_seen);
      if(phase==4 || phase==8)assert(rise_seen);
      phase++;phase_tick=0;
      if(phase==10)DamagePlayer_Kill();
      return 0;
    }
    uint32_t b=phase_tick<19?1:0;
    if(phase_tick==(phase<5?1:9))b|=attacks[phase];
    if(phase_tick>65 && phase_tick<80)b|=1u<<7;
    return b;
  }
  return tick%45==0?8:tick%45==22?1:0;
}
static void save(uint8_t *pixels,size_t pitch,int w,int h,const char *name) {
  SDL_Surface *s=SDL_CreateRGBSurfaceFrom(pixels,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
  assert(s);assert(!SDL_SaveBMP(s,name));SDL_FreeSurface(s);
}
void ww_test_render(uint8_t *pixels,size_t pitch,int w,int h) {
  if(phase==0 && phase_tick==73 && !capture_walk++)save(pixels,pitch,w,h,"walk.bmp");
  if(phase==7 && phase_tick==18 && !capture_air++)save(pixels,pitch,w,h,"air-hadouken.bmp");
  if(death_ticks==40 && !capture_continue++)save(pixels,pitch,w,h,"continue.bmp");
  if(timer_end_level && continued>90 && !capture_clear++)save(pixels,pitch,w,h,"course-clear.bmp");
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);char *args[]={"test-movement","--config","test-play.ini",NULL};
  int result=ww_upstream_main(3,args);
  printf("Movement: phases %d, walk %d, continued %d, clear %d\n",phase,moving,continued,capture_clear);
  assert(phase==10 && moving && continued>160 && capture_clear);
  return result;
}
