/* Link against the actual patched SMW objects. These tests exercise real RAM,
 * clipping, damage/death routines and sprite status transitions without assets.
 * They do not claim to boot a complete level or validate the PPU output. */
#include "ww_smw.h"
#include "ww_targets.h"
#include "audio.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool frame(uint32_t buttons) { return ww_before_frame(&buttons); }
static void enter(void) {
  memset(g_ram,0,0x20000);ww_init();misc_game_mode=0x14;
  player_xpos=100;player_ypos=100;player_facing_direction=1;
  assert(!frame(0));assert(!frame(8)); /* Actual select + confirm paths. */
  assert(frame(0));assert(ww_enabled());
}
static void enemy(int slot,int id) {
  spr_spriteid[slot]=(uint8)id;spr_current_status[slot]=8;
  InitializeNormalSpriteRAMTables((uint8)slot);
  SetSprXYPos((uint8)slot,118,114);
}
static void test_all_targets(void) {
  int hits=0;
  for(int id=0;id<=0xc8;id++) {
    memset(g_ram,0,0x20000);ww_targets_reset();enemy(0,id);
    FcFight f;fc_init(&f,FC_RYU);FcTarget targets[WW_TARGET_COUNT];
    ww_targets_collect(&f,targets);assert(targets[0].active);
    FcBox b=targets[0].box;
    FcBody player={{b.x-30,b.y+b.h/2-20,16,32},1,true};
    bool hit=false;
    for(int tick=0;tick<40;tick++) {
      ww_targets_collect(&f,targets);
      fc_tick(&f,tick?0:FC_SPECIAL,player,targets,WW_TARGET_COUNT);
      hit|=targets[0].hit;ww_targets_apply(&f,targets);
    }
    if(!hit)fprintf(stderr,"No hadouken hit for sprite %02x\n",id);
    assert(hit);++hits;
    if(id==0x46 || (id>=0x91 && id<=0x98))assert(!spr_current_status[0]);
  }
  /* Every Chuck variant also gets obliterated by a level-one super. */
  for(int id=0x91;id<=0x98;id++) {
    enter();enemy(0,id);frame(1u<<11);
    for(int t=0;t<65;t++)frame(0);
    assert(!spr_current_status[0]);
  }
  memset(g_ram,0,0x20000);ww_targets_reset();
  ext_spr_spriteid[0]=2;ext_spr_xpos_lo[0]=130;ext_spr_ypos_lo[0]=120;
  cluster_spr_spriteid[0]=4;cluster_spr_xpos_lo[0]=170;cluster_spr_ypos_lo[0]=120;
  FcFight f;fc_init(&f,0);f.meter=100;FcTarget t[WW_TARGET_COUNT];
  for(int i=0;i<90;i++) {
    ww_targets_collect(&f,t);fc_tick(&f,i?0:FC_SUPER,(FcBody){{100,100,16,32},1,true},t,WW_TARGET_COUNT);
    ww_targets_apply(&f,t);
  }
  assert(!ext_spr_spriteid[0] && !cluster_spr_spriteid[0]);
  printf("Hadouken hit all %d normal sprite IDs; all Chucks, extended and cluster hazards exploded.\n",hits);
}
static void test_progression_objects(void) {
  const int ids[]={0x0e,0x2f,0x35,0x3e,0x49,0x4a,0x55,0x60,0x61,0x79,0x7b,0x80,0x8d,0xc0};
  for(unsigned i=0;i<sizeof(ids)/sizeof(ids[0]);i++) {
    memset(g_ram,0,0x20000);ww_targets_reset();enemy(0,ids[i]);
    FcFight f;fc_init(&f,0);FcTarget t[WW_TARGET_COUNT];ww_targets_collect(&f,t);
    t[0].hit=true;t[0].hp=0;ww_targets_apply(&f,t);
    assert(spr_current_status[0]==8 && f.blasts[0].ttl);
    ww_targets_collect(&f,t);assert(!t[0].active); /* Short visual-hit cooldown. */
    for(int j=0;j<30;j++)ww_targets_collect(&f,t);
    assert(t[0].active && t[0].hp==1);
  }
}
static void test_castle_plungers(void) {
  for(int id=0xac;id<=0xad;id++)for(int height=8;height<=72;height+=32) {
    memset(g_ram,0,0x20000);ww_targets_reset();enemy(0,id);SetSprXYPos(0,140,80);
    FcFight f;fc_init(&f,0);FcTarget t[WW_TARGET_COUNT];
    FcBody player={{102,80+height-20,16,32},1,false};
    for(int tick=0;tick<30;tick++) {
      ww_targets_collect(&f,t);fc_tick(&f,tick?0:FC_SPECIAL,player,t,WW_TARGET_COUNT);ww_targets_apply(&f,t);
    }
    assert(spr_current_status[0]==0);
  }
  memset(g_ram,0,0x20000);ww_targets_reset();enemy(0,0x89);
  FcFight f;fc_init(&f,0);FcTarget t[WW_TARGET_COUNT];ww_targets_collect(&f,t);
  t[0].hp=0;t[0].hit=true;ww_targets_apply(&f,t);
  assert(!spr_current_status[0] && GetSprYPos(0)==0);
}
static void finish_target(FcFight *f,int k) {
  FcTarget t[WW_TARGET_COUNT];ww_targets_collect(f,t);assert(t[k].active);
  t[k].hp=0;t[k].hit=true;ww_targets_apply(f,t);
}
static void test_bosses(void) {
  FcFight f;FcTarget t[WW_TARGET_COUNT];
  for(int kind=0;kind<7;kind++) {
    memset(g_ram,0,0x20000);ww_targets_reset();enemy(0,0x29);spr_table00c2[0]=kind;
    fc_init(&f,0);ww_targets_collect(&f,t);ww_targets_apply(&f,t);
    assert(ww_opponent()->hp==18 && ww_opponent()->max_hp==18);
    t[0].hp-=3;t[0].hit=true;ww_targets_apply(&f,t);assert(ww_opponent()->hp==15);
    finish_target(&f,0);assert(spr_current_status[0]==8);
    assert(!ww_opponent()->hp && (f.events&FC_SND_BOSS_KO));
    if(kind<3)assert(spr_table151c[0]==4 && spr_table1626[0]==3);
    else if(kind<5)assert(spr_table160e[0]);
    else assert(spr_table151c[0]==5 && spr_table1534[0]==3);
    f.events=0;ww_targets_collect(&f,t);ww_targets_apply(&f,t);assert(!(f.events&FC_SND_BOSS_KO));
  }
  memset(g_ram,0,0x20000);ww_targets_reset();fc_init(&f,0);
  for(int k=4;k<8;k++)enemy(k,0xa9);
  ww_targets_collect(&f,t);ww_targets_apply(&f,t);assert(ww_opponent()->hp==24);
  for(int k=4;k<8;k++) {
    finish_target(&f,k);assert(spr_current_status[k]==8 && spr_table151c[k]==1);
    assert(ww_opponent()->hp==(7-k)*6);
    if(k<7)assert(!(f.events&FC_SND_BOSS_KO));
  }
  assert(f.events&FC_SND_BOSS_KO);
  memset(g_ram,0,0x20000);ww_targets_reset();enemy(0,0xc5);fc_init(&f,0);
  finish_target(&f,0);assert(spr_table00c2[0]==6 && spr_current_status[0]==8);
  memset(g_ram,0,0x20000);ww_targets_reset();enemy(0,0xa0);fc_init(&f,0);
  finish_target(&f,0);assert(spr_table151c[0]==4 && spr_current_status[0]==8);
}
int main(void) {
  assert(SDL_Init(SDL_INIT_AUDIO)==0);
  enter();enemy(0,0x0f);
  frame(2);for(int i=0;i<5;i++)frame(0);
  assert(spr_current_status[0]==0); /* Goomba explodes from the shared jab. */
  assert(ww_fight_state()->meter==112);
  for(int i=0;i<30;i++)frame(0);
  enemy(0,0x0f);frame(2);for(int i=0;i<5;i++)frame(0);
  assert(spr_current_status[0]==0 && ww_fight_state()->meter==124); /* Recycled slot. */

  enter();enemy(0,0x0f);spr_current_slotid=0;
  DamagePlayer_Hurt();assert(player_current_state==0 && player_current_power_up==1);
  assert(ww_fight_state()->hp==88 && timer_player_hurt==60);
  DamagePlayer_Hurt();assert(ww_fight_state()->hp==88);
  for(int i=0;i<8;i++)frame(0);
  ww_velocity();assert((int8)player_xspeed<0 && (int8)player_yspeed<0);
  /* Entering a pipe must freeze combat without suppressing SMW's transition. */
  player_current_state=5;int tick=(int)ww_fight_state()->tick;
  assert(frame(1u<<11));assert((int)ww_fight_state()->tick==tick);
  player_current_state=0;
  DamagePlayer_Kill();assert(player_current_state==9 && ww_fight_state()->hp==0);
  assert(io_music_ch1==0); /* Original Mario death jingle is replaced. */
  for(int i=0;i<32;i++)frame(0);
  frame(8); /* Accept the new arcade continue before the map transition. */
  misc_game_mode=0x0e;assert(frame(0));
  player_current_state=0;flag_sprites_locked=0;timer_player_hurt=0;
  misc_game_mode=0x14;assert(frame(0));
  assert(ww_fight_state()->hp==100 && ww_fight_state()->meter==100);
  assert(!frame(8));assert(!frame(0));assert(!frame(8)); /* Controller pause/resume. */
  assert(frame(0));
  /* A turn + attack uses this frame's direction and preserves held air jump. */
  enter();player_in_air_flag=1;uint32_t input=(1u<<6)|(1u<<9)|1;
  ww_before_frame(&input);assert(ww_fight_state()->facing==-1);
  assert((input&1) && !ww_fight_state()->override_x);
  test_all_targets();test_progression_objects();test_castle_plungers();test_bosses();
  fc_audio_shutdown();SDL_Quit();puts("SMW bridge: broad targeting, progression objects, boss deaths, slot reuse, hurt, movement, transitions, KO and pause passed.");
  return 0;
}
