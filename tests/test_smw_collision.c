/* Replay dashes against native Map16 pipe walls and camera barriers. */
#include "ww_smw.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int tick,map_ticks,base_frames,warped,room_frames,case_id,step,done;
static int pipe_x,pipe_y,floor_y,start_x;
int ww_upstream_main(int,char**);
static unsigned tile(int x,int y) {
  unsigned o=GetLevelLayoutPtr_Horizontal(x>>8)+(y&0xf0)+((x&255)>>4)+((y>>8)<<8);
  return g_ram[o]+256*g_ram[o+0x10000];
}
uint32_t ww_test_input(void) {
  ++tick;FcFight *f=(FcFight*)ww_fight_state();
  if(done || tick>16000){SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;}
  if(!warped) {
    if(misc_game_mode==0x0e){++map_ticks;return map_ticks<240?(map_ticks%30<20?128:0):map_ticks%40==0?1:0;}
    if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
    if(misc_game_mode==0x14 && !misc_intro_level_flag && ++base_frames>45) {
      for(int i=0;i<32;i++)misc_subscreen_exit_entrance_number_lo[i]=0x06;
      ow_players_map[0]=1;flag_use_secondary_entrance=flag_show_player_start=0;
      player_current_state=timer_end_level=0;counter_sublevels_entered=1;misc_game_mode=0x0f;
      warped=1;return 0;
    }
    return misc_game_mode==0x14?0:tick%45==0?8:tick%45==22?1:0;
  }
  if(misc_game_mode!=0x14)return 0;
  if(++room_frames<50)return 0;
  if(!pipe_x) {
    /* A flat native Map16 test lane makes wall deaths unambiguous: no pits,
     * sprites or sloped floors can kill the player during recovery. */
    pipe_x=128;pipe_y=128;floor_y=208;
    for(int x=0;x<1024;x+=16)for(int y=0;y<432;y+=16) {
      unsigned o=GetLevelLayoutPtr_Horizontal(x>>8)+(y&0xf0)+((x&255)>>4)+((y>>8)<<8);
      unsigned t=y>=floor_y?0x130:0x25;
      if(x>=pipe_x && x<pipe_x+32 && y>=pipe_y && y<floor_y)
        t=y==pipe_y?(x==pipe_x?0x137:0x138):(x==pipe_x?0x135:0x136);
      g_ram[o]=t;g_ram[o+0x10000]=t>>8;
    }
  }
  int c=case_id>=16?FC_DHALSIM:case_id/8?FC_VEGA:FC_BISON,kind=case_id%8;
  if(!step) {
    fc_init(f,c);f->meter=kind==3?200:100;
    for(int k=0;k<12;k++)spr_current_status[k]=0;
    player_current_power_up=1;player_current_state=0;
    player_xspeed=player_yspeed=player_sub_xpos=player_sub_ypos=0;
    player_blocked_flags=player_ducking_flag=0;player_in_air_flag=0;
    player_ypos=floor_y-32;player_xpos=pipe_x-38;
    player_facing_direction=1;
    if(kind==4){player_xpos=pipe_x+54;player_facing_direction=0;}
    if(kind==5){player_ypos=pipe_y+2;player_in_air_flag=11;}
    if(kind>=6){player_xpos=kind==6?mirror_current_layer1_xpos+9:mirror_current_layer1_xpos+229;
      player_ypos=100;player_in_air_flag=11;player_facing_direction=kind!=6;}
    mirror_current_layer1_xpos=misc_layer1_pos[0].x=0;
    flag_layer1_horiz_scroll_level_setting=0;camera_last_screen_horiz=1;
    mirror_current_layer1_ypos=misc_layer1_pos[0].y=floor_y>208?floor_y-208:0;
    if(kind>=6)player_xpos=mirror_current_layer1_xpos+(kind==6?9:229);
    if(kind>=6)player_ypos=floor_y-32;
    player_on_screen_pos_x=player_xpos-mirror_current_layer1_xpos;
    player_on_screen_pos_y=player_ypos-mirror_current_layer1_ypos;
    start_x=player_xpos;
    printf("Case %d %s kind %d x=%d y=%d cam=%d\n",case_id,fc_roster[c].name,kind,player_xpos,player_ypos,mirror_current_layer1_xpos);
  } else if(player_current_state==9 || f->hp<=0) {
    fprintf(stderr,"COLLISION DEATH case=%d frame=%d pos=%d,%d flags=%x cam=%d screen=%d,%d move=%d/%d\n",
      case_id,step,player_xpos,player_ypos,player_blocked_flags,mirror_current_layer1_xpos,
      player_on_screen_pos_x,player_on_screen_pos_y,f->move,f->move_tick);assert(0);
  }
  ++step;
  if(step==65) {
    if(kind<6)assert(player_xpos<pipe_x || player_xpos>=pipe_x+20);
    assert(tile(pipe_x,pipe_y)==0x137 && tile(pipe_x+16,pipe_y)==0x138);
    printf("Survived %s kind %d: x %d -> %d\n",fc_roster[c].name,kind,start_x,player_xpos);
    if(++case_id==24)done=1;step=0;return 0;
  }
  if(step==2)return c==FC_DHALSIM?32|512:kind==1?32|256:kind==2 || kind==3?2048:512;
  /* Air edge cases land normally; the original ground is below the camera. */
  return 0;
}
void ww_test_render(uint8_t *pixels,size_t pitch,int w,int h) {
  if(step!=18)return;
  char name[64];snprintf(name,sizeof(name),"collision-%02d.bmp",case_id);
  SDL_Surface *s=SDL_CreateRGBSurfaceFrom(pixels,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
  assert(s);assert(!SDL_SaveBMP(s,name));SDL_FreeSurface(s);
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);char *args[]={"test-collision","--config","test-play.ini",NULL};
  int result=ww_upstream_main(3,args);assert(done);
  puts("Native pipe tiles remain intact; both camera edges survived all dash cases.");return result;
}
