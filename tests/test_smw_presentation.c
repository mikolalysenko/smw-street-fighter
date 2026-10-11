/* Regression replay: platform redraw flags, original Yoshi OAM, and loading
 * an actual save through the original file menu before choosing a fighter. */
#include "ww_smw.h"
#include "audio.h"
#include "draw.h"
#include "assets.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int tick,flow,map_ticks,level_ticks,select_ticks,selections,platform,yoshi,riding,done;
int ww_upstream_main(int,char**);
static void save(uint8_t *pixels,size_t pitch,int w,int h,const char *name) {
  SDL_Surface *s=SDL_CreateRGBSurfaceFrom(pixels,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
  assert(s);assert(!SDL_SaveBMP(s,name));SDL_FreeSurface(s);
}
uint32_t ww_test_input(void) {
  ++tick;
  if(tick>9000 || done){SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;}
  if(fc_audio_selection_active()) {
    assert(misc_game_mode==0x0b); /* Before intro, overworld or resumed level. */
    if(!select_ticks++) {
      selections++;printf("Player select %d before map/level, intro=%d\n",selections,misc_intro_level_flag);
      if(flow)assert(!misc_intro_level_flag);
    }
    if(select_ticks==2 && flow)return 1u<<7;
    return select_ticks==6?8:0;
  }
  if(misc_game_mode==0x0e) {
    ++map_ticks;
    if(flow)return map_ticks%40==0?1:0;
    return map_ticks<240?(map_ticks%30<20?1u<<7:0):map_ticks%40==0?1:0;
  }
  if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
  if(misc_game_mode==0x14 && !misc_intro_level_flag) {
    ++level_ticks;assert(selections==flow+1 && ww_enabled());
    if(flow) {
      assert(ww_fight_state()->character==FC_HONDA);
      if(level_ticks==60)done=1;
      return 0;
    }
    if(level_ticks==100) {
      for(int k=0;k<12;k++)spr_current_status[k]=0;
      spr_spriteid[10]=0x35;spr_current_status[10]=1;
      InitializeNormalSpriteRAMTables(10);
      SetSprXYPos(10,player_xpos+55,player_ypos);
      flag_yoshi_saved=1;growing_yoshi_timer=0;
    }
    if(level_ticks==140) {
      player_xpos=GetSprXPos(10);player_ypos=GetSprYPos(10)-40;
      player_in_air_flag=11;player_yspeed=16;player_xspeed=0;
    }
    if(level_ticks==190)misc_display_message=1;
    if(level_ticks==210) {
      assert(platform && yoshi && riding);SaveGame();
      assert(BufferFileSelectText_009DB5(0).first!=0xffff);
      /* Reboot only the isolated test runtime; no user's save is read. */
      SmwVectorReset();ww_init();flow=1;map_ticks=level_ticks=select_ticks=0;
    }
    return 0;
  }
  return tick%45==0?8:tick%45==22?1:0;
}
static void check_yoshi(void) {
  int slot=(spr_oamindex[10]>>2)+64;
  OamEnt *oam=(OamEnt*)oam_buf;
  printf("Yoshi: ride=%d slot=%d head=%d/%d body=%d/%d\n",player_riding_yoshi_flag,slot,
    oam[slot].charnum,oam[slot].ypos,oam[slot+1].charnum,oam[slot+1].ypos);
  printf("Player: x=%d y=%d hp=%d invuln=%d hide=%d state=%d locked=%d\n",player_xpos,player_ypos,
    ww_fight_state()->hp,ww_fight_state()->invuln,player_hide_player_tile_flags,player_current_state,flag_sprites_locked);
  const FcFight *f=ww_fight_state();printf("Draw: char=%d tick=%u move=%d facing=%d air=%d cam=%d,%d message=%d\n",
    f->character,f->tick,f->move,f->facing,player_in_air_flag,mirror_current_layer1_xpos,mirror_current_layer1_ypos,misc_display_message);
  uint32_t overlay[256*224]={0};ww_render((uint8_t*)overlay,256*4,256,224);
  int count=0;for(int y=60;y<200;y++)for(int x=0;x<256;x++)count+=overlay[y*256+x]!=0;
  printf("Fighter pixels %d\n",count);assert(count>100);
  assert(spr_current_status[10]==8 && spr_spriteid[10]==0x35);
  assert(oam[slot].charnum==6 && oam[slot+1].charnum==8);
  assert(oam[slot].ypos<224 && oam[slot+1].ypos<224);
}
void ww_test_render(uint8_t *pixels,size_t pitch,int w,int h) {
  if(flow)return;
  if(level_ticks==80 && !platform) {
    FcFight *f=(FcFight*)ww_fight_state();f->invuln=f->stun=0;
    timer_player_hurt=0;player_hide_player_tile_flags=0;
    ww_begin_frame();
    PointU16 point={player_xpos+8,player_ypos+40};
    sub_1C9EC(10,point); /* Actual brown chained platform contact/redraw. */
    assert(misc_player_on_solid_sprite==3 && player_hide_player_tile_flags==0xff);
    ww_player_draw(); /* Native second draw sees ff, must keep the early draw. */
    uint32_t overlay[256*224]={0};ww_render((uint8_t*)overlay,256*4,256,224);
    int count=0;for(int y=60;y<200;y++)for(int x=0;x<256;x++)count+=overlay[y*256+x]!=0;
    assert(count>100);platform=1;puts("Platform early draw stays visible after native hide=ff.");
  }
  if(level_ticks==125 && !yoshi) {check_yoshi();save(pixels,pitch,w,h,"yoshi-visible.bmp");yoshi=1;}
  if(level_ticks==175 && !riding) {
    assert(player_riding_yoshi_flag);check_yoshi();save(pixels,pitch,w,h,"yoshi-riding.bmp");riding=1;
    uint32_t sheet[640*360]={0};FcCanvas c={sheet,640,360,640};
    for(int id=0;id<4;id++)for(int attack=0;attack<3;attack++) {
      int x=80+id*160,feet=100+attack*110;
      fc_sprite(c,id,attack==0?FC_POSE_DUCK:attack==1?FC_POSE_CROUCH_PUNCH:FC_POSE_CROUCH_KICK,
        attack==1?4:8,x,feet,1,1,1,0);
    }
    save((uint8_t*)sheet,640*4,640,360,"crouching-attacks.bmp");
  }
  if(level_ticks==195) {
    assert(misc_display_message);
    uint32_t overlay[256*224]={0};ww_render((uint8_t*)overlay,256*4,256,224);
    int count=0;for(int y=60;y<200;y++)for(int x=0;x<256;x++)count+=overlay[y*256+x]!=0;
    assert(count>100);puts("Message boxes preserve the cached fighter draw.");
  }
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);char *args[]={"test-presentation","--config","test-play.ini",NULL};
  int result=ww_upstream_main(3,args);
  assert(done && selections==2 && platform && yoshi && riding);
  puts("Presentation: platform, Yoshi standing/riding, new-game and saved-game player select passed.");
  return result;
}
