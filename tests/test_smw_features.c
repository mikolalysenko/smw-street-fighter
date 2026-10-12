/* Native block interactions, crouch guarding and all twelve ending screens. */
#include "ww_smw.h"
#include "ww_targets.h"
#include "ww_ending.h"
#include "assets.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int tick,map_ticks,base,ready,n,character,done,home_x,home_y;
int ww_upstream_main(int,char**);
static unsigned offset(int x,int y) {
  return GetLevelLayoutPtr_Horizontal(x>>8)+(y&0xf0)+((x&255)>>4)+((y>>8)<<8);
}
static void tile(int x,int y,unsigned id) {
  unsigned o=offset(x,y);g_ram[o]=id;g_ram[o+0x10000]=id>>8;
}
static unsigned get_tile(int x,int y) {unsigned o=offset(x,y);return g_ram[o]+256*g_ram[o+0x10000];}
static void blocks(void) {
  unsigned ids[]={0x11f,0x130,0x132,0x137,0x025,0x100,0x11e};
  for(int face=-1;face<=1;face+=2)for(int crouch=0;crouch<2;crouch++)for(int kick=0;kick<2;kick++)
    for(unsigned type=0;type<sizeof(ids)/sizeof(ids[0]);type++) {
      for(int y=112;y<=160;y+=16)for(int x=64;x<=208;x+=16)tile(x,y,0x25);
      memset(bounce_spr_spriteid,0,4);memset(spr_current_status,0,12);
      int x=face>0?144:96,y=crouch?144:128;tile(x,y,ids[type]);
      /* A block behind the first column must remain intact. */
      tile(x+face*16,y,0x130);
      FcFight f;fc_init(&f,FC_RYU);FcBody b={{120,128,16,32},face,true,false,false};
      int active=kick?7:4;
      blocks_xpos=300;blocks_ypos=321;misc_current_layer_being_processed=1;
      for(int t=1;t<=active;t++) {
        fc_tick(&f,(crouch?FC_DOWN:0)|(t==1?(kick?FC_KICK:FC_PUNCH):0),b,NULL,0);
        ww_scenery_hits(&f);
        assert(blocks_xpos==300 && blocks_ypos==321 && misc_current_layer_being_processed==1);
        if(t<active)assert(get_tile(x,y)==ids[type] && !bounce_spr_spriteid[3]);
      }
      if(type==0) {
        assert(bounce_spr_spriteid[3]);
        int item=0;for(int k=0;k<12;k++)item|=spr_current_status[k] && spr_spriteid[k]>=0x74 && spr_spriteid[k]<=0x77;
        assert(item); /* Original item dispensing, not a cosmetic explosion. */
      } else if(type==6)assert(bounce_spr_spriteid[3]==7); /* Turn, don't shatter. */
      else assert(get_tile(x,y)==ids[type]);
      if(type!=4)assert(get_tile(x+face*16,y)==0x130);
    }
  for(int kick=0;kick<2;kick++) {
    for(int y=112;y<=192;y+=16)for(int x=64;x<=208;x+=16)tile(x,y,0x25);
    tile(128,160,0x11e);memset(bounce_spr_spriteid,0,4);
    FcFight f;fc_init(&f,FC_RYU);FcBody b={{128,120,16,32},1,false,false,false};
    int active=kick?7:4;
    for(int t=1;t<=active;t++) {
      fc_tick(&f,FC_DOWN|(t==1?(kick?FC_KICK:FC_PUNCH):0),b,NULL,0);
      ww_scenery_hits(&f);
    }
    assert(bounce_spr_spriteid[3]==7 && get_tile(128,160)==0x11e);
    assert(!f.blasts[0].ttl);
  }
  puts("Normals: native item/turn blocks, aerial down-strikes, intact cement/pipes/ground and both facings/crouches passed.");
}
static void super_blocks(void) {
  unsigned ids[]={0x11e,0x12e,0x130,0x132,0x137,0x100,0x25,0x11f};
  for(int face=-1;face<=1;face+=2)for(int level=1;level<=3;level++)for(unsigned type=0;type<8;type++) {
    for(int y=64;y<208;y+=16)for(int x=0;x<512;x+=16)tile(x,y,0x25);
    memset(bounce_spr_spriteid,0,4);memset(spr_current_status,0,12);
    FcFight f;fc_init(&f,FC_RYU);f.meter=level*100;
    FcBody body={{160,128,16,32},face,true,false,false};
    int frame=0;
    do {fc_tick(&f,frame++?0:FC_SUPER,body,NULL,0);assert(frame<120);}while(!f.shots[0].ttl);
    FcBox box=f.shots[0].box;
    int x=((int)box.x+8)&~15,y=((int)box.y+8)&~15;
    if(x<0)x=0;
    int outside=((int)(box.y+box.h)+31)&~15;
    tile(x,y,ids[type]);tile(x,outside,0x130);
    blocks_xpos=300;blocks_ypos=321;misc_current_layer_being_processed=1;
    ww_scenery_hits(&f);
    assert(blocks_xpos==300 && blocks_ypos==321 && misc_current_layer_being_processed==1);
    if(type<4)assert(get_tile(x,y)==0x25);
    else if(type==7)assert(bounce_spr_spriteid[3]);
    else assert(get_tile(x,y)==ids[type]);
    assert(get_tile(x,outside)==0x130);
  }
  /* A turning block must not respawn after being shattered mid-spin. */
  FcFight f;fc_init(&f,FC_RYU);tile(128,128,0x152);
  memset(bounce_spr_spriteid,0,4);bounce_spr_spriteid[0]=7;
  bounce_spr_xpos_lo[0]=128;bounce_spr_ypos_lo[0]=128;
  bounce_spr_xpos_hi[0]=bounce_spr_ypos_hi[0]=0;
  f.shots[0]=(FcShot){.box={128,128,16,16},.ttl=1,.super=true};
  ww_scenery_hits(&f);assert(!bounce_spr_spriteid[0] && get_tile(128,128)==0x25);
  puts("Shinku: all three levels/both directions break blocks, preserve items/routes/terrain and stay within the beam.");
}
static void pipe_clipping(void) {
  uint32_t pixels[256*224];FcFight *f=(FcFight*)ww_fight_state();
  for(int character=0;character<12;character++)for(int action=0;action<8;action++)for(int camera=0;camera<=16;camera+=16) {
    for(int y=32;y<224;y+=16)for(int x=0;x<288;x+=16)tile(x,y,0x25);
    int inside=action&3;if(action>=4)inside^=1;
    int rim=(inside>=2?144:128)+camera;
    for(int d=0;d<80;d+=16)for(int cross=0;cross<32;cross+=16) {
      int along=inside==0 || inside==2?rim-16-d:rim+d;
      tile(inside>=2?112+camera+cross:along,inside>=2?along:128+camera+cross,0x135);
    }
    fc_init(f,character);player_xpos=120+camera;player_ypos=(inside>=2?122:128)+camera;
    mirror_current_layer1_xpos=mirror_current_layer1_ypos=camera;
    player_current_state=inside>=2?6:5;player_pipe_action=action;player_timer_pipe_warping=16;
    player_in_air_flag=player_riding_yoshi_flag=player_hide_player_tile_flags=timer_end_level=0;
    ww_begin_frame();ww_player_draw();memset(pixels,0,sizeof(pixels));ww_render((uint8_t*)pixels,1024,256,224);
    int visible=0;
    for(int y=80;y<190;y++)for(int x=32;x<224;x++) {
      bool interior=inside==0?x<128:inside==1?x>=128:inside==2?y<144:y>=144;
      if(interior)assert(!pixels[y*256+x]);else visible+=pixels[y*256+x]!=0;
    }
    assert(visible>5);
  }
  player_current_state=0;mirror_current_layer1_xpos=mirror_current_layer1_ypos=0;
  puts("Pipe clipping: all 12 fighters, all four directions, entering/exiting and scrolled camera offsets passed.");
}
static void endings(void) {
  uint8 old_mode=misc_game_mode;
  uint32_t pixels[256*224];FcCanvas c={pixels,256,224,256};uint64_t hashes[12];
  for(int id=0;id<FC_ROSTER_COUNT;id++) {
    ww_ending_reset();ww_ending_win(id);misc_game_mode=0x14;
    assert(!ww_ending_tick(0) && ww_ending_character()==-1);
    misc_game_mode=0x1b;
    assert(ww_ending_tick(FC_START) && ww_ending_character()==id);
    for(int i=0;i<125;i++)assert(ww_ending_tick(0));
    memset(pixels,0,sizeof(pixels));assert(ww_ending_render(c));
    uint64_t hash=UINT64_C(1469598103934665603);
    for(int i=30*256;i<174*256;i++)hash=(hash^pixels[i])*UINT64_C(1099511628211);
    for(int i=0;i<id;i++)assert(hashes[i]!=hash);hashes[id]=hash;
    char name[64];snprintf(name,sizeof(name),"ending-%02d.bmp",id);
    SDL_Surface *s=SDL_CreateRGBSurfaceFrom(pixels,256,224,32,1024,0xff0000,0xff00,0xff,0xff000000);
    assert(s && !SDL_SaveBMP(s,name));SDL_FreeSurface(s);
    assert(!ww_ending_tick(FC_START) && !ww_ending_render(c));
    assert(misc_game_mode==0x1b && !ww_ending_tick(0)); /* Resume credits once. */
  }
  misc_game_mode=old_mode;
  puts("Twelve distinct ending screens, reveal after rescue and input debounce/credits resume passed.");
}
uint32_t ww_test_input(void) {
  ++tick;FcFight *f=(FcFight*)ww_fight_state();
  if(done || tick>8000){SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;}
  if(misc_game_mode==0x0e){++map_ticks;return map_ticks<240?(map_ticks%30<20?128:0):map_ticks%40==0?1:0;}
  if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
  if(misc_game_mode!=0x14)return tick%45==0?8:tick%45==22?1:0;
  if(++base<80 || !ww_enabled())return 0;
  if(!ready){if(player_in_air_flag)return 0;home_x=player_xpos;home_y=player_ypos;ready=1;}
  if(!n) {
    fc_init(f,character);player_xpos=home_x;player_ypos=home_y;
    player_xspeed=player_yspeed=player_in_air_flag=0;player_facing_direction=1;
    memset(spr_current_status,0,12);timer_player_hurt=timer_star_power=0;
  }
  if(n==15) {
    assert(player_ducking_flag && f->guarding && f->crouching_guard);
    assert(fc_animation(f).pose==FC_POSE_CROUCH_GUARD);
    int hp=f->hp;assert(ww_take_damage(16,1) && f->hp==hp-2 && f->velocity_y==0);
  }
  if(n==40) {
    assert(player_ducking_flag && !player_in_air_flag);
    n=0;if(++character==FC_ROSTER_COUNT){blocks();super_blocks();pipe_clipping();endings();done=1;}
    return 0;
  }
  ++n;return 32|1024;
}
void ww_test_render(uint8_t *p,size_t pitch,int w,int h) {(void)p;(void)pitch;(void)w;(void)h;}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);char *args[]={"test-features","--config","test-play.ini",NULL};
  int r=ww_upstream_main(3,args);assert(done);puts("Native crouch guard passed for all twelve fighters.");return r;
}
