/* End-to-end replay through the real SMW main loop, level loader and PPU.
 * Built only by test_smw_play.py; never part of the shipped executable. */
#include "ww_smw.h"
#include "audio.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <stdio.h>
#include <assert.h>
static int tick,level_ticks,kills,last_mode=-1,map_ticks,super_seen,hurt_seen,ko_seen;
static int capture_selection,capture_play,capture_super,capture_ko;
int ww_upstream_main(int argc,char **argv);
uint32_t ww_test_input(void) {
  ++tick;
  if(last_mode!=misc_game_mode) {
    printf("frame %d: mode %02x intro %d level %02x\n",tick,misc_game_mode,misc_intro_level_flag,ow_level_number_lo);
    last_mode=misc_game_mode;
  }
  const FcFight *f=ww_fight_state();
  if(f->events&FC_SND_HIT)++kills;
  if(f->hp>0 && f->hp<100)hurt_seen=1;
  if(f->hp==0)ko_seen=1;
  if(f->move==FC_SUPER_MOVE)super_seen=1;
  if(tick>5000 || (level_ticks>480 && ko_seen)) {
    SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;
  }
  if(misc_game_mode==0x0e) {
    ++map_ticks;
    return map_ticks<240?(map_ticks%30<20?1u<<7:0):map_ticks%40==0?1:0;
  }
  if(misc_game_mode==0x14 && misc_intro_level_flag)
    return tick%45==0?1:0;
  if(misc_game_mode==0x14 && !misc_intro_level_flag) {
    ++level_ticks;
    if(!ww_enabled())return level_ticks%30==0?8:0;
    /* A deterministic enemy fixture inside the fully loaded original level. */
    if(level_ticks==100 || level_ticks==270) {
      int slot=10;
      spr_spriteid[slot]=0x0f;spr_current_status[slot]=1;
      SetSprXYPos((uint8)slot,player_xpos+(level_ticks==100?36:8),player_ypos+16);
    }
    if(level_ticks==400)DamagePlayer_Kill();
    if(level_ticks==150) return 1u<<11;
    if(level_ticks<220 && level_ticks%50==0)return 1u<<9;
    return 0;
  }
  return tick%45==0?8:tick%45==22?1:0;
}
static void save(uint8_t *pixels,size_t pitch,int w,int h,const char *name) {
  SDL_Surface *s=SDL_CreateRGBSurfaceFrom(pixels,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
  assert(s);assert(SDL_SaveBMP(s,name)==0);SDL_FreeSurface(s);
}
void ww_test_render(uint8_t *pixels,size_t pitch,int w,int h) {
  const FcFight *f=ww_fight_state();
  if(fc_audio_selection_active() && !capture_selection++)save(pixels,pitch,w,h,"selection.bmp");
  if(level_ticks==90 && !capture_play++)save(pixels,pitch,w,h,"original-level.bmp");
  if(f->flash && !capture_super++)save(pixels,pitch,w,h,"super.bmp");
  if(f->hp==0 && !capture_ko++)save(pixels,pitch,w,h,"ko.bmp");
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);
  char *argv[]={"test-smw-play","--config","test-play.ini",NULL};
  int result=ww_upstream_main(3,argv);
  printf("Replay: level_frames=%d hits=%d super=%d hurt=%d ko=%d\n",level_ticks,kills,super_seen,hurt_seen,ko_seen);
  assert(level_ticks>300);assert(ow_level_number_lo!=0x28);assert(kills>0);assert(super_seen);assert(hurt_seen);assert(ko_seen);
  return result;
}
