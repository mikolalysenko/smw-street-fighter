/* Verify real primary/secondary sublevel entrances against original ROM data. */
#include "ww_smw.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include "assets/smw_assets.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int tick,map_ticks,base_frames,warped,stage,room_frames,done,main_level=0x106,bonus_level,return_exit;
int ww_upstream_main(int,char**);
static void warp(int level) {
  for(int i=0;i<32;i++)misc_subscreen_exit_entrance_number_lo[i]=level;
  ow_players_map[0]=level>>8;flag_use_secondary_entrance=flag_show_player_start=0;
  player_current_state=timer_end_level=0;counter_sublevels_entered=1;misc_game_mode=0x0f;
}
static void take_exit(int screen) {
  if(misc_level_layout_flags&1)player_ypos=screen*256+64;else player_xpos=screen*256+64;
  printf("Take exit screen %d -> %03x secondary=%d\n",screen,
    (ow_players_map[0]!=0)*256+misc_subscreen_exit_entrance_number_lo[screen],flag_use_secondary_entrance);
  player_current_state=0;IncrementSublevelsEnteredAndPrepareToLoadSublevel();room_frames=0;
}
uint32_t ww_test_input(void) {
  ++tick;
  if(done || tick>8000){SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;}
  if(!warped) {
    if(misc_game_mode==0x0e){++map_ticks;return map_ticks<240?(map_ticks%30<20?128:0):map_ticks%40==0?1:0;}
    if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
    if(misc_game_mode==0x14 && !misc_intro_level_flag && ++base_frames>45){warp(main_level);warped=1;return 0;}
    return misc_game_mode==0x14?0:tick%45==0?8:tick%45==22?1:0;
  }
  if(misc_game_mode!=0x14)return 0;
  if(++room_frames<15)return 0;
  assert(kLevelInfo_05F800_SIZE==512 && kLevelInfo_05FA00_SIZE==512 &&
    kLevelInfo_05FC00_SIZE==512 && kLm5FE00_SIZE==512);
  if(stage==0) {
    for(int i=0;i<misc_screens_in_lvl;i++)if(misc_subscreen_exit_entrance_number_lo[i]!=(main_level&255) && misc_subscreen_exit_entrance_number_lo[i]) {
      int id=(ow_players_map[0]!=0)*256+misc_subscreen_exit_entrance_number_lo[i];
      bonus_level=flag_use_secondary_entrance?(id&0x100)|kLevelInfo_05F800[id]:id;
      take_exit(i);stage=1;return 0;
    }
    assert(0);
  }
  if(stage==1) {
    assert(ptr_sprite_list_data.addr==kLoadLevel_SpriteDataPtrs[bonus_level]);
    for(int i=0;i<misc_screens_in_lvl;i++) {
      int id=(ow_players_map[0]!=0)*256+misc_subscreen_exit_entrance_number_lo[i];
      int dest=flag_use_secondary_entrance?(id&0x100)|kLevelInfo_05F800[id]:id;
      if(dest==main_level){return_exit=id;take_exit(i);stage=2;return 0;}
    }
    assert(0);
  }
  if(stage==2) {
    printf("Return expected %03x via secondary %03x, sprite data %04x / %04x, pos %d,%d\n",
      main_level,return_exit,ptr_sprite_list_data.addr,kLoadLevel_SpriteDataPtrs[main_level],player_xpos,player_ypos);
    assert(ptr_sprite_list_data.addr==kLoadLevel_SpriteDataPtrs[main_level]);
    assert(player_xpos==(main_level==0x106?4328:2952));
    assert(player_ypos==(main_level==0x106?337:319));
    if(main_level==0x106){main_level=0x015;warp(main_level);stage=room_frames=0;return 0;}
    done=1;
  }
  return 0;
}
void ww_test_render(uint8_t *pixels,size_t pitch,int w,int h) {(void)pixels;(void)pitch;(void)w;(void)h;}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);char *args[]={"test-pipes","--config","test-play.ini",NULL};
  int result=ww_upstream_main(3,args);assert(done);puts("Original bonus-room return routes passed.");return result;
}
