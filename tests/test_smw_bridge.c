/* Link against the actual patched SMW objects. These tests exercise real RAM,
 * clipping, damage/death routines and sprite status transitions without assets.
 * They do not claim to boot a complete level or validate the PPU output. */
#include "ww_smw.h"
#include "ww_targets.h"
#include "ww_score.h"
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
static void test_manual_pickup(void) {
  const int ids[]={4,0xf,0x3e,0x80}; /* Shell, stunned Goomba, switch, key. */
  const uint32_t inputs[]={128,1024,2,1024|2,128|1024|2};
  for(unsigned i=0;i<sizeof(ids)/sizeof(ids[0]);i++)
    for(unsigned j=0;j<sizeof(inputs)/sizeof(inputs[0]);j++) {
      enter();uint32_t b=inputs[j];assert(ww_before_frame(&b));
      bool grab=(inputs[j]&(1024|2))==(1024|2);
      if(inputs[j]&128)assert(b&2); /* Automatic running still works. */
      assert(ww_pickup_held()==grab);
      if(grab)assert(ww_fight_state()->move==FC_IDLE && !ww_fight_state()->guarding);
      enemy(0,ids[i]);spr_current_slotid=0;spr_current_status[0]=9;
      io_controller_hold1=0x40; /* Run must never substitute for the chord. */
      CheckPlayerToNormalSpriteColl_01AA42(0);
      assert((spr_current_status[0]==11)==grab);
    }
  enter();player_riding_yoshi_flag=1;frame(1024|2);
  assert(!ww_yoshi_tongue_pressed());
  frame(2);assert(ww_fight_state()->move==FC_IDLE); /* Release Guard first. */
  frame(0);frame(2);assert(ww_yoshi_tongue_pressed());
  assert(ww_fight_state()->move==FC_JAB);
  puts("Pickup requires held Guard + Punch; running/guard/punch alone cannot grab, and the chord does not attack or lick.");
}
static void test_projectile_height(void) {
  for(int character=FC_RYU;character<=FC_KEN;character+=FC_KEN-FC_RYU) {
    for(int y=-32;y<=96;y+=32) {
      enter();FcFight *f=(FcFight*)ww_fight_state();fc_init(f,character);
      player_ypos=(uint16)y;player_in_air_flag=1;
      frame(1u<<9);
      for(int t=0;t<5;t++)frame(0);
      assert(f->shots[0].ttl>0);
      assert(f->shots[0].box.y==y+32-30-10);
    }
  }
}
static void test_all_targets(void) {
  int hits=0;
  for(int id=0;id<=0xc8;id++) {
    memset(g_ram,0,0x20000);ww_targets_reset();enemy(0,id);
    FcFight f;fc_init(&f,FC_RYU);FcTarget targets[WW_TARGET_COUNT];
    ww_targets_collect(&f,targets);assert(targets[0].active);
    FcBox b=targets[0].box;
    FcBody player={{b.x-30,b.y+b.h/2-20,16,32},1,true,false,false};
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
    ww_targets_collect(&f,t);fc_tick(&f,i?0:FC_SUPER,(FcBody){{100,100,16,32},1,true,false,false},t,WW_TARGET_COUNT);
    ww_targets_apply(&f,t);
  }
  assert(!ext_spr_spriteid[0] && !cluster_spr_spriteid[0]);
  printf("Hadouken hit all %d normal sprite IDs; all Chucks, extended and cluster hazards exploded.\n",hits);
}
static void test_normal_bonks(void) {
  const int ids[]={4,5,6,7,0xf,0x11,0xa2,0x46,0x91,0x35,0x80};
  for(unsigned n=0;n<sizeof(ids)/sizeof(ids[0]);n++)for(int kick=0;kick<2;kick++) {
    memset(g_ram,0,0x20000);ww_targets_reset();enemy(0,ids[n]);
    FcFight f;fc_init(&f,FC_RYU);FcTarget t[WW_TARGET_COUNT];
    ww_targets_collect(&f,t);FcBox b=t[0].box;
    FcBody body={{b.x-16,b.y+b.h-28,16,32},1,true,false,false};
    bool hit=false;
    for(int i=0;i<18;i++) {
      ww_targets_collect(&f,t);fc_tick(&f,i?0:kick?FC_KICK:FC_PUNCH,body,t,WW_TARGET_COUNT);
      if(t[0].hit){assert(t[0].hit==FC_HIT_NORMAL);hit=true;}
      ww_targets_apply(&f,t);
    }
    assert(hit && spr_current_status[0] && !f.blasts[0].ttl);
    if(ids[n]>=4 && ids[n]<=7)assert(spr_current_status[0]==9 && (spr_table15f6[0]&0x80));
    if(ids[n]==0xf || ids[n]==0x11 || ids[n]==0xa2)assert(spr_current_status[0]==9);
    if(ids[n]==0x35 || ids[n]==0x80)assert(spr_current_status[0]==8);
    if(ids[n]==4 || ids[n]==0xf) {
      /* Normals leave a usable enemy; a projectile follow-up destroys it. */
      ww_targets_collect(&f,t);assert(t[0].active);t[0].hit=FC_HIT_SPECIAL;t[0].hp=0;
      ww_targets_apply(&f,t);assert(!spr_current_status[0] && f.blasts[0].ttl);
    }
  }
  puts("Normal punches/kicks flip shells, stun Goombas/Mechakoopas, preserve objects; specials still obliterate.");
}
static void test_progression_objects(void) {
  const int ids[]={0x0e,0x2f,0x35,0x3e,0x49,0x4a,0x55,0x60,0x61,0x79,0x7b,0x80,0x8d,0xa3,0xc0};
  for(unsigned i=0;i<sizeof(ids)/sizeof(ids[0]);i++) {
    memset(g_ram,0,0x20000);ww_targets_reset();enemy(0,ids[i]);
    FcFight f;fc_init(&f,0);FcTarget t[WW_TARGET_COUNT];ww_targets_collect(&f,t);
    t[0].hit=true;t[0].hp=0;ww_targets_apply(&f,t);
    assert(spr_current_status[0]==8 && f.blasts[0].ttl);
    ww_targets_collect(&f,t);assert(!t[0].active); /* Short visual-hit cooldown. */
    for(int j=0;j<30;j++)ww_targets_collect(&f,t);
    assert(t[0].active && t[0].hp==1);
    fc_init(&f,FC_ZANGIEF);f.move=FC_THROW;
    if(ids[i]==0x35)player_riding_yoshi_flag=flag_yoshi_saved=1;
    if(ids[i]==0x80){spr_current_status[0]=11;player_carrying_something_flag1=1;}
    ww_targets_collect(&f,t);assert(t[0].active);
    f.grabbed=0;ww_grapple_sync(&f);
    if(ids[i]==0x35)assert(!player_riding_yoshi_flag);
    if(ids[i]==0x80)assert(spr_current_status[0]==9 && !player_carrying_something_flag1);
    t[0].hit=FC_HIT_SPECIAL|FC_HIT_SLAM;t[0].hp=0;ww_targets_apply(&f,t);
    assert(!spr_current_status[0]); /* Piledrivers intentionally remove safeguards. */
    if(ids[i]==0x35)assert(!flag_yoshi_saved);
  }
}
static void test_swinging_balls(void) {
  const int expected[4][2]={{0,80},{80,0},{0,-80},{-80,0}};
  for(int phase=0;phase<4;phase++) {
    memset(g_ram,0,0x20000);ww_targets_reset();enemy(0,0x9e);
    SetSprXYPos(0,200,160);spr_table187b[0]=80;
    FcFight f;fc_init(&f,FC_RYU);FcTarget t[WW_TARGET_COUNT];
    spr_table187b[0]=0;ww_targets_collect(&f,t);FcBox pivot=t[0].box;
    spr_table187b[0]=80;spr_table1602[0]=(phase*128)&255;spr_table151c[0]=(phase*128)>>8;
    ww_targets_collect(&f,t);FcBox ball=t[0].box;
    assert(ball.x==pivot.x+expected[phase][0] && ball.y==pivot.y+expected[phase][1]);
    FcBody b={{ball.x-48,ball.y+ball.h/2-2,16,32},1,true,false,false};
    f.meter=100;
    for(int i=0;i<95;i++) {
      ww_targets_collect(&f,t);fc_tick(&f,i?0:FC_SUPER,b,t,WW_TARGET_COUNT);ww_targets_apply(&f,t);
    }
    assert(!spr_current_status[0]);assert(GetSprXPos(0)==200 && GetSprYPos(0)==160);
  }
}
static void test_mount_and_perfect(void) {
  enter();enemy(0,0x35);enemy(1,0x0f);SetSprXYPos(1,160,114);player_riding_yoshi_flag=1;
  FcFight f;fc_init(&f,0);FcTarget t[WW_TARGET_COUNT];
  for(int i=0;i<50;i++) {
    ww_targets_collect(&f,t);assert(!t[0].active);
    fc_tick(&f,i?0:FC_SPECIAL,(FcBody){{100,100,16,32},1,true,false,false},t,WW_TARGET_COUNT);
    ww_targets_apply(&f,t);
  }
  assert(spr_current_status[0]==8 && !spr_current_status[1]);
  for(int i=0;i<20;i++){uint32_t input=i%2?0:128;ww_before_frame(&input);assert(!ww_yoshi_tongue_pressed());}
  frame(2);assert(ww_yoshi_tongue_pressed());frame(2);assert(!ww_yoshi_tongue_pressed());
  enter();timer_end_level=255;assert(ww_perfect_clear());
  enter();assert(ww_take_damage(12,1));assert(ww_powerup(0x74));
  assert(ww_fight_state()->hp==88 && ww_fight_state()->meter==208);
  timer_end_level=255;assert(!ww_perfect_clear()); /* A pickup never erases a hit. */
  enter();timer_end_level=255;assert(ww_perfect_clear());
  enter();frame(1u<<10);assert(ww_take_damage(12,1));
  assert(ww_fight_state()->hp==100 && ww_fight_state()->parries==1);
  timer_end_level=255;assert(ww_perfect_clear()); /* A parry preserves PERFECT. */
}
static void test_scenery(void) {
  extern const uint8 *g_asset_ptrs[];
  static const LmFeatures vanilla={0};g_asset_ptrs[177]=(const uint8*)&vanilla;
  for(int solid=0;solid<2;solid++) {
    enter();misc_screens_in_lvl=1;FcFight f;fc_init(&f,0);
    unsigned p=GetLevelLayoutPtr_Horizontal(0)+0x60+8;
    g_ram[p]=solid?0x30:0x41;g_ram[p+0x10000]=solid?1:0;
    f.shots[0]=(FcShot){.box={108,92,20,20},.vx=4,.ttl=20,.damage=1,.pierce=1};
    ww_scenery_hits(&f);
    assert(f.shots[0].ttl==20 && f.shots[0].scenery_hit==!!solid);
    assert(!!f.blasts[0].ttl==!!solid);
  }
}
static void test_combo_score(void) {
  enter();FcFight f;fc_init(&f,FC_BISON);f.move=FC_SUPER_MOVE;f.super_level=3;
  player_current_life_count=4;
  FcTarget t[12];for(int k=0;k<12;k++)t[k]=(FcTarget){{118,110,16,20},1,true,false,0,0};
  fc_tick(&f,0,(FcBody){{100,100,16,32},1,true,false,false},t,12);ww_combat_score(&f);
  assert(ww_score()==16400 && misc_1up_handler==12);
  /* Rendering every popup past its original award time must not pay twice. */
  for(int i=0;i<50;i++)ProcessScoreSprites();
  assert(ww_score()==16400 && misc_1up_handler==12);
  player_current_life_count=97;misc_1up_handler=0;ww_combat_score(&f);
  assert(misc_1up_handler==1); /* Never wrap the native life byte. */
  player_mario_score_lo=0x3d;player_mario_score_mid=0x42;player_mario_score_hi=0x0f;
  ww_combat_score(&f);assert(ww_score()==9999990);
}
static void test_psycho_exits(void) {
  for(int vertical=0;vertical<2;vertical++)for(int cancel=0;cancel<2;cancel++) {
    enter();misc_screens_in_lvl=2;misc_level_layout_flags=vertical;
    for(int x=0;x<512;x+=16) {
      unsigned p=(vertical?GetLevelLayoutPtr_Vertical(1)+((x>>8)<<8):GetLevelLayoutPtr_Horizontal(x>>8)+256)+0x90+((x&255)>>4);
      g_ram[p]=0;g_ram[p+0x10000]=1; /* Ground at Y=400. */
    }
    FcFight *f=(FcFight*)ww_fight_state();fc_init(f,FC_BISON);
    f->move=FC_SUPER_MOVE;f->super_level=3;f->move_tick=10;
    player_xpos=100;player_ypos=320;
    for(int i=0;i<100 && f->move;i++) {
      if(cancel && i==4)f->previous=FC_GUARD;
      assert(ww_psycho_position());
    }
    assert(f->move==FC_IDLE && player_ypos==368 && player_xpos<512);
    assert(!ww_psycho_collision() && f->invuln==90);
  }
  enter();enemy(0,0x29);spr_table00c2[0]=0;frame(0);
  FcFight *f=(FcFight*)ww_fight_state();fc_init(f,FC_BISON);f->meter=300;
  frame(2048);assert(f->flash && !f->move_tick);
  for(int i=0;i<25;i++)frame(0);
  assert(!ww_opponent()->hp && spr_table151c[0]==4);
}
static void test_psycho_camera(void) {
  for(int vertical=0;vertical<2;vertical++) {
    enter();misc_screens_in_lvl=14;misc_level_layout_flags=vertical;
    l1_l2_scroll_spr_spriteid[0]=12;
    FcFight *f=(FcFight*)ww_fight_state();fc_init(f,FC_BISON);
    f->move=FC_SUPER_MOVE;f->super_level=3;f->move_tick=1198;
    player_xpos=100;player_ypos=160;
    for(int i=0;i<500;i++) {
      mirror_current_layer1_xpos=vertical?80:i;
      mirror_current_layer1_ypos=vertical?500-i:160;
      if(i>100)f->previous=FC_GUARD; /* No visible floor: safely wait over a gap. */
      assert(ww_psycho_position() && fc_psycho_flight(f));
      int x=player_xpos-mirror_current_layer1_xpos,y=player_ypos-mirror_current_layer1_ypos;
      assert(x>=24 && x<=208 && y>=48 && y<=176);
      assert(f->move_tick<1200 && !player_current_state);
    }
    /* A floor enters the camera; cancellation must land there, never back at
     * the starting position or on a platform above/below the visible room. */
    int ground=(mirror_current_layer1_ypos+175)&~15;
    for(int x=mirror_current_layer1_xpos;x<mirror_current_layer1_xpos+256;x+=16) {
      if(vertical && x>=512)break;
      unsigned p=vertical?GetLevelLayoutPtr_Vertical(ground>>8)+((x>>8)<<8)+(ground&0xf0)+((x&255)>>4):
        GetLevelLayoutPtr_Horizontal(x>>8)+((ground>>8)<<8)+(ground&0xf0)+((x&255)>>4);
      g_ram[p]=0;g_ram[p+0x10000]=1;
    }
    assert(ww_psycho_position() && !fc_psycho_flight(f));
    assert(player_ypos==ground-32 && player_xpos>=mirror_current_layer1_xpos+16);
  }
}
static void test_practice_menu(void) {
  assert(!ww_debug_enabled());enter();
  frame(8);frame(0);frame(32);frame(0);frame(32);frame(0);frame(8);
  assert(!ww_practicing() && !fc_audio_selection_active() && frame(0)); /* Two choices: wraps to Resume. */
  ww_set_debug(true);
  enter();player_mario_score_lo=12;player_current_life_count=4;
  frame(8);frame(0);frame(32);frame(0);frame(32);frame(0);frame(8);
  assert(ww_practicing() && misc_game_mode==0x0f && ww_fight_state()->meter==300);
  assert(misc_subscreen_exit_entrance_number_lo[0]==0xc7 && ow_players_map[0]==1);
  misc_game_mode=0x14;frame(0);assert(!ow_players_map[0]);
  player_mario_score_lo=90;
  DamagePlayer_Kill();for(int i=0;i<32;i++)frame(0);frame(8);
  misc_game_mode=0x0e;frame(0);
  assert(!ww_practicing() && misc_game_mode==0x0e && fc_audio_selection_active());
  assert(ww_score()==120 && player_current_life_count==4);
  frame(0);frame(8);frame(0); /* Choose the fighter, then stay on the map. */
  misc_game_mode=0x14;player_current_state=0;frame(0);
  assert(ww_fight_state()->hp==100 && ww_fight_state()->meter==100);
  DamagePlayer_Kill();for(int i=0;i<32;i++)frame(0);frame(4);
  misc_game_mode=0x0e;frame(0);
  assert(!ww_practicing() && ww_score()==120 && player_current_life_count==4);
  ww_set_debug(false);
}
static void test_meter_pickups(void) {
  const int sprites[]={0x74,0x75,0x77},gains[]={100,200,300},meters[]={0,75,250,300};
  for(int item=0;item<3;item++)for(int m=0;m<4;m++)for(int power=0;power<4;power++) {
    enter();FcFight *f=(FcFight*)ww_fight_state();f->hp=60;f->meter=meters[m];
    player_current_power_up=power;player_current_item_box=players_item_box[0]=players_item_box[1]=4;
    enemy(11,sprites[item]);spr_decrementing_table1540[11]=0;
    SprXXX_PowerUps_01C4BF(11); /* Actual collection, score and sound path. */
    int expected=meters[m]+gains[item];if(expected>300)expected=300;
    assert(f->meter==expected && f->hp==60 && !spr_current_status[11]);
    assert(!player_current_item_box && !players_item_box[0] && !players_item_box[1]);
    assert(player_current_power_up==1 && !player_current_state && !flag_sprites_locked);
    assert(io_sound_ch1==10);
  }
  enter();enemy(11,0x76);spr_decrementing_table1540[11]=0;SprXXX_PowerUps_01C4BF(11);
  assert(timer_star_power==255 && ww_fight_state()->meter==100);
  assert(!ww_powerup(0x78)); /* 1UPs still use the original handler. */
  enter();enemy(10,0xa0);player_current_item_box=players_item_box[0]=players_item_box[1]=1;
  frame(1024|2);assert(!player_current_item_box && !players_item_box[0] && !players_item_box[1]);
  assert(!spr_current_status[11] && spr_spriteid[10]==0xa0);
  puts("Power-ups: mushroom +1, flower +2, feather +3 bars; cap, no reserve or transformations passed.");
}
static void test_castle_plungers(void) {
  for(int id=0xac;id<=0xad;id++)for(int height=8;height<=72;height+=32) {
    memset(g_ram,0,0x20000);ww_targets_reset();enemy(0,id);SetSprXYPos(0,140,80);
    FcFight f;fc_init(&f,0);FcTarget t[WW_TARGET_COUNT];
    FcBody player={{102,80+height-20,16,32},1,false,false,false};
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
  assert(spr_current_status[0]==9); /* A jab leaves a stunned, usable Goomba. */
  assert(ww_fight_state()->meter==112);
  for(int i=0;i<30;i++)frame(0);
  enemy(0,0x0f);frame(2);for(int i=0;i<5;i++)frame(0);
  assert(spr_current_status[0]==9 && ww_fight_state()->meter==124); /* Recycled slot. */

  enter();enemy(0,0x0f);spr_current_slotid=0;
  DamagePlayer_Hurt();assert(player_current_state==0 && player_current_power_up==1);
  assert(ww_fight_state()->hp==88 && timer_player_hurt==60);
  DamagePlayer_Hurt();assert(ww_fight_state()->hp==88);
  for(int i=0;i<8;i++)frame(0);
  ww_velocity();assert((int8)player_xspeed<0 && (int8)player_yspeed<0);
  /* Pipe movement animates, but held attacks cannot restart combat. */
  player_current_state=5;int meter=ww_fight_state()->meter,hp=ww_fight_state()->hp;
  assert(frame(1u<<11));assert(ww_fight_state()->move==FC_IDLE);
  assert(ww_fight_state()->meter==meter && ww_fight_state()->hp==hp);
  player_current_state=0;
  DamagePlayer_Kill();assert(player_current_state==9 && ww_fight_state()->hp==0);
  assert(io_music_ch1==0); /* Original Mario death jingle is replaced. */
  for(int i=0;i<32;i++)frame(0);
  frame(8); /* Accept the new arcade continue before the map transition. */
  misc_game_mode=0x0e;assert(!frame(0));assert(fc_audio_selection_active());
  assert(!frame(128));assert(!frame(0));assert(!frame(1)); /* Pick Honda, confirm with Jump. */
  assert(!fc_audio_selection_active() && ww_fight_state()->character==FC_HONDA);
  for(int i=0;i<45;i++) {
    uint32_t held=1;assert(ww_before_frame(&held));assert(!held); /* Never auto-enter. */
  }
  assert(frame(0));uint32_t press=1;assert(ww_before_frame(&press) && press==1);
  player_current_state=0;flag_sprites_locked=0;timer_player_hurt=0;
  misc_game_mode=0x14;assert(frame(0));
  assert(ww_fight_state()->hp==125 && ww_fight_state()->meter==100);
  assert(!frame(8));assert(!frame(0));assert(!frame(8)); /* Controller pause/resume. */
  assert(frame(0));
  /* A turn + attack uses this frame's direction and preserves held air jump. */
  enter();player_in_air_flag=1;uint32_t input=(1u<<6)|(1u<<9)|1;
  ww_before_frame(&input);assert(ww_fight_state()->facing==-1);
  assert((input&1) && !ww_fight_state()->override_x);
  test_projectile_height();test_normal_bonks();test_all_targets();test_progression_objects();test_swinging_balls();test_castle_plungers();test_bosses();
  test_mount_and_perfect();test_scenery();test_combo_score();test_psycho_exits();test_psycho_camera();test_practice_menu();
  test_meter_pickups();
  test_manual_pickup();
  fc_audio_shutdown();SDL_Quit();puts("SMW bridge: broad targeting, progression objects, boss deaths, slot reuse, hurt, movement, transitions, KO and pause passed.");
  return 0;
}
