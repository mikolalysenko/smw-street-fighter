/* Counters use real inputs/projectiles in the loaded Mode 7 arena. The
 * fixture assists positioning, then waits for the return shot to hit. */
#include "ww_smw.h"
#include "ww_targets.h"
#include "ww_finalboss.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
static int tick,map_ticks,base,warped,stage,done,sent,hp,parry_hp,stage_ticks;
int ww_upstream_main(int,char**);
uint32_t ww_test_input(void) {
  ++tick;FcFight *f=(FcFight*)ww_fight_state();
  if(done || tick>9000){SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;}
  if(!warped) {
    if(misc_game_mode==0x0e){++map_ticks;return map_ticks<240?(map_ticks%30<20?128:0):map_ticks%40==0?1:0;}
    if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
    if(misc_game_mode==0x14 && !misc_intro_level_flag && ++base>45) {
      for(int i=0;i<32;i++)misc_subscreen_exit_entrance_number_lo[i]=0xc7;
      ow_players_map[0]=1;flag_use_secondary_entrance=flag_show_player_start=0;
      player_current_state=timer_end_level=0;counter_sublevels_entered=1;misc_game_mode=0x0f;warped=1;return 0;
    }
    return misc_game_mode==0x14?0:tick%45==0?8:tick%45==22?1:0;
  }
  if(misc_game_mode!=0x14)return 0;
  timer_star_power=32;timer_player_hurt=0;
  if(ww_final_phase()!=WW_FINAL_FIGHT)return 0;
  ++stage_ticks;
  if(stage_ticks%300==0)printf("Counter stage %d ticks %d hp %d reflections %d parries %d stomps %d\n",
    stage,stage_ticks,ww_opponent()->hp,ww_final_reflections(),f->parries,ww_final_stomps());
  if(stage==0) {
    if(ww_final_reflections()) {stage=1;stage_ticks=0;hp=ww_opponent()->hp;return 0;}
    for(int i=0;i<12;i++) {
      FcTarget shot;if(!ww_final_target(i,&shot))continue;
      player_xpos=(uint16)(shot.box.x-24);player_ypos=(uint16)(shot.box.y-10);
      player_xspeed=player_yspeed=0;player_in_air_flag=1;player_facing_direction=1;
      if(!sent){sent=1;fc_clear_actions(f);return 2;}
      return f->move==FC_IDLE?2:0;
    }
  } else if(stage==1 && ww_opponent()->hp<hp) {
    assert(ww_opponent()->hp==hp-6 && ww_final_attack()==WW_AKUMA_STAGGER);
    printf("Jab returned a fireball for six damage and stagger.\n");stage=2;stage_ticks=0;
  } else if(stage==2) {
    if(f->parries) {
      assert(f->hp==parry_hp && ww_final_reflections()==2 && f->meter>=25);
      stage=3;stage_ticks=0;hp=ww_opponent()->hp;
      printf("Timed guard preserved health, awarded meter and returned the shot.\n");return 0;
    }
    for(int i=0;i<12;i++) {
      FcTarget shot;if(!ww_final_target(i,&shot) || shot.box.y<153 || shot.box.y>176)continue;
      player_xpos=(uint16)shot.box.x;player_ypos=160;
      player_xspeed=player_yspeed=0;player_in_air_flag=0;
      int boss_x=0;for(int k=0;k<12;k++)if(ww_final_controls(k))boss_x=GetSprXPos(k);
      player_facing_direction=boss_x>player_xpos;timer_star_power=0;f->invuln=0;
      if(!f->guarding)fc_clear_actions(f);
      parry_hp=f->hp;return 1024;
    }
  } else if(stage==3 && ww_opponent()->hp<hp) {
    assert(ww_opponent()->hp==hp-6);stage=4;stage_ticks=0;hp=ww_opponent()->hp;
  } else if(stage==4) {
    if(ww_final_stomps()) {
      assert((int8)player_yspeed<0 && !f->air_move_used);
      assert(ww_opponent()->hp==hp-2);stage=5;stage_ticks=0;
      printf("Car stomp bounced the player, dealt two damage and refreshed the aerial move.\n");
    } else if(stage_ticks>20 && ww_final_vulnerable()) {
      fc_clear_actions(f);f->air_move_used=true;
      for(int k=0;k<12;k++)if(ww_final_controls(k)) {
        player_xpos=GetSprXPos(k)+12;player_ypos=GetSprYPos(k)-10;
      }
      player_yspeed=32;player_xspeed=0;player_in_air_flag=1;return 0;
    }
  } else if(stage==5 && stage_ticks>180) {
    assert(ww_opponent()->hp>0 && ww_final_attack()!=WW_AKUMA_STAGGER);
    done=1;
  }
  player_xpos=24;player_ypos=160;player_xspeed=player_yspeed=0;
  return 0;
}
void ww_test_render(uint8_t *p,size_t pitch,int w,int h) {
  const FcFight *f=ww_fight_state();
  if(!warped || (f->parry_flash!=14 && !(stage==1 && stage_ticks==5) && !(stage==5 && stage_ticks==5)))return;
  char path[64];snprintf(path,sizeof(path),"akuma-counter-%d.bmp",stage);
  SDL_Surface *s=SDL_CreateRGBSurfaceFrom(p,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
  assert(s);assert(!SDL_SaveBMP(s,path));SDL_FreeSurface(s);
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);char *args[]={"test-final-depth","--config","test-play.ini",NULL};
  int r=ww_upstream_main(3,args);assert(done);
  puts("Akuma: punch return, timed guard, counter damage, stomp bounce and recovery passed.");return r;
}
