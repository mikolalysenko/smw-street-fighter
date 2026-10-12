#include "ww_smw.h"
#include "ww_targets.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
static int tick,map_ticks,base,warped,n,done,keyx,keyy,holex,holey,stage,low_y,min_y,held,drop;
enum { GRAB=1024|2 };
int ww_upstream_main(int,char**);
static void pickup_checks(void) {
  /* Exercise original throw-block code with run held, and the native held
   * sprite routine for both ways of releasing the new chord. */
  const uint32_t inputs[]={128,1024,2,GRAB,GRAB|128};
  for(unsigned i=0;i<sizeof(inputs)/sizeof(inputs[0]);i++) {
    for(int k=0;k<12;k++)spr_current_status[k]=0;
    player_carrying_something_flag1=player_carrying_something_flag2=0;
    fc_init((FcFight*)ww_fight_state(),FC_RYU);
    uint32_t b=inputs[i];assert(ww_before_frame(&b));
    unsigned offset=GetLevelLayoutPtr_Horizontal(0)+0x88;
    g_ram[offset]=0x2e;g_ram[offset+0x10000]=1;
    blocks_xpos=blocks_ypos=128;misc_current_layer_being_processed=0;
    io_controller_press1=io_controller_hold1=0x40;
    RunPlayerBlockCode_CheckIfGrabbingThrowBlock(46);
    bool grab=(inputs[i]&GRAB)==GRAB;
    assert(g_ram[offset]==(grab?0x25:0x2e));
    int carried=0;
    for(int k=0;k<12;k++)carried+=spr_current_status[k]==11 && spr_spriteid[k]==0x53;
    assert(carried==grab);
  }
  for(int direction=0;direction<3;direction++)for(int release=0;release<2;release++) {
    for(int k=0;k<12;k++)spr_current_status[k]=0;
    player_carrying_something_flag1=player_carrying_something_flag2=0;
    fc_init((FcFight*)ww_fight_state(),FC_RYU);
    player_xpos=player_current_xpos=128;player_ypos=player_current_ypos=96;
    player_xspeed=player_yspeed=player_in_air_flag=0;player_facing_direction=1;
    uint32_t b=GRAB;assert(ww_before_frame(&b));
    spr_spriteid[0]=4;spr_current_status[0]=11;InitializeNormalSpriteRAMTables(0);
    spr_current_slotid=0;SetSprXYPos(0,128,96);
    io_controller_hold1=0;SprStatus0B_Carried_019F9B(0);
    assert(spr_current_status[0]==11); /* Chord holds even when idle/no native run. */
    b=release?2:1024;assert(ww_before_frame(&b));
    io_controller_hold1=0x40|(direction==1?4:direction==2?8:1);
    SprStatus0B_Carried_019F9B(0);
    assert(spr_current_status[0]==(direction==0?10:9));
    if(direction==0)assert((int8)spr_xspeed[0]>0);
    if(direction==1)assert(spr_yspeed[0]==0);
    if(direction==2)assert((int8)spr_yspeed[0]<0);
  }
  player_carrying_something_flag1=player_carrying_something_flag2=0;
  puts("Native pickup: throw blocks require the chord; releasing either button throws, drops or tosses upward even with run held.");
}
uint32_t ww_test_input(void) {
  ++tick;
  if(done || tick>9000){SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return 0;}
  if(!warped) {
    if(misc_game_mode==0x0e){++map_ticks;return map_ticks<240?(map_ticks%30<20?128:0):map_ticks%40==0?1:0;}
    if(misc_game_mode==0x14 && misc_intro_level_flag)return tick%45==0?1:0;
    if(misc_game_mode==0x14 && !misc_intro_level_flag && ++base>45) {
      pickup_checks();
      for(int i=0;i<32;i++)misc_subscreen_exit_entrance_number_lo[i]=0x15;
      ow_players_map[0]=0;flag_use_secondary_entrance=flag_show_player_start=0;
      player_current_state=timer_end_level=0;counter_sublevels_entered=1;misc_game_mode=0x0f;warped=1;return 0;
    }
    return misc_game_mode==0x14?0:tick%45==0?8:tick%45==22?1:0;
  }
  if(misc_game_mode!=0x14)return 0;
  ++n;if(!stage && n<30)return 0;
  if(!stage) {
    const uint8 *p=GetSpriteListPtr();
    for(int i=1;p[i]!=255;i+=3)if(p[i+2]==0x80 || p[i+2]==0x0e) {
      int x=(((p[i]<<3)&16)|(p[i+1]&15))*256+(p[i+1]&240),y=(p[i]&240)+((p[i]&1)<<8);
      printf("DP1 sprite %02x at %d,%d\n",p[i+2],x,y);
      if(p[i+2]==0x80){keyx=x;keyy=y;}else{holex=x;holey=y;}
    }
    assert(keyx && holex);stage=1;n=0;
    /* Position below the original secret platform. Terrain, key and keyhole
     * remain native; the charged jump must get up there with green blocks off. */
    player_xpos=keyx-128;player_ypos=352;player_xspeed=player_yspeed=0;
    player_in_air_flag=player_ducking_flag=0;
    mirror_current_layer1_xpos=misc_layer1_pos[0].x=keyx-160;
    mirror_current_layer1_ypos=misc_layer1_pos[0].y=176;
    player_on_screen_pos_x=player_xpos-mirror_current_layer1_xpos;
    player_on_screen_pos_y=player_ypos-mirror_current_layer1_ypos;
    player_sub_xpos=player_sub_ypos=player_blocked_flags=0;
    flag_layer1_vert_scroll_level_setting=1;
    for(int i=0;i<12;i++)spr_current_status[i]=0;
    const uint8 *list=GetSpriteListPtr();
    for(int i=1,j=0;list[i]!=255;i+=3,j++)if(list[i+2]==0x80 || list[i+2]==0x0e) {
      int x=list[i+2]==0x80?keyx:holex;
      sprites_load_status[j]=0;LoadOneSprite(list+i,j,(PointU16){x,0});
    }
    fc_init((FcFight*)ww_fight_state(),FC_RYU);low_y=min_y=player_ypos;return 0;
  }
  if(player_current_state==9 || ww_fight_state()->hp<=0) {
    fprintf(stderr,"Carry death stage=%d n=%d xy=%d,%d cam=%d,%d screen=%d,%d blocked=%x charge=%d boost=%d\n",stage,n,player_xpos,player_ypos,mirror_current_layer1_xpos,mirror_current_layer1_ypos,player_on_screen_pos_x,player_on_screen_pos_y,player_blocked_flags,ww_fight_state()->jump_charge,ww_fight_state()->jump_boost);assert(0);
  }
  if(player_ypos<min_y)min_y=player_ypos;
  int key=-1;
  for(int i=0;i<12;i++)if(spr_spriteid[i]==0x80 && spr_current_status[i]>=8)key=i;
  /* Hold off the nearby exit while exercising hold/release/regrab. Release
   * its ordinary contact cooldown before walking into the native keyhole. */
  for(int i=0;i<12;i++)if(spr_spriteid[i]==0x0e && spr_current_status[i]>=8)spr_decrementing_table154c[i]=stage==4?0:2;
  if(timer_end_level_via_keyhole){assert(held && low_y-min_y>240);done=1;return 0;}
  assert(key>=0);
  if(stage==1) {
    if(n<65)return 32;
    if(spr_current_status[key]==11){held=1;stage=2;n=0;return GRAB;}
    return GRAB | (n<140?1:0) | (player_xpos<keyx-40 || (player_ypos<76 && player_xpos<keyx-8)?128:0);
  }
  if(stage==2) {
    /* Keep the chord held while standing, walking and jumping. */
    assert(spr_current_status[key]==11);
    assert(ww_fight_state()->move==FC_IDLE && !ww_fight_state()->guarding);
    if(n<10)return GRAB|64;
    if(n<20)return GRAB|128;
    if(n>=35 && n<45)return GRAB|1;
    if(n==100){stage=3;n=0;return 32;}
    return GRAB;
  }
  if(stage==3) {
    if(spr_current_status[key]!=11)drop=1;
    if(n<16)return 0;
    if(drop && spr_current_status[key]==11){stage=4;n=0;return GRAB;}
    return GRAB | (GetSprXPos(key)>player_xpos+12?128:64);
  }
  if(stage==4) {
    /* Back up after regrabbing, then deliberately insert the key. */
    return GRAB | (n<16?64:128);
  }
  return 0;
}
void ww_test_render(uint8_t *p,size_t pitch,int w,int h) {
  if(stage!=2 || n!=45)return;
  SDL_Surface *s=SDL_CreateRGBSurfaceFrom(p,w,h,32,(int)pitch,0xff0000,0xff00,0xff,0xff000000);
  assert(s);assert(!SDL_SaveBMP(s,"donut-secret-key.bmp"));SDL_FreeSurface(s);
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);char *args[]={"test-carry","--config","test-play.ini",NULL};
  int r=ww_upstream_main(3,args);printf("Carry final stage=%d n=%d done=%d held=%d drop=%d mode=%x keyhole=%d\n",stage,n,done,held,drop,misc_game_mode,timer_end_level_via_keyhole);assert(done && held && drop);printf("Donut Plains 1: climbed %d pixels, held through walking/jumping, released/regrabbed and unlocked native keyhole.\n",low_y-min_y);return r;
}
