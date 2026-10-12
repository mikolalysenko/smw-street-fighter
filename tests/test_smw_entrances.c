/* Original ghost-house and castle entrance scripts, for the entire roster. */
#include "ww_smw.h"
#include "assets.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
static int tick,map_ticks,base,warped,which,frames,walked,door,done;
static unsigned poses;
int ww_upstream_main(int,char**);
static void enter(void) {
  int castle=which/12;ow_players_map[0]=castle;
  PointU16 *p=get_PointU16(ow_players_grid_aligned_pos,0);p->x=p->y=0;
  int node=castle?0x25:0x04;ow_level_number_of_each_tiletbl[castle?0x400:0]=node;
  ow_level_tile_settings[node]=0;
  counter_sublevels_entered=flag_show_player_start=flag_disable_no_yoshi_intro=0;
  yoshi_carry_over_levels_flag=player_riding_yoshi_flag=0;
  player_current_state=timer_end_level=0;misc_game_mode=0x0f;
  frames=walked=door=0;poses=0;
}
uint32_t ww_test_input(void) {
  ++tick;
  if(done || tick>18000){SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;}
  if(!warped) {
    if(misc_game_mode==0x0e){++map_ticks;return map_ticks<240?(map_ticks%30<20?128:0):map_ticks%40==0?1:0;}
    if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
    if(misc_game_mode==0x14 && !misc_intro_level_flag && ++base>45){warped=1;enter();return 0;}
    return misc_game_mode==0x14?0:tick%45==0?8:tick%45==22?1:0;
  }
  if(frames>20 && misc_game_mode==0x0f) {
    printf("Entrance %d %s: %d walking frames, cycle mask %x, door=%d\n",which/12,fc_roster[which%12].name,walked,poses,door);
    assert(walked>50 && poses && (poses&(poses-1)) && door);
    if(++which==24){done=1;return 0;}
    enter();return 0;
  }
  if(misc_game_mode!=0x14)return 0;
  assert(player_current_state==0x0a);
  FcFight *f=(FcFight*)ww_fight_state();++frames;
  if(frames==2)fc_init(f,which%12);
  if(f->moving && fc_animation(f).pose==FC_POSE_WALK){++walked;poses|=1u<<(fc_animation(f).tick%24/4);}
  if(player_timer_pipe_warping==14 && player_xpos>=232)door=1;
  return 0;
}
void ww_test_render(uint8_t *p,size_t pitch,int w,int h) {
  if(which%12!=0 && which%12!=6 && which%12!=8)return;
  if(frames!=75 && !(player_timer_pipe_warping==14 && (player_xpos==190 || player_xpos==214 || player_xpos==240)))return;
  char name[80];snprintf(name,sizeof(name),"entrance-%02d-%03d.bmp",which,player_xpos);
  SDL_Surface *s=SDL_CreateRGBSurfaceFrom(p,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
  assert(s);assert(!SDL_SaveBMP(s,name));SDL_FreeSurface(s);
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);char *args[]={"test-entrance","--config","test-play.ini",NULL};
  int r=ww_upstream_main(3,args);assert(done);puts("All 24 original entrance scenes completed with walking animations.");return r;
}
