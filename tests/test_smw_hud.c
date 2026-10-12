/* Boot the original engine and check the counters that feed the arcade HUD.
 * All snapshots/saves are produced in the replay runner's isolated directory. */
#include "ww_smw.h"
#include "ww_hud.h"
#include "audio.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int tick,map_ticks,select_ticks,level_ticks,clock_start,paused_time,done;
static void pickup(int id) {
  spr_spriteid[11]=id;spr_current_status[11]=8;InitializeNormalSpriteRAMTables(11);
  SetSprXYPos(11,player_xpos,player_ypos+16);
  spr_decrementing_table1540[11]=0;
}
int ww_upstream_main(int,char**);
uint32_t ww_test_input(void) {
  if(++tick>5000 || done){SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;}
  if(fc_audio_selection_active()) {
    ++select_ticks;return select_ticks==2?128:select_ticks==6?8:0; /* Honda's wide name. */
  }
  if(misc_game_mode==0x0e) {
    ++map_ticks;return map_ticks<240?(map_ticks%30<20?128:0):map_ticks%40==0?1:0;
  }
  if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
  if(misc_game_mode!=0x14)return tick%45==0?8:tick%45==22?1:0;
  ++level_ticks;assert(ww_enabled());
  FcStageHud h=ww_stage_hud();
  if(level_ticks==40) {
    clock_start=h.time;assert(clock_start>300 && h.lives==5 && h.timed);
    for(int i=0;i<12;i++)spr_current_status[i]=0;
    /* Loading original icons must never write decompression scratch into RAM. */
    uint8 before[0x20000];memcpy(before,g_ram,sizeof(before));
    ww_hud_reset();h=ww_stage_hud();assert(!memcmp(before,g_ram,sizeof(before)));
    assert(h.coin_icon.pixels && h.star_icon.pixels);
  }
  if(level_ticks==60) {
    player_current_coin_count=98;player_mario_bonus_stars=27;
    counter_yoshi_coins_to_display=3;player_current_item_box=1;
    GiveCoins_OneCoin();
  }
  if(level_ticks==85) {
    assert(h.time<clock_start && h.coins==99 && h.lives==5);
    assert(h.bonus_stars==27 && h.dragon_coins==3 && !player_current_item_box);
  }
  if(level_ticks==90)GiveCoins_OneCoin();
  if(level_ticks==100) {
    assert(h.coins==0 && h.lives==6); /* Native 100-coin rollover pays exactly once. */
    counter_timer_hundreds=0;counter_timer_tens=9;counter_timer_ones=9;
    FcFight *f=(FcFight*)ww_fight_state();f->hp=75;f->meter=0;pickup(0x74);
  }
  if(level_ticks==106 || level_ticks==112 || level_ticks==118 || level_ticks==124) {
    FcFight *f=(FcFight*)ww_fight_state();
    assert(f->meter==(level_ticks==106?100:level_ticks==112?200:300) && f->hp==75);
    assert(!player_current_item_box && !player_current_state && !flag_sprites_locked);
    if(level_ticks==106){f->meter=0;pickup(0x75);}
    if(level_ticks==112){f->meter=0;pickup(0x77);}
    if(level_ticks==118)pickup(0x74); /* At MAX, consume without storing. */
  }
  if(level_ticks==130){paused_time=h.time;return 8;}
  if(level_ticks>130 && level_ticks<=170)assert(h.time==paused_time);
  if(level_ticks==170)return 8;
  if(level_ticks==220) {
    assert(h.time<paused_time && h.lives==6);
    player_mario_score_lo=0x3f;player_mario_score_mid=0x42;player_mario_score_hi=0x0f;
    player_current_life_count=98;player_current_coin_count=99;player_mario_bonus_stars=99;
    counter_yoshi_coins_to_display=5;player_current_item_box=4;
    counter_timer_hundreds=9;counter_timer_tens=9;counter_timer_ones=9;
  }
  if(level_ticks==225)assert(h.score==9999990 && h.high_score==9999990 && h.lives==99 && h.dragon_coins==5);
  if(level_ticks==230)done=1;
  return 0;
}
static void save(uint8_t *pixels,size_t pitch,int w,int h,const char *name) {
  SDL_Surface *s=SDL_CreateRGBSurfaceFrom(pixels,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
  assert(s);assert(!SDL_SaveBMP(s,name));SDL_FreeSurface(s);
}
void ww_test_render(uint8_t *pixels,size_t pitch,int w,int h) {
  if(misc_game_mode!=0x14 || misc_intro_level_flag)return;
  if(level_ticks==85)save(pixels,pitch,w,h,"hud-honda.bmp");
  if(level_ticks==104 || level_ticks==110 || level_ticks==116 || level_ticks==124) {
    char name[32];snprintf(name,sizeof(name),"hud-meter-%d.bmp",level_ticks);save(pixels,pitch,w,h,name);
  }
  if(level_ticks==225) {
    save(pixels,pitch,w,h,"hud-max-counters.bmp");
    /* Contact sheet for all twelve name/portrait combinations at maximum counters. */
    uint32_t sheet[768*224]={0};FcCanvas c={sheet,768,224,768};
    FcStageHud status=ww_stage_hud();
    for(int i=0;i<12;i++) {
      int x=(i%3)*256,y=(i/3)*56;
      FcCanvas cell={sheet+y*768+x,256,56,768};
      FcFight f=*ww_fight_state();f.character=i;f.hp=fc_roster[i].vitality;
      /* Give the HUD its normal height; keep only its 56 top rows. */
      uint32_t full[256*224]={0};FcCanvas view={full,256,224,256};
      fc_rect(view,0,0,256,224,0x0064b0);
      fc_battle_hud(view,&f,NULL,&status);
      for(int row=0;row<56;row++)memcpy(cell.pixels+row*cell.pitch,full+row*256,256*4);
    }
    save((uint8_t*)c.pixels,c.pitch*4,c.w,c.h,"hud-roster.bmp");
  }
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);char *args[]={"test-hud","--config","test-play.ini",NULL};
  int result=ww_upstream_main(3,args);assert(done);
  puts("HUD: native timer/pause, coins to 1UP, lives, bonus/Dragon Coins, three meter pickups and max score passed.");
  return result;
}
