#include "fight.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static FcBody player={{100,100,16,32},1,true};
static void tick(FcFight *f,uint32_t buttons,FcTarget *targets,int n) {
  fc_tick(f,buttons,player,targets,n);
}
static void frames(FcFight *f,int n,FcTarget *t,int count) {
  for(int i=0;i<n;i++)tick(f,0,t,count);
}
static void motion(FcFight *f,int facing) {
  player.facing=facing;
  tick(f,FC_DOWN,NULL,0);
  tick(f,FC_DOWN|(facing>0?FC_RIGHT:FC_LEFT),NULL,0);
  tick(f,facing>0?FC_RIGHT:FC_LEFT,NULL,0);
}
static void test_motions(void) {
  FcFight f;
  for(int facing=-1;facing<=1;facing+=2) {
    fc_init(&f,FC_RYU);motion(&f,facing);tick(&f,FC_PUNCH,NULL,0);
    assert(f.move==FC_PROJECTILE);
    fc_init(&f,FC_RYU);f.meter=200;motion(&f,facing);motion(&f,facing);tick(&f,FC_PUNCH,NULL,0);
    assert(f.move==FC_SUPER_MOVE && f.super_level==2 && f.meter==0 && f.flash);
    fc_init(&f,FC_RYU);motion(&f,facing);frames(&f,25,NULL,0);tick(&f,FC_PUNCH,NULL,0);
    assert(f.move==FC_JAB);
  }
  player.facing=1;
  fc_init(&f,FC_RYU);tick(&f,FC_RIGHT,NULL,0);tick(&f,FC_DOWN,NULL,0);
  tick(&f,FC_DOWN|FC_RIGHT|FC_PUNCH,NULL,0);assert(f.move==FC_RISING);
  fc_init(&f,FC_RYU);tick(&f,FC_DOWN,NULL,0);tick(&f,FC_RIGHT|FC_PUNCH,NULL,0);
  assert(f.move==FC_JAB); /* missing diagonal must not count */
}
static void test_super_meter(void) {
  FcFight f;
  for(int level=1;level<=3;level++) {
    fc_init(&f,FC_RYU);f.meter=level*100+9;tick(&f,FC_SUPER,NULL,0);
    assert(f.move==FC_SUPER_MOVE && f.super_level==level && f.meter==9);
    assert(f.move_tick==0 && f.frozen);
    frames(&f,18,NULL,0);assert(f.move_tick==0);tick(&f,0,NULL,0);assert(f.move_tick==1);
  }
  fc_init(&f,0);f.meter=99;tick(&f,FC_SUPER,NULL,0);assert(f.move==FC_IDLE && f.meter==99);
  fc_meter(&f,1000);assert(f.meter==300);fc_meter(&f,-1000);assert(f.meter==0);
}
static void test_hits(void) {
  FcFight f;fc_init(&f,FC_RYU);
  FcTarget t={{118,110,16,20},10,true,false,0,0};
  tick(&f,FC_PUNCH,&t,1);frames(&f,40,&t,1);
  assert(t.hp==9 && f.meter==12); /* One hit per active attack, not per frame. */
  tick(&f,FC_PUNCH,&t,1);frames(&f,10,&t,1);assert(t.hp==8);
  FcTarget friendly={{118,110,16,20},4,false,false,0,0};
  fc_init(&f,0);tick(&f,FC_PUNCH,&friendly,1);frames(&f,30,&friendly,1);assert(friendly.hp==4);
}
static void test_projectiles(void) {
  FcFight f;fc_init(&f,0);
  FcTarget targets[]={{{153,112,16,16},1,true,false,0,0},{{197,112,16,16},1,true,false,0,0}};
  tick(&f,FC_SPECIAL,targets,2);frames(&f,100,targets,2);
  assert(targets[0].hp==0 && targets[1].hp==1);
  targets[0].hp=targets[1].hp=3;
  fc_init(&f,0);f.meter=300;tick(&f,FC_SUPER,targets,2);frames(&f,150,targets,2);
  assert(!targets[0].hp && !targets[1].hp && f.meter==0);
  /* Super damage cannot pay for another super. */
  for(int i=0;i<FC_MAX_SHOTS;i++)assert(f.shots[i].ttl==0);
}
static void test_reactions(void) {
  FcFight f;fc_init(&f,0);tick(&f,0,NULL,0);
  assert(fc_hurt(&f,12,1) && f.hp==88 && f.stun==22 && f.hitstop==7);
  assert(!fc_hurt(&f,12,1) && f.hp==88);
  frames(&f,7,NULL,0);assert(f.stun==22);
  tick(&f,0,NULL,0);assert(f.stun==21 && f.override_y && f.velocity_y<0);
  frames(&f,70,NULL,0);assert(fc_hurt(&f,12,1) && f.hp==76);
  fc_init(&f,0);tick(&f,FC_GUARD,NULL,0);assert(fc_hurt(&f,12,1) && f.hp==99);
  fc_init(&f,0);tick(&f,FC_GUARD,NULL,0);assert(fc_hurt(&f,12,-1) && f.hp==88);
  fc_init(&f,0);f.hp=1;tick(&f,0,NULL,0);assert(fc_hurt(&f,12,1));
  assert(!f.hp && (f.events&FC_SND_KO));frames(&f,12,NULL,0);tick(&f,0,NULL,0);
  assert(f.override_y && f.velocity_y<0 && f.ko_timer==2);
  frames(&f,90,NULL,0);assert(f.move==FC_IDLE && f.hp==0);
}
static void test_charge_and_roster(void) {
  FcFight f;
  for(int id=0;id<FC_ROSTER_COUNT;id++) {
    fc_init(&f,id);tick(&f,FC_SPECIAL,NULL,0);assert(f.move>=FC_PROJECTILE);
    fc_init(&f,id);tick(&f,FC_UP|FC_SPECIAL,NULL,0);assert(f.move>=FC_RISING);
    fc_init(&f,id);f.meter=100;tick(&f,FC_SUPER,NULL,0);assert(f.move==FC_SUPER_MOVE);
  }
  for(int id=FC_HONDA;id<=FC_GUILE;id+=2) {
    fc_init(&f,id);for(int i=0;i<30;i++)tick(&f,FC_LEFT,NULL,0);
    tick(&f,FC_RIGHT|FC_PUNCH,NULL,0);assert(f.move==FC_RUSH || f.move==FC_PROJECTILE);
    fc_init(&f,id);for(int i=0;i<29;i++)tick(&f,FC_LEFT,NULL,0);
    tick(&f,FC_RIGHT|FC_PUNCH,NULL,0);assert(f.move==FC_JAB);
  }
  fc_init(&f,FC_GUILE);for(int i=0;i<30;i++)tick(&f,FC_DOWN,NULL,0);
  tick(&f,FC_UP|FC_KICK,NULL,0);assert(f.move==FC_RISING);
}
static void test_cancel_and_buffer(void) {
  FcFight f;fc_init(&f,0);FcTarget t={{118,110,16,20},5,true,false,0,0};
  tick(&f,FC_PUNCH,&t,1);frames(&f,3,&t,1);assert(f.move_hit);
  tick(&f,FC_SPECIAL,&t,1);frames(&f,5,&t,1);assert(f.move==FC_PROJECTILE);
  /* A whiffed jab cannot cancel immediately. */
  fc_init(&f,0);tick(&f,FC_PUNCH,NULL,0);tick(&f,FC_SPECIAL,NULL,0);
  frames(&f,5,NULL,0);assert(f.move==FC_JAB);
  frames(&f,15,NULL,0);assert(f.move==FC_IDLE);
  /* Holding a normal button must not create repeated jabs. */
  fc_init(&f,0);for(int i=0;i<60;i++)tick(&f,FC_PUNCH,NULL,0);assert(f.move==FC_IDLE);
}
static void test_stress(void) {
  FcFight f;fc_init(&f,0);FcTarget targets[FC_MAX_TARGETS]={0};
  uint32_t rng=1;
  for(int i=0;i<50000;i++) {
    rng=rng*1664525u+1013904223u;
    if(i%200==0){fc_init(&f,(i/200)%FC_ROSTER_COUNT);f.meter=300;}
    tick(&f,(rng>>16)&0xfff,targets,FC_MAX_TARGETS);
    if(i%37==0)fc_hurt(&f,12,(i&1)?-1:1);
    assert(f.meter>=0 && f.meter<=300 && f.hp>=0 && f.hp<=125);
  }
}
int main(void) {
  test_motions();test_super_meter();test_hits();test_projectiles();test_reactions();
  test_charge_and_roster();test_cancel_and_buffer();test_stress();
  puts("Combat: 8 suites passed (motions, meter, hits, projectiles, reactions, roster, cancels, stress).");
  return 0;
}
