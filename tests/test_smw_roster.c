/* Select every fighter through the real menus, exercise normals and specials
 * through the original level loop, then let Bison finish an unmodified stage. */
#include "ww_smw.h"
#include "ww_targets.h"
#include "audio.h"
#include "assets.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
static int tick,map_ticks,level_ticks,selected,select_ticks,menu_ticks,action,action_tick;
static int home_x,home_y,started,done,flight_ticks,flight_start,flight_end;
static int saw_move,saw_hit,saw_shot,saw_air,selections;
static int flight_prev_x,flight_prev_y,goal_jump,goal_jump_air;
enum { ACTION_COUNT=17 };
static const uint32_t actions[]={0,2,256,32|2,32|256,512,16|512,32|512,2048};
int ww_upstream_main(int,char**);
uint32_t ww_test_input(void) {
  ++tick;FcFight *f=(FcFight*)ww_fight_state();
  if(done || tick>32000){SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;}
  if(fc_audio_selection_active()) {
    if(!select_ticks++)selections++;
    if(selected && select_ticks==2)return 128;
    if(select_ticks==6)return 8;
    return 0;
  }
  if(menu_ticks) {
    if(f->character==selected && select_ticks>=6) {
      menu_ticks=0;select_ticks=0;action=action_tick=0;
      assert(f->hp==fc_roster[selected].vitality);
    } else {
      int n=menu_ticks++;return n==1 || n==5?8:n==3?32:0;
    }
  }
  if(misc_game_mode==0x0e) {
    if(flight_end){done=1;return 0;}
    ++map_ticks;return map_ticks<240?(map_ticks%30<20?128:0):map_ticks%40==0?1:0;
  }
  if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
  if(misc_game_mode!=0x14)return tick%45==0?8:tick%45==22?1:0;
  if(++level_ticks<80 || !ww_enabled())return 0;
  timer_player_hurt=80;
  if(!started) {
    if(player_in_air_flag)return 0;
    home_x=player_xpos;home_y=player_ypos;started=1;select_ticks=0;
  }
  if(selected==FC_BISON && action==ACTION_COUNT) {
    if(!flight_ticks) {
      fc_clear_actions(f);f->meter=300;player_xpos=home_x;player_ypos=home_y;
      player_xspeed=player_yspeed=0;player_in_air_flag=0;flight_start=player_xpos;
    }
    ++flight_ticks;
    if(flight_ticks>30 && !timer_end_level) {
      int dx=(int)player_xpos-flight_prev_x,dy=(int)player_ypos-flight_prev_y;
      assert(dx>=-8 && dx<=8 && dy>=-8 && dy<=8); /* No goal teleport. */
      if(f->move==FC_IDLE){goal_jump++;goal_jump_air+=!!player_in_air_flag;}
    }
    flight_prev_x=player_xpos;flight_prev_y=player_ypos;
    if(flight_ticks%60==0) {
      printf("Flight %d: x=%d y=%d cam=%d screens=%d move=%d/%d boss=%d end=%d\n",flight_ticks,player_xpos,player_ypos,mirror_current_layer1_xpos,misc_screens_in_lvl,f->move,f->move_tick,ww_opponent()->active,timer_end_level);
    }
    if(timer_end_level){flight_end=1;assert(player_xpos-flight_start>1500);return 0;}
    if(flight_ticks>1800){fprintf(stderr,"Psycho stalled: x=%d y=%d move=%d mode=%d end=%d\n",player_xpos,player_ypos,f->move,misc_game_mode,timer_end_level);assert(0);}
    return flight_ticks==1?2048:0;
  }
  if(action_tick==0) {
    fc_clear_actions(f);f->meter=100;f->hp=fc_roster[selected].vitality;
    player_xpos=home_x;player_ypos=home_y;player_xspeed=player_yspeed=0;
    player_in_air_flag=player_ducking_flag=0;player_facing_direction=1;
    for(int k=0;k<12;k++)spr_current_status[k]=0;
    spr_spriteid[10]=0x46;spr_current_status[10]=8;InitializeNormalSpriteRAMTables(10);
    SetSprXYPos(10,home_x+36,home_y+6);spr_yspeed[10]=0;
    saw_move=saw_hit=saw_shot=saw_air=0;
  }
  saw_move|=f->move!=FC_IDLE;saw_hit|=!spr_current_status[10];
  saw_air|=!f->body.grounded;
  for(int i=0;i<FC_MAX_SHOTS;i++)saw_shot|=f->shots[i].ttl>0;
  if(action_tick==75) {
    if(action==4 && (selected==FC_BISON || selected==FC_DHALSIM))assert(player_xpos>=home_x+35);
    if(action && action!=9 && action!=10)assert(saw_move);
    if(action>=9)assert(saw_air);
    printf("%s action %d: move=%d hit=%d shot=%d air=%d\n",fc_roster[selected].name,action,saw_move,saw_hit,saw_shot,saw_air);
    ++action;action_tick=0;
    if(action==ACTION_COUNT && selected!=FC_BISON){++selected;menu_ticks=1;}
    return 0;
  }
  int n=action_tick++;
  if(action==0)return n<30?128:0;
  if(action>=9) {
    static const uint32_t air_actions[]={2,256,512,16|512,32|512,2048};
    uint32_t b=n>=1 && n<18?1:0;
    if(action==10 && n<35)b|=128;
    if(action>=11 && n==10)b|=air_actions[action-11];
    return b;
  }
  return n==4?actions[action]:0;
}
static void save(FcCanvas c,const char *name) {
  SDL_Surface *s=SDL_CreateRGBSurfaceFrom(c.pixels,c.w,c.h,32,c.pitch*4,0xff0000,0xff00,0xff,0xff000000);
  assert(s);assert(!SDL_SaveBMP(s,name));SDL_FreeSurface(s);
}
void ww_test_render(uint8_t *pixels,size_t pitch,int w,int h) {
  if(!started)return;
  if(action_tick==8 || action_tick==16 || action_tick==30 || (flight_ticks==90 && !flight_end)) {
    char name[80];snprintf(name,sizeof(name),"roster-%02d-%d-%02d.bmp",selected,action,action_tick);
    save((FcCanvas){(uint32_t*)pixels,w,h,(int)(pitch/4)},name);
  }
  if(timer_end_level && !flight_end)save((FcCanvas){(uint32_t*)pixels,w,h,(int)(pitch/4)},"psycho-goal.bmp");
  if(selected==FC_BISON && action==0 && action_tick==12) {
    uint32_t sheet[768*576]={0};FcCanvas c={sheet,768,576,768};
    for(int id=0;id<FC_ROSTER_COUNT;id++) {
      int x=(id%6)*128,y=(id/6)*288;fc_text(c,x,y,fc_roster[id].name,0xffffff,1);
      int poses[]={FC_POSE_IDLE,FC_POSE_PUNCH,FC_POSE_CROUCH_KICK,FC_POSE_JUMP};
      for(int p=0;p<4;p++)fc_sprite(c,id,poses[p],8,x+64,y+66+p*68,1,1,2,0);
    }
    save(c,"roster-art.bmp");
  }
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);char *args[]={"test-roster","--config","test-play.ini","--debug",NULL};
  int result=ww_upstream_main(4,args);
  printf("Roster selected %d/12, Bison flew %d frames, native goal %d, world map %d\n",selections,flight_ticks,flight_end,done);
  assert(selections==FC_ROSTER_COUNT && flight_end && done && goal_jump>=25 && goal_jump_air>=25);return result;
}
