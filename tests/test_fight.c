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
static void test_hurricane_kick(void) {
  FcFight f;
  for(int facing=-1;facing<=1;facing+=2)for(int air=0;air<2;air++) {
    player.facing=facing;player.grounded=!air;
    fc_init(&f,FC_RYU);tick(&f,FC_DOWN|FC_SPECIAL,NULL,0);
    assert(f.move==FC_SPIN && !strcmp(fc_move_name(&f),"TATSUMAKI"));
    fc_init(&f,FC_RYU);tick(&f,FC_DOWN,NULL,0);
    uint32_t back=facing>0?FC_LEFT:FC_RIGHT;
    player.facing=-facing; /* The platformer turns while walking back. */
    tick(&f,FC_DOWN|back,NULL,0);tick(&f,back|FC_KICK,NULL,0);
    assert(f.move==FC_SPIN && f.facing==facing);
  }
  player.facing=1;player.grounded=true;
}
static void test_roster(void) {
  static const FcMove primary[]={FC_PROJECTILE,FC_RUSH,FC_FLURRY,FC_PROJECTILE,FC_PROJECTILE,FC_RUSH,
    FC_SPIN,FC_PROJECTILE,FC_RUSH,FC_RUSH,FC_PROJECTILE,FC_RUSH};
  static const FcMove alternate[]={FC_RISING,FC_FLURRY,FC_SPIN,FC_RISING,FC_RISING,FC_RISING,
    FC_THROW,FC_FLAME,FC_RISING,FC_DIVE,FC_RISING,FC_DIVE};
  for(int c=0;c<FC_ROSTER_COUNT;c++) {
    FcFight f;fc_init(&f,c);tick(&f,FC_SPECIAL,NULL,0);assert(f.move==primary[c]);
    fc_init(&f,c);tick(&f,FC_UP|FC_SPECIAL,NULL,0);assert(f.move==alternate[c]);
    for(int meter=100;meter<=300;meter+=100) {
      FcTarget t={{116,100,40,40},50,true,false,0,0};fc_init(&f,c);f.meter=meter;
      tick(&f,FC_SUPER,&t,1);frames(&f,150,&t,1);
      assert(t.hp<50 && f.meter==0);
    }
  }
  FcFight f;fc_init(&f,FC_BISON);f.meter=300;
  FcTarget targets[]={{{10,20,16,16},99,true,false,0,0},{{220,190,16,16},99,true,false,0,0}};
  tick(&f,FC_SUPER,targets,2);frames(&f,30,targets,2);
  assert(fc_psycho_flight(&f) && !targets[0].hp && !targets[1].hp && !f.hitstop);
  assert(!fc_hurt(&f,999,1));frames(&f,500,NULL,0);assert(fc_psycho_flight(&f));
  fc_init(&f,FC_DHALSIM);tick(&f,FC_DOWN|FC_SPECIAL,NULL,0);assert(f.move==FC_TELEPORT);
  fc_init(&f,FC_BLANKA);tick(&f,FC_DOWN|FC_SPECIAL,NULL,0);assert(f.move==FC_ELECTRIC);
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
    assert(f.meter>=0 && f.meter<=300 && f.hp>=0 && f.hp<=fc_roster[f.character].vitality);
  }
}
static void test_air_attacks_and_animation(void) {
  FcFight f;FcBody b={{100,100,16,32},1,false};
  const uint32_t attacks[]={FC_PUNCH,FC_KICK,FC_SPECIAL,FC_UP|FC_SPECIAL,FC_SUPER};
  for(int character=0;character<FC_ROSTER_COUNT;character++)for(int a=0;a<5;a++) {
    fc_init(&f,character);f.meter=300;
    FcTarget target={{116,100,24,32},30,true,false,0,0};
    int impulse=0,shots=0,hits=0;
    for(int t=0;t<100;t++) {
      fc_tick(&f,FC_JUMP|(t==0?attacks[a]:0),b,&target,1);
      impulse|=f.override_y && f.velocity_y<0;hits|=target.hit;
      for(int j=0;j<FC_MAX_SHOTS;j++)shots|=f.shots[j].ttl>0;
    }
    assert(hits);
    if(a==3 && (character==FC_RYU || character==FC_GUILE))assert(impulse);
    if(a==2 && (character==FC_RYU || character==FC_GUILE))assert(shots || hits);
  }
  fc_init(&f,0);b.grounded=true;fc_tick(&f,FC_RIGHT,b,NULL,0);
  assert(!f.moving);b.box.x+=3;fc_tick(&f,FC_RIGHT,b,NULL,0);
  assert(f.moving && f.walk_distance==3);
  fc_tick(&f,FC_RIGHT,b,NULL,0);assert(!f.moving); /* Pushing against a wall. */
  b.grounded=false;b.box.y-=3;fc_tick(&f,FC_JUMP,b,NULL,0);assert(f.air_tick==1);
  b.box.y-=3;fc_tick(&f,FC_JUMP,b,NULL,0);assert(f.air_tick==2 && f.vertical_motion<0);
  b.grounded=true;fc_tick(&f,0,b,NULL,0);assert(!f.air_tick);
}
static void test_crouching_attacks(void) {
  for(int id=0;id<FC_ROSTER_COUNT;id++)for(int kick=0;kick<2;kick++) {
    FcFight f;fc_init(&f,id);
    FcTarget targets[]={{{118,124,12,8},10,true,false,0,0},{{118,100,12,10},10,true,false,0,0}};
    tick(&f,FC_DOWN|(kick?FC_KICK:FC_PUNCH),targets,2);
    assert(f.crouching_attack);
    /* Releasing Down must not change a crouching attack to a standing pose. */
    for(int t=0;t<12;t++){tick(&f,0,targets,2);assert(f.crouching_attack);}
    assert(targets[0].hp<10 && targets[1].hp==10);
    frames(&f,40,targets,2);assert(f.move==FC_IDLE && !f.crouching_attack);
  }
}
static void test_combo_rewards(void) {
  FcFight f;fc_init(&f,FC_BISON);f.meter=300;
  tick(&f,FC_SUPER,NULL,0);frames(&f,19,NULL,0);
  const int expected[]={200,400,800,1000,2000,4000,8000,-1,-2,-3,-3};
  for(unsigned i=0;i<sizeof(expected)/sizeof(expected[0]);i++) {
    FcTarget t={{118,110,16,20},1,true,false,0,0};
    tick(&f,0,&t,1);assert(t.hit && f.combo==(int)i+1);
    assert(f.reward_count==1 && f.rewards[0].award==expected[i]);
    assert(f.score_gain==(expected[i]>0?expected[i]:0));
    assert(f.lives_gain==(expected[i]<0?-expected[i]:0));
  }
  frames(&f,150,NULL,0);assert(f.combo==11); /* Continuous flight keeps the chain. */
  f.move=FC_IDLE;f.invuln=0;assert(fc_hurt(&f,1,1));assert(!f.combo);
  fc_init(&f,FC_BISON);f.move=FC_SUPER_MOVE;f.super_level=3;
  FcTarget targets[12];for(int i=0;i<12;i++)targets[i]=(FcTarget){{118,110,16,20},1,true,false,0,0};
  tick(&f,0,targets,12);
  assert(f.reward_count==12 && f.score_gain==16400 && f.lives_gain==12);
  tick(&f,0,NULL,0);assert(!f.reward_count && !f.score_gain && !f.lives_gain);
  f.move=FC_IDLE;frames(&f,91,NULL,0);assert(!f.combo);
}
int main(void) {
  test_motions();test_super_meter();test_hurricane_kick();test_hits();test_projectiles();test_reactions();
  test_charge_and_roster();test_cancel_and_buffer();test_air_attacks_and_animation();test_crouching_attacks();test_stress();test_roster();
  test_combo_rewards();
  puts("Combat: 13 suites passed, including twelve fighters, all super levels, hurricane inputs, Psycho flight and combo rewards.");
  return 0;
}
