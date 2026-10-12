/* Teaser director, linked only into the capture executable. The actual engine
 * owns every animation, attack, collision, boss wave, pixel and audio sample.
 * Scenes start from fresh saves. Positions, fighters and meter are staged just
 * before recording; production builds contain none of these hooks. */
#include "ww_smw.h"
#include "ww_targets.h"
#include "ww_finalboss.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include "audio.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static int tick,map_ticks,base,started,scene,n,warped,done,recording,recorded;
static FILE *video,*sound;
int ww_upstream_main(int,char**);
static void finish(void) {done=1;SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);}
static void spawn(int k,int id,int x,int y) {
  spr_spriteid[k]=id;spr_current_status[k]=8;InitializeNormalSpriteRAMTables(k);
  SetSprXYPos(k,x,y);spr_xspeed[k]=spr_yspeed[k]=0;
}
uint32_t ww_test_input(void) {
  ++tick;recording=0;
  FcFight *f=(FcFight*)ww_fight_state();
  if(done || tick>18000){finish();return 0;}
  if(scene==0) {
    if(misc_game_mode==6 || misc_game_mode==8) {
      if(++n>45)recording=1;
      if(recorded>=240)finish();
    }
    return 0;
  }
  if(fc_audio_selection_active()) {
    if(scene==1) {
      recording=1;++n;
      if(recorded>=300)finish();
      return n%24==0?128:0;
    }
    return tick%30==0?8:0;
  }
  if(!warped && misc_game_mode==0x0e) {
    ++map_ticks;return map_ticks<240?(map_ticks%30<20?128:0):map_ticks%40==0?1:0;
  }
  if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
  if(misc_game_mode!=0x14) {
    if(warped)return 0;
    return tick%45==0?8:tick%45==22?1:0;
  }
  if(++base<80 || !ww_enabled())return 0;
  if(scene>=6 && !warped) {
    for(int i=0;i<32;i++)misc_subscreen_exit_entrance_number_lo[i]=0xc7;
    ow_players_map[0]=1;flag_use_secondary_entrance=flag_show_player_start=0;
    player_current_state=timer_end_level=0;counter_sublevels_entered=1;misc_game_mode=0x0f;
    warped=1;return 0;
  }
  if(scene==6) {
    recording=1;
    if(ww_final_phase()==WW_FINAL_FIGHT && ww_final_tick()>40)finish();
    return 0;
  }
  if(scene>=7 && !started && (ww_final_phase()!=WW_FINAL_FIGHT || ww_final_tick()<190))return 0;
  if(!started) {
    if(player_in_air_flag)return 0;
    int character=scene==4?FC_BISON:scene==5?FC_CHUNLI:scene==8?FC_ZANGIEF:FC_RYU;
    fc_init(f,character);f->meter=300;
    player_xspeed=player_yspeed=0;player_facing_direction=1;
    if(scene<6 && scene!=4) {
      player_xpos+=64;
      memset(spr_current_status,0,12);
      if(scene==2)spawn(9,4,player_xpos+28,player_ypos+16);
      if(scene==3)for(int k=0;k<6;k++)spawn(k,0xf,player_xpos+68+k*28,player_ypos+16);
      if(scene==5)spawn(9,0xf,player_xpos+80,player_ypos+16);
    }
    if(scene==8)for(int k=0;k<12;k++)if(ww_final_controls(k))player_xpos=GetSprXPos(k);
    started=1;n=0;
  }
  recording=1;++n;
  switch(scene) {
    case 2:
      if(n>=210)finish();
      if(n==20)return 2;
      if(n==95)return 512;
      return 0;
    case 3:
      if(n>=270)finish();
      return n==25?2048:0;
    case 4:
      if(timer_end_level && n>700)finish();
      if(n>=950)finish();
      return n==20?2048:0;
    case 5:
      if(n>=290)finish();
      if(n==20)return 512;
      return n>=85 && n<250 && n%8==0?256:0;
    case 7:
      if(n>=600)finish();
      if(n%130==0)return 512;
      if(n%240>=90 && n%240<110)return 1;
      if(n%240==110)return 16|512;
      return 0;
    case 8:
      if(n>=210)finish();
      return n==1?2048:0;
  }
  return 0;
}
void ww_test_render(uint8_t *pixels,size_t pitch,int w,int h) {
  int16_t music[801*2];float mix[801*2];
  if(fc_audio_music_active())memset(music,0,sizeof(music));else RtlRenderAudio(music,801,2);
  fc_audio_mix(mix,801);
  for(int i=0;i<1602;i++){float v=mix[i]+music[i]/32768.0f*0.65f;mix[i]=v>1?1:v<-1?-1:v;}
  if(!recording)return;
  assert(w==256 && h==224);
  for(int y=0;y<h;y++)assert(fwrite(pixels+y*pitch,4,w,video)==(size_t)w);
  assert(fwrite(mix,sizeof(float),1602,sound)==1602);
  recorded++;
}
int main(int argc,char **argv) {
  assert(argc==4);scene=atoi(argv[1]);video=fopen(argv[2],"wb");sound=fopen(argv[3],"wb");assert(video && sound);
  char *args[]={"capture","--config","capture.ini",NULL};
  int r=ww_upstream_main(3,args);fclose(video);fclose(sound);
  printf("Captured scene %d: %d frames, %.2f seconds\n",scene,recorded,recorded*801.0/48000);
  assert(done && recorded>100);return r;
}
