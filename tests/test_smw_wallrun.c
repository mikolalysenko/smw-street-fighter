#include "ww_smw.h"
#include "assets.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
static int tick,map_ticks,base,ready,n,which,walk,sloped,done;
int ww_upstream_main(int,char**);
uint32_t ww_test_input(void) {
  ++tick;FcFight *f=(FcFight*)ww_fight_state();
  if(done || tick>6000){SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;}
  if(misc_game_mode==0x0e){++map_ticks;return map_ticks<240?(map_ticks%30<20?128:0):map_ticks%40==0?1:0;}
  if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
  if(misc_game_mode!=0x14)return tick%45==0?8:tick%45==22?1:0;
  if(++base<80 || !ww_enabled())return 0;
  int left=which&1;
  if(!n) {
    fc_init(f,which/2?FC_HONDA:FC_RYU);
    for(int i=0;i<12;i++)spr_current_status[i]=0;
    for(int x=0;x<512;x+=16)for(int y=0;y<432;y+=16) {
      unsigned o=GetLevelLayoutPtr_Horizontal(x>>8)+(y&0xf0)+((x&255)>>4)+((y>>8)<<8);
      int id=y>=208?0x100:0x25;
      if(y>=48 && y<208 && (left?x<112:x>=128))id=0x100;
      if(x==112 && y==192)id=left?0x1b5:0x1b4;
      if(x==112 && y==208)id=0x1eb;
      g_ram[o]=id;g_ram[o+0x10000]=id>>8;
    }
    player_xpos=left?200:40;player_ypos=176;player_xspeed=player_yspeed=player_sub_xpos=player_sub_ypos=0;
    player_in_air_flag=player_wall_walk_status=player_blocked_flags=player_ducking_flag=0;
    player_facing_direction=!left;
    mirror_current_layer1_xpos=misc_layer1_pos[0].x=mirror_current_layer1_ypos=misc_layer1_pos[0].y=0;
    player_on_screen_pos_x=player_xpos;player_on_screen_pos_y=player_ypos;
    flag_layer1_horiz_scroll_level_setting=flag_layer1_vert_scroll_level_setting=0;
    walk=sloped=0;ready=1;
  }
  ++n;assert(player_current_state!=9 && f->hp>0);
  if(player_wall_walk_status>0 && player_wall_walk_status<6)sloped++;
  if(player_wall_walk_status>=6 && f->moving && fc_animation(f).pose==FC_POSE_WALK)walk++;
  if(walk>=24) {
    assert(sloped && player_ypos<120);
    printf("Wall run %s facing %d: triangle=%d, walking=%d, y=%d\n",fc_roster[f->character].name,!left,sloped,walk,player_ypos);
    if(++which==4)done=1;n=0;return 0;
  }
  if(n>240){fprintf(stderr,"Wall run stalled state=%d pos=%d,%d speed=%d,%d slope=%d\n",player_wall_walk_status,player_xpos,player_ypos,(int8)player_xspeed,(int8)player_yspeed,sloped);assert(0);}
  return left?64:128;
}
void ww_test_render(uint8_t *p,size_t pitch,int w,int h) {
  if(!ready || (walk!=12 && sloped!=1))return;
  char path[64];snprintf(path,sizeof(path),"wallrun-%d-%d.bmp",which,player_wall_walk_status);
  SDL_Surface *s=SDL_CreateRGBSurfaceFrom(p,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
  assert(s);assert(!SDL_SaveBMP(s,path));SDL_FreeSurface(s);
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);char *args[]={"test-wallrun","--config","test-play.ini",NULL};
  int r=ww_upstream_main(3,args);assert(done);return r;
}
