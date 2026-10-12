/* Original level loop: sustained moves, projectile origins and Shinku beam. */
#include "ww_smw.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
static int tick,map_ticks,level_ticks,started,home_x,home_y,action,n,done,shots,beam,hits;
static int chars[]={FC_HONDA,FC_CHUNLI,FC_BLANKA,FC_HONDA,FC_RYU,FC_KEN,FC_CHUNLI,FC_RYU,FC_RYU,
  FC_DHALSIM,FC_DHALSIM,FC_DHALSIM,FC_DHALSIM};
int ww_upstream_main(int,char**);
uint32_t ww_test_input(void) {
  ++tick;FcFight *f=(FcFight*)ww_fight_state();
  if(done || tick>16000){SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;}
  if(misc_game_mode==0x0e){++map_ticks;return map_ticks<240?(map_ticks%30<20?128:0):map_ticks%40==0?1:0;}
  if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
  if(misc_game_mode!=0x14)return tick%45==0?8:tick%45==22?1:0;
  if(++level_ticks<80 || !ww_enabled())return 0;
  if(!started){if(player_in_air_flag)return 0;home_x=player_xpos;home_y=player_ypos;started=1;}
  if(!n) {
    fc_init(f,chars[action]);f->meter=action==8?300:200;
    player_xpos=home_x+80;player_ypos=home_y;player_xspeed=player_yspeed=0;
    player_in_air_flag=player_ducking_flag=player_blocked_flags=0;player_facing_direction=1;
    for(int k=0;k<12;k++)spr_current_status[k]=0;
    if(action==3){player_ypos-=60;player_in_air_flag=11;}
    if(action>=9) {
      player_facing_direction=action%2;
      if(action>=11){player_ypos-=60;player_in_air_flag=11;}
    }
    shots=beam=hits=0;
  }
  for(int k=0;k<12;k++)spr_current_status[k]=0;
  assert(f->hp>0 && player_current_state!=9);
  for(int i=0;i<FC_MAX_SHOTS;i++)if(f->shots[i].ttl){shots=1;beam|=f->shots[i].beam;}
  if(action<3 && n>35 && n<360)assert(f->move==(action==2?FC_ELECTRIC:FC_FLURRY));
  if(action==3 && f->move==FC_RUSH && f->move_tick>=8 && f->move_tick<=29) {
    if(!hits)hits=player_ypos;
    assert(player_ypos==hits); /* Host gravity cannot pull down active headbutt. */
  }
  if(n==(action<3?440:180)) {
    assert(f->move==FC_IDLE);
    if(action>=4 && action<=8)assert(shots);
    if(action==8)assert(beam);
    printf("%s scenario %d complete: shots=%d beam=%d\n",fc_roster[f->character].name,action,shots,beam);
    n=0;if(++action==(int)(sizeof(chars)/sizeof(chars[0])))done=1;return 0;
  }
  ++n;
  if(action<3 && n<360) return n%8==1?(action==0?2:action==1?256:32|512):action==2?32:0;
  return n==2?(action>=9?16|512:action>=7?2048:512):0;
}
void ww_test_render(uint8_t *pixels,size_t pitch,int w,int h) {
  if(!started || (n!=11 && n!=24 && n!=78 && n!=98 && n!=300))return;
  char name[64];snprintf(name,sizeof(name),"special-%02d-%03d.bmp",action,n);
  SDL_Surface *s=SDL_CreateRGBSurfaceFrom(pixels,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
  assert(s);assert(!SDL_SaveBMP(s,name));SDL_FreeSurface(s);
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);char *args[]={"test-specials","--config","test-play.ini",NULL};
  int result=ww_upstream_main(3,args);assert(done);puts("Sustained specials, Honda flight and projectile rendering passed.");return result;
}
