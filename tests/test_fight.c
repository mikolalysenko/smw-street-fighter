#include "fight.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static FcBody player={{100,100,16,32},1,true,false,false};
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
  static const FcMove primary[]={FC_PROJECTILE,FC_RUSH,FC_PROJECTILE,FC_PROJECTILE,FC_PROJECTILE,FC_RUSH,
    FC_SPIN,FC_PROJECTILE,FC_RUSH,FC_RUSH,FC_PROJECTILE,FC_RUSH};
  static const FcMove alternate[]={FC_RISING,FC_FLURRY,FC_SPIN,FC_RISING,FC_RISING,FC_RISING,
    FC_THROW,FC_FLAME,FC_RISING,FC_DIVE,FC_RISING,FC_DIVE};
  for(int c=0;c<FC_ROSTER_COUNT;c++) {
    FcFight f;fc_init(&f,c);tick(&f,FC_SPECIAL,NULL,0);assert(f.move==primary[c]);
    fc_init(&f,c);tick(&f,FC_UP|FC_SPECIAL,NULL,0);assert(f.move==alternate[c]);
    for(int meter=100;meter<=300;meter+=100) {
      FcTarget t={{116,100,40,40},50,true,false,0,0};fc_init(&f,c);f.meter=meter;
      tick(&f,FC_SUPER,&t,1);
      for(int n=0;n<150;n++) {
        player.grounded=c!=FC_ZANGIEF || n<25 || n>=70;
        tick(&f,0,&t,1);
      }
      player.grounded=true;
      assert(t.hp<50 && f.meter==0);
    }
  }
  FcFight f;fc_init(&f,FC_BISON);f.meter=300;
  FcTarget targets[]={{{10,100,16,16},99,true,false,0,0},{{220,190,16,16},99,true,false,0,0},
    {{90,30,16,16},99,true,false,0,0},{{90,160,16,16},99,true,false,0,0}};
  tick(&f,FC_SUPER,targets,4);frames(&f,30,targets,4);
  assert(fc_psycho_flight(&f) && !targets[0].hp && targets[1].hp==99 &&
    targets[2].hp==99 && targets[3].hp==99 && !f.hitstop);
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
  for(int facing=-1;facing<=1;facing+=2) {
    player.facing=facing;fc_init(&f,FC_DHALSIM);
    int x=facing>0?155:50;
    FcTarget flame[]={{{x,95,8,8},20,true,false,0,0},{{x,124,8,8},20,true,false,0,0},
      {{100,116,8,8},20,true,false,0,0}};
    tick(&f,FC_UP|FC_SPECIAL,flame,3);frames(&f,50,flame,3);
    assert(flame[0].hp<20 && flame[1].hp==20 && flame[2].hp==20);
  }
  player.facing=1;
}
static void test_projectiles(void) {
  FcFight f;fc_init(&f,0);
  FcTarget targets[]={{{153,102,16,16},1,true,false,0,0},{{197,102,16,16},1,true,false,0,0}};
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
  fc_init(&f,0);for(int i=0;i<7;i++)tick(&f,FC_GUARD,NULL,0);
  assert(fc_hurt(&f,12,1) && f.hp==99); /* Holding guard still takes chip. */
  fc_init(&f,0);tick(&f,FC_GUARD,NULL,0);assert(fc_hurt(&f,12,-1) && f.hp==88);
  fc_init(&f,0);f.hp=1;tick(&f,0,NULL,0);assert(fc_hurt(&f,12,1));
  assert(!f.hp && (f.events&FC_SND_KO));frames(&f,12,NULL,0);tick(&f,0,NULL,0);
  assert(f.override_y && f.velocity_y<0 && f.ko_timer==2);
  frames(&f,90,NULL,0);assert(f.move==FC_IDLE && f.hp==0);
}
static void test_timed_guard(void) {
  for(int c=0;c<FC_ROSTER_COUNT;c++)for(int facing=-1;facing<=1;facing+=2) {
    player.facing=facing;player.grounded=true;FcFight f;fc_init(&f,c);
    int hp=f.hp;f.combo=3;f.combo_timer=60;
    tick(&f,FC_GUARD,NULL,0);assert(fc_hurt(&f,16,facing));
    assert(f.hp==hp && f.meter==25 && f.parries==1 && f.parry_flash && f.combo==3);
    /* A repress during cooldown blocks normally instead of farming a parry. */
    f.invuln=f.hitstop=0;tick(&f,0,NULL,0);tick(&f,FC_GUARD,NULL,0);
    assert(fc_hurt(&f,16,facing) && f.hp==hp-2 && f.parries==1);
    fc_init(&f,c);tick(&f,FC_GUARD,NULL,0);assert(fc_hurt(&f,16,-facing));
    assert(f.hp==hp-16 && !f.parries);
    fc_init(&f,c);player.grounded=false;tick(&f,FC_GUARD,NULL,0);
    assert(fc_hurt(&f,16,facing) && !f.parries);
  }
  player.grounded=true;player.facing=1;
  for(int delay=1;delay<=9;delay++) {
    FcFight f;fc_init(&f,FC_RYU);
    for(int i=0;i<delay;i++)tick(&f,FC_GUARD,NULL,0);
    assert(fc_hurt(&f,16,1));assert(f.parries==(delay<=6));
  }
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
  FcFight f;FcBody b={{100,100,16,32},1,false,false,false};
  const uint32_t attacks[]={FC_PUNCH,FC_KICK,FC_SPECIAL,FC_UP|FC_SPECIAL,FC_SUPER};
  for(int character=0;character<FC_ROSTER_COUNT;character++)for(int a=0;a<5;a++) {
    fc_init(&f,character);f.meter=300;
    FcTarget target={{116,100,24,32},30,true,false,0,0};
    if(character==FC_DHALSIM && a==3)target.box.x=148; /* Flame begins at his mouth. */
    int impulse=0,shots=0,hits=0;
    for(int t=0;t<100;t++) {
      b.grounded=character==FC_ZANGIEF && (a==3 || a==4) && t>=60;
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
  fc_tick(&f,FC_RIGHT,b,NULL,0);assert(f.moving && f.walk_distance==3);
  b.box.x+=1;fc_tick(&f,FC_RIGHT,b,NULL,0);assert(f.walk_distance==4);
  for(int i=0;i<3;i++)fc_tick(&f,FC_RIGHT,b,NULL,0);
  assert(!f.moving && f.walk_distance==0); /* Sustained wall contact. */
  b.box.x+=2;fc_tick(&f,0,b,NULL,0);assert(!f.moving); /* Carried by a platform. */
  b.grounded=false;b.box.y-=3;fc_tick(&f,FC_JUMP,b,NULL,0);assert(f.air_tick==1);
  b.box.y-=3;fc_tick(&f,FC_JUMP,b,NULL,0);assert(f.air_tick==2 && f.vertical_motion<0);
  b.grounded=true;fc_tick(&f,0,b,NULL,0);assert(!f.air_tick);
}
static void test_grab_and_high_jump(void) {
  FcBody b=player;FcFight f;fc_init(&f,FC_ZANGIEF);
  FcTarget target={{110,45,96,96},500,true,false,0,0};
  for(int t=0;t<4;t++)fc_tick(&f,t?0:FC_UP|FC_SPECIAL,b,&target,1);
  assert(f.grabbed==0 && target.hp==500 && !target.hit);
  assert(f.override_y || f.move_tick>=11);
  b.grounded=false;
  for(int t=0;t<65;t++){b.box.y=t<30?99-t:69+(t-30);fc_tick(&f,0,b,&target,1);assert(target.hp==500 && !target.hit);}
  b.grounded=true;fc_tick(&f,0,b,&target,1);
  assert(!target.hp && (target.hit&FC_HIT_SLAM) && f.grab_slam && f.grabbed==-1);
  fc_init(&f,FC_ZANGIEF);
  for(int t=0;t<70;t++)fc_tick(&f,t?0:FC_UP|FC_SPECIAL,b,NULL,0);
  assert(f.move==FC_IDLE && f.grabbed==-1);
  fc_init(&f,FC_RYU);
  for(int t=0;t<50;t++)fc_tick(&f,FC_DOWN,b,NULL,0);
  fc_tick(&f,FC_JUMP|FC_RIGHT,b,NULL,0);assert(f.jump_boost && f.override_y && f.velocity_y==-7);
  b.grounded=false;
  for(int t=0;t<70;t++)fc_tick(&f,FC_JUMP,b,NULL,0);
  assert(!f.jump_boost);fc_tick(&f,0,b,NULL,0);fc_tick(&f,FC_JUMP,b,NULL,0);assert(!f.jump_boost);
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
static void test_stretch_and_slides(void) {
  for(int facing=-1;facing<=1;facing+=2)for(int crouch=0;crouch<2;crouch++) {
    FcFight f;fc_init(&f,FC_DHALSIM);FcBody b={{100,100,16,32},facing,true,false,false};
    FcTarget t[]={{{facing>0?190:18,124,8,8},10,true,false,0,0},
                  {{facing>0?230:-22,124,8,8},10,true,false,0,0}};
    for(int n=0;n<40;n++)fc_tick(&f,(crouch?FC_DOWN:0)|(n==0?FC_PUNCH:0),b,t,2);
    assert(t[0].hp==9 && t[1].hp==10 && f.move==FC_IDLE);
  }
  for(int c=0;c<2;c++)for(int facing=-1;facing<=1;facing+=2) {
    FcFight f;fc_init(&f,c?FC_BISON:FC_DHALSIM);FcBody b={{100,100,16,32},facing,true,false,false};
    float x=facing>0?188:20;
    FcTarget t[]={{{x,124,8,8},10,true,false,0,0},{{x,100,8,10},10,true,false,0,0},
                  {{facing>0?260:-60,124,8,8},10,true,false,0,0}};
    for(int n=0;n<60;n++) {
      fc_tick(&f,FC_DOWN|(n==0?FC_KICK:0),b,t,3);
      if(f.override_x && !f.frozen)b.box.x+=f.velocity_x;
    }
    assert((b.box.x-100)*facing>=40 && f.move==FC_IDLE);
    assert(t[0].hp==8 && t[1].hp==10 && t[2].hp==10);
  }
}
static void test_rapid_specials(void) {
  for(int c=0;c<3;c++) {
    FcFight f;fc_init(&f,c==0?FC_HONDA:c==1?FC_CHUNLI:FC_BLANKA);
    FcMove move=c==2?FC_ELECTRIC:FC_FLURRY;
    uint32_t button=c==1?FC_KICK:FC_PUNCH;
    FcTarget target={{116,106,12,26},10000,true,false,0,0};
    for(int t=0;t<900;t++) {
      uint32_t b=t%8==0?button:0;
      if(c==2)b=FC_DOWN|(t%8==0?FC_SPECIAL:0);
      tick(&f,b,&target,1);
      if(t>24)assert(f.move==move);
    }
    assert(target.hp<9980); /* Every loop remains an attack, not just art. */
    frames(&f,100,NULL,0);assert(f.move==FC_IDLE);
    fc_init(&f,c==0?FC_HONDA:c==1?FC_CHUNLI:FC_BLANKA);
    for(int t=0;t<200;t++)tick(&f,c==2?FC_DOWN|FC_SPECIAL:button,NULL,0);
    assert(f.move==FC_IDLE); /* Holding a button is not repeated tapping. */
  }
  FcFight f;fc_init(&f,FC_HONDA);player.grounded=false;
  tick(&f,FC_SPECIAL,NULL,0);
  for(int t=2;t<=29;t++) {
    tick(&f,0,NULL,0);
    if(t>=7)assert(f.override_y && f.velocity_y==0 && f.velocity_x==4);
  }
  player.grounded=true;
}
static void test_muzzles_and_beam(void) {
  for(int c=0;c<2;c++)for(int facing=-1;facing<=1;facing+=2) {
    FcFight f;fc_init(&f,c?FC_KEN:FC_RYU);player.facing=facing;
    tick(&f,FC_SPECIAL,NULL,0);frames(&f,5,NULL,0);
    FcShot *p=&f.shots[0];assert(p->ttl && !p->beam);
    assert(p->box.y+p->box.h/2==player.box.y+player.box.h-30);
    float hand=player.box.x+player.box.w/2+facing*38;
    assert((p->box.x+(facing<0?p->box.w:0)-hand)*facing>0);
    fc_init(&f,FC_RYU);f.meter=300;
    FcTarget targets[]={{{facing>0?400:-300,90,32,48},200,true,false,0,0},
      {{facing>0?1400:-1300,90,32,48},200,true,false,0,0}};
    tick(&f,FC_SUPER,targets,2);
    bool wide=false;
    for(int t=0;t<240;t++) {
      tick(&f,0,targets,2);
      if(f.shots[0].ttl && f.shots[0].box.w>1500)wide=true;
    }
    assert(wide && targets[0].hp<180 && targets[1].hp<190 && f.move==FC_IDLE);
    assert(!f.shots[0].ttl && !f.meter);
  }
  player.facing=1;
}
static void test_one_air_move(void) {
  const struct { int character; uint32_t input; FcMove move; } cases[]={
    {FC_RYU,FC_UP|FC_SPECIAL,FC_RISING},{FC_RYU,FC_DOWN|FC_SPECIAL,FC_SPIN},
    {FC_HONDA,FC_SPECIAL,FC_RUSH},{FC_CHUNLI,FC_UP|FC_SPECIAL,FC_SPIN},
    {FC_GUILE,FC_UP|FC_SPECIAL,FC_RISING},{FC_KEN,FC_UP|FC_SPECIAL,FC_RISING},
    {FC_BLANKA,FC_SPECIAL,FC_RUSH},{FC_BLANKA,FC_UP|FC_SPECIAL,FC_RISING},
    {FC_ZANGIEF,FC_SPECIAL,FC_SPIN},{FC_ZANGIEF,FC_UP|FC_SPECIAL,FC_THROW},
    {FC_DHALSIM,FC_DOWN|FC_SPECIAL,FC_TELEPORT},{FC_BALROG,FC_SPECIAL,FC_RUSH},
    {FC_VEGA,FC_SPECIAL,FC_RUSH},{FC_VEGA,FC_UP|FC_SPECIAL,FC_DIVE},
    {FC_SAGAT,FC_UP|FC_SPECIAL,FC_RISING},{FC_BISON,FC_SPECIAL,FC_RUSH},
    {FC_BISON,FC_UP|FC_SPECIAL,FC_DIVE}
  };
  FcBody air={{100,100,16,32},1,false,false,false};
  for(unsigned c=0;c<sizeof(cases)/sizeof(cases[0]);c++) {
    FcFight f;fc_init(&f,cases[c].character);
    fc_tick(&f,cases[c].input,air,NULL,0);assert(f.move==cases[c].move && f.air_move_used);
    for(int t=0;t<180;t++) {
      fc_tick(&f,t%8==0?cases[c].input:0,air,NULL,0);
      assert(!(f.events&FC_SND_SPECIAL));
    }
    assert(f.move==FC_IDLE);
    /* Each real support type refreshes the shared allowance. */
    for(int contact=0;contact<3;contact++) {
      FcBody supported=air;
      supported.grounded=contact==0;supported.swimming=contact==1;supported.climbing=contact==2;
      fc_tick(&f,0,supported,NULL,0);assert(!f.air_move_used);
      fc_tick(&f,cases[c].input,air,NULL,0);assert(f.move==cases[c].move);
      for(int t=0;t<70;t++)fc_tick(&f,0,air,NULL,0);
    }
  }
  for(int c=0;c<FC_ROSTER_COUNT;c++)if(c!=FC_GUILE && c!=FC_DHALSIM) {
    FcFight f;fc_init(&f,c);f.meter=300;fc_tick(&f,FC_SUPER,air,NULL,0);
    assert(f.move==FC_SUPER_MOVE && f.air_move_used && !f.meter);
    for(int t=0;t<1400;t++)fc_tick(&f,0,air,NULL,0);
    f.meter=300;fc_tick(&f,FC_SUPER,air,NULL,0);
    assert(f.move==FC_IDLE && f.meter==300 && !(f.events&FC_SND_SUPER));
  }
  /* A grounded launcher cannot refund itself in startup, and a connected
   * air uppercut cannot bypass the limit by cancelling into a super. */
  FcFight f;fc_init(&f,FC_RYU);FcBody ground=air;ground.grounded=true;
  fc_tick(&f,FC_UP|FC_SPECIAL,ground,NULL,0);fc_tick(&f,0,ground,NULL,0);
  fc_tick(&f,0,air,NULL,0);assert(f.air_move_used);
  f.move_hit=true;f.meter=300;fc_tick(&f,FC_SUPER,air,NULL,0);
  assert(f.move==FC_RISING && f.meter==300);
  for(int t=0;t<70;t++)fc_tick(&f,0,air,NULL,0);
  fc_tick(&f,FC_DOWN|FC_SPECIAL,air,NULL,0);assert(f.move==FC_IDLE);
  /* Normal attacks and fireballs still work after spending the air move. */
  fc_tick(&f,0,air,NULL,0);
  for(int i=0;i<3;i++) {
    fc_tick(&f,FC_SPECIAL,air,NULL,0);assert(f.move==FC_PROJECTILE);
    for(int t=0;t<30;t++)fc_tick(&f,0,air,NULL,0);
  }
  fc_tick(&f,FC_PUNCH,air,NULL,0);assert(f.move==FC_JAB);
  for(int t=0;t<30;t++)fc_tick(&f,0,air,NULL,0);
  f.invuln=0;assert(fc_hurt(&f,1,1));
  for(int t=0;t<80;t++)fc_tick(&f,0,air,NULL,0);
  fc_tick(&f,FC_UP|FC_SPECIAL,air,NULL,0);assert(f.move==FC_IDLE && f.air_move_used);
}
int main(void) {
  test_motions();test_super_meter();test_hurricane_kick();test_hits();test_projectiles();test_reactions();
  test_charge_and_roster();test_cancel_and_buffer();test_air_attacks_and_animation();test_crouching_attacks();test_stress();test_roster();
  test_combo_rewards();test_stretch_and_slides();test_rapid_specials();test_muzzles_and_beam();
  test_one_air_move();test_grab_and_high_jump();
  test_timed_guard();
  puts("Combat: 19 suites passed, including timed guard, air limits, charged jumps and delayed piledriver impacts.");
  return 0;
}
