/* Replay Iggy's original autoscrolling smasher room. No camera, terrain or
 * health cheats: only warp into the room and supply Bison's full meter. */
#include "ww_smw.h"
#include "smw_rtl.h"
#include "variables.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>

static int tick,map_ticks,base_frames,warped,room_frames,done,max_camera;
static int cancel_seen,restarted,landed;
static int at_exit;
static int last_camera,max_step,normal_frames,normal_steps,cancel_frame;
int ww_upstream_main(int,char**);
uint32_t ww_test_input(void) {
  ++tick;FcFight *f=(FcFight*)ww_fight_state();
  if(done || tick>13000) {SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;}
  if(!warped) {
    if(misc_game_mode==0x0e) {
      ++map_ticks;return map_ticks<240?(map_ticks%30<20?128:0):map_ticks%40==0?1:0;
    }
    if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
    if(misc_game_mode==0x14 && !misc_intro_level_flag && ++base_frames>45) {
      for(int i=0;i<32;i++)misc_subscreen_exit_entrance_number_lo[i]=0xfc;
      ow_players_map[0]=1;flag_use_secondary_entrance=flag_show_player_start=0;
      player_current_state=timer_end_level=0;counter_sublevels_entered=1;
      misc_game_mode=0x0f;warped=1;return 0;
    }
    return misc_game_mode==0x14?0:tick%45==0?8:tick%45==22?1:0;
  }
  if(misc_game_mode!=0x14) {
    if(at_exit && misc_game_mode==0x0f)done=1;
    return 0;
  }
  ++room_frames;
  int delta=mirror_current_layer1_xpos-last_camera;last_camera=mirror_current_layer1_xpos;
  if(room_frames>40 && delta>max_step)max_step=delta;
  if(cancel_seen && !restarted && room_frames>cancel_frame+2){normal_frames++;normal_steps+=delta;if(delta>1)fprintf(stderr,"Normal scroll n=%d delta=%d move=%d\n",room_frames,delta,f->move);assert(delta<=1);}
  if(room_frames==40) {fc_init(f,FC_BISON);f->meter=300;return 2048;}
  if(room_frames<40)return 0;
  if(player_current_state==9 || f->hp<=0) {
    fprintf(stderr,"Died at frame %d: pos=%d,%d camera=%d,%d move=%d/%d scroll=%d\n",
      room_frames,player_xpos,player_ypos,mirror_current_layer1_xpos,mirror_current_layer1_ypos,
      f->move,f->move_tick,l1_l2_scroll_spr_spriteid[0]);assert(0);
  }
  if(mirror_current_layer1_xpos>max_camera)max_camera=mirror_current_layer1_xpos;
  if(fc_psycho_flight(f) && !f->flash && f->move_tick>0) {
    int x=(int)player_xpos-mirror_current_layer1_xpos,y=(int)player_ypos-mirror_current_layer1_ypos;
    assert(x>=16 && x<=216 && y>=32 && y<=192);
  }
  if(room_frames%300==0)printf("Scroll %d: pos=%d,%d cam=%d,%d move=%d/%d\n",room_frames,
    player_xpos,player_ypos,mirror_current_layer1_xpos,mirror_current_layer1_ypos,f->move,f->move_tick);
  /* Cancel after the entry platform is long gone, then launch again. */
  if(!cancel_seen && max_camera>400 && f->move==FC_SUPER_MOVE)return 1024;
  if(!cancel_seen && max_camera>400 && f->move==FC_IDLE){cancel_seen=1;cancel_frame=room_frames;}
  if(cancel_seen && !restarted && room_frames-cancel_frame>=64 && !player_in_air_flag) {
    restarted=1;f->meter=300;return 2048;
  }
  if(restarted && f->move==FC_IDLE && !player_in_air_flag)landed=1;
  if(max_camera>0 && max_camera==(camera_last_screen_horiz-1)*256 && landed) {
    at_exit=1;return 128|16; /* Enter the untouched original boss door. */
  }
  return 0;
}
void ww_test_render(uint8_t *pixels,size_t pitch,int w,int h) {
  if(room_frames!=240 && !done)return;
  SDL_Surface *s=SDL_CreateRGBSurfaceFrom(pixels,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
  assert(s);assert(!SDL_SaveBMP(s,done?"psycho-scroll-finish.bmp":"psycho-scroll-flight.bmp"));SDL_FreeSurface(s);
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);char *args[]={"test-scroll","--config","test-play.ini",NULL};
  int result=ww_upstream_main(3,args);
  printf("Smasher autoscroll: camera=%d, cancel=%d, restart=%d, landing=%d\n",max_camera,cancel_seen,restarted,landed);
  assert(done && cancel_seen && restarted && landed && max_step>=6 && max_step<=8);
  assert(normal_frames>=60 && normal_steps>0 && normal_steps<=normal_frames);
  assert(room_frames<1300);return result;
}
