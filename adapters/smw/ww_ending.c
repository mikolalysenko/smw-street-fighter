#include "ww_ending.h"
#include "assets.h"
#include "audio.h"
#include "smw_rtl.h"
#include "variables.h"
#include "ending_text.h"

static int winner=-1,ticks;
static bool active;
static void cloud(FcCanvas c,int x,int y) {
  fc_rect(c,x+8,y,24,4,0xc4e9ec);fc_rect(c,x,y+4,44,12,0xc4e9ec);
  fc_rect(c,x+4,y+4,32,8,0xf0f5dd);fc_rect(c,x+8,y+16,24,4,0xc4e9ec);
}
static void hill(FcCanvas c,int x,int base,int width,int height,uint32_t light,uint32_t dark) {
  for(int y=0;y<height;y+=4) {
    int inset=(height-y)*width/(height*2);
    fc_rect(c,x+inset,base-height+y,width-2*inset,4,dark);
    fc_rect(c,x+inset,base-height+y,(width-2*inset)/2,4,light);
  }
}
static void flower(FcCanvas c,int x,int y,uint32_t color) {
  fc_rect(c,x,y+3,2,9,0x195d38);fc_rect(c,x-3,y+7,3,2,0x399f4a);
  fc_rect(c,x-3,y,8,4,color);fc_rect(c,x-1,y-2,4,8,color);fc_rect(c,x,y+1,2,2,0xffd76b);
}
/* Five interim endings compose the original arcade sprites over distinct
 * pixel scenery. The seven illustrated endings use the generated PNGs. */
static void sprite_scene(FcCanvas c,int id) {
  uint32_t sky=id==FC_RYU?0xe5a067:id==FC_VEGA?0xa991bd:0x659cc4;
  fc_rect(c,0,0,256,144,sky);
  fc_rect(c,0,48,256,48,id==FC_RYU?0xf3c185:0x9fcac7);
  fc_rect(c,181,12,28,28,0xffe3a1);fc_rect(c,177,20,36,12,0xffe3a1);
  cloud(c,22,20);cloud(c,205,53);
  hill(c,-22,118,120,59,0x6a8e8b,0x577285);hill(c,61,118,124,84,0x9cb08c,0x6b9989);
  hill(c,153,118,139,51,0x8bad87,0x4e8b76);
  fc_rect(c,0,111,256,33,0x62784b);fc_rect(c,0,111,256,4,0xa6bf66);
  if(id==FC_RYU) {
    /* Winding road beyond the cliff. */
    for(int y=96;y<136;y+=4) {
      int x=165+(y<116?y-96:136-y);
      fc_rect(c,x,y,4+(y-96)/2,4,0xe1c898);
    }
    fc_rect(c,0,133,124,11,0x614937);fc_rect(c,0,129,128,4,0xb1b960);
    fc_sprite(c,id,FC_POSE_WIN,28,68,134,-1,1,1,0);
  } else if(id==FC_GUILE) {
    /* Airship rails, brass fittings, propeller and timber deck. */
    fc_rect(c,0,112,256,32,0x916247);
    for(int y=112;y<144;y+=8)fc_rect(c,0,y,256,2,0xcc9b64);
    for(int x=5;x<256;x+=32) {
      fc_rect(c,x,73,4,42,0x725a4b);fc_rect(c,x,71,4,3,0xedc67c);
      fc_rect(c,x+10,125,2,2,0x523e38);
    }
    fc_rect(c,0,80,256,5,0xd4ae78);fc_rect(c,0,85,256,3,0x725a4b);
    fc_rect(c,214,45,6,65,0x725a4b);fc_rect(c,194,43,46,5,0xe3cfab);
    fc_rect(c,211,29,10,30,0xb69a70);fc_rect(c,208,41,16,10,0x805749);
    fc_sprite(c,id,FC_POSE_WIN,20,101,137,1,1,1,0);
  } else if(id==FC_KEN) {
    fc_rect(c,105,99,151,45,0x659b98);fc_rect(c,113,105,140,28,0xa4d4c2);
    for(int x=108;x<256;x+=22){fc_rect(c,x,95,19,8,0x929387);fc_rect(c,x+3,94,12,3,0xd2cfb1);}
    for(int x=129;x<248;x+=36) {
      fc_rect(c,x,125,23,2,0xd7efe0);fc_rect(c,x+4,114,18,2,0x6caeac);
      fc_rect(c,x,63,3,12,0xd9e5d1);fc_rect(c,x+3,55,3,10,0xd9e5d1);
    }
    fc_rect(c,11,111,89,6,0xc29a62);fc_rect(c,19,117,5,20,0x64493e);fc_rect(c,89,117,5,20,0x64493e);
    fc_sprite(c,id,FC_POSE_WIN,30,59,137,1,1,1,0);
  } else {
    /* Garden trellis for Vega, castle fountain for Chun-Li. */
    fc_rect(c,0,126,256,18,0xbfc19e);fc_rect(c,0,133,256,2,0x939d80);
    for(int x=0;x<256;x+=32)fc_rect(c,x,134,1,10,0x939d80);
    if(id==FC_VEGA) {
      for(int x=172;x<256;x+=20)fc_rect(c,x,35,3,76,0xdac8a5);
      for(int y=43;y<111;y+=18)fc_rect(c,169,y,87,3,0xdac8a5);
      for(int x=180;x<250;x+=20)flower(c,x,45+(x%3)*17,0xda5575);
    } else {
      fc_rect(c,173,104,47,9,0xd7d9bc);fc_rect(c,186,81,20,24,0xb0c6bd);
      fc_rect(c,181,80,30,5,0xe5e4c9);fc_rect(c,193,58,5,23,0xc2f1ef);
      fc_rect(c,183,65,25,3,0xc2f1ef);fc_rect(c,180,68,3,11,0xc2f1ef);fc_rect(c,208,68,3,11,0xc2f1ef);
    }
    for(int x=12;x<250;x+=19)flower(c,x,115+(x%3)*3,id==FC_VEGA?0xdd4764:0xf0a0c0);
    fc_sprite(c,id,FC_POSE_WIN,id==FC_VEGA?63:30,85,140,1,1,1,0);
  }
}
void ww_ending_reset(void) {winner=-1;ticks=0;active=false;}
void ww_ending_win(int character) {
  if(character<0 || character>=FC_ROSTER_COUNT)return;
  winner=character;ticks=0;active=false;
}
int ww_ending_character(void) {return active?winner:-1;}
bool ww_ending_finish_level(void) {
  if(winner<0 || player_current_state!=11)return false;
  /* Practice enters through the room loader from any overworld node. The
   * native cutscene lookup would otherwise finish that unrelated level. */
  misc_currently_active_boss_end_cutscene=8;
  return true;
}
bool ww_ending_tick(uint32_t edge) {
  if(winner<0)return false;
  /* Let the original rescue and credits loader finish first. Its music
   * continues while the illustration holds the first credits frame. */
  if(!active && misc_game_mode==0x1b)active=true;
  if(!active) {
    if(misc_game_mode<0x0f)ww_ending_reset();
    return false;
  }
  ++ticks;
  if(ticks>=120 && (edge&(FC_START|FC_JUMP|FC_PUNCH))) {
    ww_ending_reset();fc_audio_play(FC_SND_SELECT);return false;
  }
  return true;
}
bool ww_ending_render(FcCanvas c) {
  if(!active)return false;
  fc_rect(c,0,0,c.w,c.h,0x090c1b);
  fc_center(c,2,fc_roster[winner].name,0xffdc66,2);
  fc_center(c,19,ending_text[winner][0],0xbedfff,1);
  if(fc_sprite_duration(winner,FC_POSE_ENDING))
    fc_sprite(c,winner,FC_POSE_ENDING,0,(c.w-256)/2,30,1,1,1,0);
  else if(c.w>=256 && c.h>=174)
    sprite_scene((FcCanvas){c.pixels+30*c.pitch+(c.w-256)/2,256,144,c.pitch},winner);
  fc_rect(c,0,175,c.w,1,0xb79543);
  for(int i=0;i<3;i++)fc_center(c,181+i*9,ending_text[winner][i+1],0xf3eace,1);
  if(ticks>=120 && ticks%90<65)fc_center(c,215,"START - CREDITS",0x9eb3cf,1);
  return true;
}
