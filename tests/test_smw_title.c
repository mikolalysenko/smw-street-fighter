/* Boots the real title, earns points, then starts a second process against
 * the isolated runtime's .srm to verify persistence without touching user saves. */
#include "ww_smw.h"
#include "ww_score.h"
#include "ww_title.h"
#include "audio.h"
#include "smw_rtl.h"
#include "common_cpu_infra.h"
#include "snes/ppu.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int tick,title_ticks,map_ticks,level_ticks,select_ticks,done,reload;
static int title_captured,file_captured,hits;
static uint32_t expected,enemy_score;
int ww_upstream_main(int,char**);
static void save(uint8_t *pixels,size_t pitch,int w,int h,const char *name) {
  SDL_Surface *s=SDL_CreateRGBSurfaceFrom(pixels,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
  assert(s);assert(!SDL_SaveBMP(s,name));SDL_FreeSurface(s);
}
uint32_t ww_test_input(void) {
  if(++tick>5000 || done){SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;}
  if(misc_game_mode==6)return 0;
  if(misc_game_mode==7) {
    if(!title_ticks && reload) {
      FILE *f=fopen("expected-score","rb");assert(f);assert(fread(&expected,4,1,f)==1);fclose(f);
    }
    ++title_ticks;assert(ww_high_score()==(reload?expected:0));
    if(reload && title_ticks==180){done=1;return 0;}
    return title_ticks==180?8:0;
  }
  if(fc_audio_selection_active()) {
    assert(misc_game_mode==0x0b);
    return ++select_ticks==6?8:0;
  }
  if(misc_game_mode==0x0e) {
    ++map_ticks;return map_ticks<240?(map_ticks%30<20?128:0):map_ticks%40==0?1:0;
  }
  if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
  if(misc_game_mode==0x14) {
    ++level_ticks;assert(ww_enabled());
    if(ww_fight_state()->events&FC_SND_HIT)++hits;
    if(level_ticks==60) {
      for(int i=0;i<12;i++)spr_current_status[i]=0;
      spr_spriteid[10]=0x0f;spr_current_status[10]=1;
      SetSprXYPos(10,player_xpos+36,player_ypos+16);
    }
    if(level_ticks==65)return 512;
    if(level_ticks==150) {
      assert(hits && ww_score()>=200 && ww_high_score()==ww_score());
      enemy_score=ww_score();SpawnScoreSpriteAtPlayerPosition(9);
    }
    if(level_ticks==180) {
      expected=ww_high_score();assert(expected>enemy_score && expected==ww_score());
      SaveGame();assert(BufferFileSelectText_009DB5(0).first!=0xffff);
      uint8_t original[2048];memcpy(original,g_sram,2048);
      /* Bad/legacy tails are ignored and never interpreted as a huge record. */
      g_sram[0x7fc]^=1;assert(!ww_high_score());memcpy(g_sram,original,2048);
      GameMode0A_PlayerSelect_Entry2();assert(ww_score()==0 && ww_high_score()==expected);
      assert(!memcmp(original,g_sram,2048));
      FILE *f=fopen("expected-score","wb");assert(f);assert(fwrite(&expected,4,1,f)==1);fclose(f);
      done=1;
    }
    return 0;
  }
  return tick%45==0?8:tick%45==22?1:0;
}
void ww_test_render(uint8_t *pixels,size_t pitch,int w,int h) {
  if(misc_game_mode==7 && title_ticks==30 && !title_captured++) {
    save(pixels,pitch,w,h,reload?"title-high-score.bmp":"title-crossover.bmp");
    uint16_t vram[0x8000];memcpy(vram,g_my_ppu->vram,sizeof(vram));
    ww_title_begin(g_my_ppu);assert(memcmp(vram,g_my_ppu->vram,sizeof(vram)));
    ww_title_end(g_my_ppu);assert(!memcmp(vram,g_my_ppu->vram,sizeof(vram)));
  }
  if(misc_game_mode==8 && !file_captured++)save(pixels,pitch,w,h,"title-file-menu.bmp");
  if(level_ticks==175)save(pixels,pitch,w,h,"score-hud.bmp");
}
int main(void) {
  reload=getenv("WW_TEST_RELOAD")!=NULL;setvbuf(stdout,NULL,_IONBF,0);
  char *args[]={"test-title-score","--config","test-play.ini",NULL};
  int result=ww_upstream_main(3,args);assert(done && title_captured);
  if(!reload)assert(file_captured && hits && level_ticks>=180);
  printf("Title and score: %s, high score %u; original file menu and VRAM intact.\n",
    reload?"record survived process restart and attract mode":"combat and bonus points saved; new game preserves record",expected);
  return result;
}
