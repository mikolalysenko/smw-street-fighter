/* Native carry/spin/slam: enemies, an essential key, pipes and solid blocks. */
#include "ww_smw.h"
#include "ww_targets.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
static int tick,map_ticks,base,started,n,which,grabbed,air,slam,done,home_x,home_y;
static const int ids[]={0x0f,0x91,0x9f,0x80,0x137,0x130,0x11f};
static int pipe_x,pipe_y;
static unsigned tile_offset(int x,int y) {
  return GetLevelLayoutPtr_Horizontal(x>>8)+(y&0xf0)+((x&255)>>4)+((y>>8)<<8);
}
int ww_upstream_main(int,char**);
uint32_t ww_test_input(void) {
  ++tick;FcFight *f=(FcFight*)ww_fight_state();
  if(done || tick>8000){SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;}
  if(misc_game_mode==0x0e){++map_ticks;return map_ticks<240?(map_ticks%30<20?128:0):map_ticks%40==0?1:0;}
  if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
  if(misc_game_mode!=0x14)return tick%45==0?8:tick%45==22?1:0;
  if(++base<80 || !ww_enabled())return 0;
  if(!started){if(player_in_air_flag)return 0;home_x=player_xpos+64;home_y=player_ypos;started=1;}
  if(!n) {
    fc_init(f,FC_ZANGIEF);ww_targets_reset();
    player_xpos=home_x;player_ypos=home_y;player_xspeed=player_yspeed=0;
    player_in_air_flag=player_ducking_flag=player_blocked_flags=0;player_facing_direction=1;
    for(int i=0;i<12;i++)spr_current_status[i]=0;
    if(which<4) {
      spr_spriteid[10]=ids[which];spr_current_status[10]=8;InitializeNormalSpriteRAMTables(10);
      SetSprXYPos(10,home_x+24,home_y+(which==2?-24:8));
      spr_xspeed[10]=spr_yspeed[10]=0;
    } else {
      pipe_x=(home_x+32)&~15;pipe_y=home_y&~15;
      int size=which==4?32:16;
      for(int y=0;y<size;y+=16)for(int x=0;x<size;x+=16) {
        unsigned o=tile_offset(pipe_x+x,pipe_y+y),id=which==4?(y?0x135:0x137)+(x!=0):ids[which];
        g_ram[o]=id;g_ram[o+0x10000]=id>>8;
      }
    }
    timer_player_hurt=30;
    grabbed=air=slam=0;
  }
  assert(f->hp>0 && player_current_state!=9);
  if(f->grabbed==(which>=4?WW_PIPE_TARGET:10)) {
    grabbed++;air|=player_in_air_flag!=0;
    assert((which>=4 || spr_current_status[10]>=8) && !f->grab_slam);
  }
  if(f->grab_slam)slam=1;
  if(which<4 && !spr_current_status[10])assert(slam); /* Never destroyed on contact. */
  if(which>=4) {
    unsigned o=tile_offset(pipe_x,pipe_y);
    assert((g_ram[o]+256*g_ram[o+0x10000])==(slam?0x25:ids[which]));
  }
  if(n==150) {
    printf("Piledriver %02x: held=%d airborne=%d slam=%d status=%d\n",ids[which],grabbed,air,slam,spr_current_status[10]);
    assert(grabbed>15 && air && slam);
    if(which<4)assert(!spr_current_status[10]);
    n=0;if(++which==sizeof(ids)/sizeof(ids[0]))done=1;return 0;
  }
  ++n;return n==2?16|512:0;
}
void ww_test_render(uint8_t *pixels,size_t pitch,int w,int h) {
  if(!started || (n!=3 && n!=14 && n!=24 && n!=34 && n!=44 && n!=65))return;
  char name[64];snprintf(name,sizeof(name),"grapple-%02d-%03d.bmp",which,n);
  SDL_Surface *s=SDL_CreateRGBSurfaceFrom(pixels,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
  assert(s);assert(!SDL_SaveBMP(s,name));SDL_FreeSurface(s);
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);char *args[]={"test-grapple","--config","test-play.ini",NULL};
  int result=ww_upstream_main(3,args);assert(done);puts("Small, large and essential-object piledrivers completed on landing.");return result;
}
