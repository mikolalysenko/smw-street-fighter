/* Grab the original Mode 7 clown car and destroy Akuma only on landing. */
#include "ww_smw.h"
#include "ww_targets.h"
#include "ww_finalboss.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
static int tick,map_ticks,base,warped,started,n,held,slam,done,hp,ground_y;
int ww_upstream_main(int,char**);
uint32_t ww_test_input(void) {
  ++tick;FcFight *f=(FcFight*)ww_fight_state();
  if(done || tick>8000){SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;}
  if(!warped) {
    if(misc_game_mode==0x0e){++map_ticks;return map_ticks<240?(map_ticks%30<20?128:0):map_ticks%40==0?1:0;}
    if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
    if(misc_game_mode==0x14 && !misc_intro_level_flag && ++base>45) {
      for(int i=0;i<32;i++)misc_subscreen_exit_entrance_number_lo[i]=0xc7;
      ow_players_map[0]=1;flag_use_secondary_entrance=flag_show_player_start=0;
      player_current_state=timer_end_level=0;counter_sublevels_entered=1;misc_game_mode=0x0f;warped=1;return 0;
    }
    return misc_game_mode==0x14?0:tick%45==0?8:tick%45==22?1:0;
  }
  if(misc_game_mode!=0x14)return 0;
  timer_star_power=timer_player_hurt=32;
  if(!player_in_air_flag)ground_y=player_ypos;
  if(!started && ww_final_phase()!=WW_FINAL_FIGHT)return 0;
  if(!started && ww_final_tick()<200)return 0;
  if(!started) {
    fc_init(f,FC_ZANGIEF);f->meter=300;hp=ww_opponent()->hp;
    for(int k=0;k<12;k++)if(ww_final_controls(k))player_xpos=GetSprXPos(k);
    player_ypos=ground_y;player_xspeed=player_yspeed=0;player_in_air_flag=0;
    started=1;printf("Akuma grab start player=%d,%d hp=%d\n",player_xpos,player_ypos,hp);
    return 2048;
  }
  ++n;assert(f->hp>0 && player_current_state!=9);
  if(f->grabbed>=0) {
    ++held;assert(ww_final_controls(f->grabbed));assert(ww_opponent()->hp==hp);
    assert(!f->grab_slam);
  }
  /* Entering KO clears transient combat flags in the same frame. Observe
   * the landing and boss health transition, not the cleared grab_slam. */
  if(!slam && ww_opponent()->hp==0) {
    slam=1;assert(held>15);assert(ww_opponent()->hp==0);
    assert(!player_in_air_flag && player_ypos==ground_y);
    assert(ww_final_phase()==WW_FINAL_KO);
  }
  if(n>120){assert(held && slam && ww_opponent()->hp==0);done=1;}
  return 0;
}
void ww_test_render(uint8_t *p,size_t pitch,int w,int h) {
  if(!started || (held!=4 && held!=14 && held!=24 && !(slam && n==90)))return;
  char path[64];snprintf(path,sizeof(path),"akuma-grapple-%03d.bmp",n);
  SDL_Surface *s=SDL_CreateRGBSurfaceFrom(p,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
  assert(s);assert(!SDL_SaveBMP(s,path));SDL_FreeSurface(s);
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);char *args[]={"test-final-grapple","--config","test-play.ini",NULL};
  int r=ww_upstream_main(3,args);assert(done);printf("Akuma: held %d frames without damage, destroyed on slam, original defeat sequence started.\n",held);return r;
}
