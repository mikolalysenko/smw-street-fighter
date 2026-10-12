/* Exercise the combat-to-renderer boundary with the actual packed art. */
#include "assets.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t pixels[192*160];
static uint64_t render(const FcFight *f) {
  memset(pixels,0,sizeof(pixels));
  FcCanvas canvas={pixels,192,160,192};
  FcAnimation a=fc_animation(f);
  assert(fc_sprite_duration(f->character,a.pose)>0);
  fc_sprite(canvas,f->character,a.pose,a.tick,96,130,f->facing,1,2,0);
  unsigned visible=0;uint64_t hash=UINT64_C(1469598103934665603);
  for(unsigned i=0;i<sizeof(pixels)/sizeof(*pixels);i++) {
    visible+=!!pixels[i];hash=(hash^pixels[i])*UINT64_C(1099511628211);
  }
  assert(visible>80);return hash;
}
static FcBody body={{72,98,16,32},1,true,false,false};
int main(void) {
  assert(fc_assets_load("local/sf2/sprites.dat"));
  /* Feet stay on the platform plane through every idle/walk/crouch frame,
   * including the opposite horizontal sampling phase when mirrored. */
  for(int c=0;c<FC_ROSTER_COUNT;c++)for(int facing=-1;facing<=1;facing+=2) {
    int poses[]={FC_POSE_IDLE,FC_POSE_WALK,FC_POSE_DUCK,FC_POSE_PUNCH,FC_POSE_KICK,
      FC_POSE_CROUCH_PUNCH,FC_POSE_CROUCH_KICK,FC_POSE_RUSH,FC_POSE_RISE};
    for(int p=0;p<(c==FC_BALROG?9:7);p++)for(unsigned t=0;t<fc_sprite_duration(c,poses[p]);t++) {
      memset(pixels,0,sizeof(pixels));
      fc_sprite((FcCanvas){pixels,192,160,192},c,poses[p],t,96,130,facing,1,2,0);
      int bottom=0;
      for(int y=0;y<160;y++)for(int x=0;x<192;x++)if(pixels[y*192+x])bottom=y;
      if(bottom!=129)fprintf(stderr,"Floating feet character=%d pose=%d tick=%u facing=%d bottom=%d\n",c,poses[p],t,facing,bottom);
      assert(bottom==129);
    }
  }
  for(int c=0;c<FC_ROSTER_COUNT;c++)for(int facing=-1;facing<=1;facing+=2) {
    body.facing=facing;
    for(int posture=0;posture<3;posture++)for(int kick=0;kick<2;kick++) {
      FcFight f;fc_init(&f,c);body.grounded=posture!=2;
      int active=kick?7:4;uint64_t before=0,strike=0;
      for(int t=1;t<=(kick?25:16);t++) {
        uint32_t buttons=t==1?(kick?FC_KICK:FC_PUNCH):0;
        if(posture==1)buttons|=FC_DOWN;
        fc_tick(&f,buttons,body,NULL,0);
        FcAnimation a=fc_animation(&f);
        int expected=posture==2?(kick?FC_POSE_AIR_KICK:FC_POSE_AIR_PUNCH):
          posture==1?(kick?FC_POSE_CROUCH_KICK:FC_POSE_CROUCH_PUNCH):kick?FC_POSE_KICK:FC_POSE_PUNCH;
        assert(a.pose==expected);
        uint64_t hash=render(&f);
        if(t==active-1)before=hash;
        if(t==active)strike=hash;
        if(c==FC_DHALSIM && !kick && posture<2 && t==active) {
          int tip=0;
          for(int y=0;y<160;y++)for(int x=0;x<192;x++)if(pixels[y*192+x]) {
            int reach=facing>0?x-96:96-x;if(reach>tip)tip=reach;
          }
          assert(tip>=85); /* Actual extended-arm pixels, not a short jab. */
        }
      }
      assert(before!=strike); /* The hand/foot extends when damage activates. */
      if(posture==2) {
        body.grounded=true;fc_tick(&f,0,body,NULL,0);
        assert(f.move==FC_IDLE && fc_animation(&f).pose==FC_POSE_DUCK);
        for(int i=0;i<7;i++)fc_tick(&f,0,body,NULL,0);
        assert(fc_animation(&f).pose==FC_POSE_IDLE);
      }
    }
    body.grounded=true;
    for(int move=0;move<4;move++) {
      FcFight f;fc_init(&f,c);f.meter=200;
      uint32_t buttons[]={FC_SPECIAL,FC_UP|FC_SPECIAL,FC_DOWN|FC_SPECIAL,FC_SUPER};
      fc_tick(&f,buttons[move],body,NULL,0);assert(fc_busy(&f));
      for(int t=0;t<160;t++) {
        render(&f);FcAnimation before=fc_animation(&f);
        if(f.flash || f.hitstop) {
          int clock=f.move_tick;fc_tick(&f,0,body,NULL,0);
          assert(f.move_tick==clock);
          FcAnimation after=fc_animation(&f);
          /* Last flash frame changes from charge to the active move. */
          if(f.flash)assert(before.pose==after.pose && before.tick==after.tick);
        } else fc_tick(&f,0,body,NULL,0);
      }
      assert(f.move==FC_IDLE);
    }
    FcFight f;fc_init(&f,c);body.grounded=true;fc_tick(&f,0,body,NULL,0);
    body.grounded=false;
    for(int t=0;t<40;t++) {
      body.box.y+=t<20?-2:2;fc_tick(&f,FC_RIGHT,body,NULL,0);render(&f);
      assert(fc_animation(&f).pose==FC_POSE_JUMP_FORWARD);
    }
    assert(fc_animation(&f).tick>=fc_sprite_duration(c,FC_POSE_JUMP_FORWARD)-1);
    body.grounded=true;fc_tick(&f,0,body,NULL,0);
    assert(f.landing_tick && fc_animation(&f).pose==FC_POSE_DUCK);
    /* Landing is only a pose; a new attack interrupts it immediately. */
    fc_tick(&f,FC_PUNCH,body,NULL,0);assert(fc_animation(&f).pose==FC_POSE_PUNCH);
    fc_init(&f,c);fc_tick(&f,0,body,NULL,0);
    body.grounded=false;body.box.y+=2;fc_tick(&f,FC_RIGHT,body,NULL,0);
    assert(!f.jump_forward && f.fall_tick && fc_animation(&f).pose==FC_POSE_JUMP);
    body.box.y=98;body.grounded=true;
  }
  FcFight f;fc_init(&f,FC_RYU);body.grounded=false;
  fc_tick(&f,FC_PUNCH,body,NULL,0);fc_tick(&f,0,body,NULL,0);
  int air=f.air_tick,clock=f.animation_tick;f.hitstop=4;
  for(int i=0;i<4;i++)fc_tick(&f,0,body,NULL,0);
  assert(f.air_tick==air && f.animation_tick==(unsigned)clock && f.move_tick==2);
  for(int super=0;super<2;super++) {
    fc_init(&f,FC_ZANGIEF);f.meter=300;body.grounded=true;
    for(int t=0;t<80;t++) {
      fc_tick(&f,t?0:super?FC_SUPER:FC_UP|FC_SPECIAL,body,NULL,0);
      if(fc_piledriver(&f))assert(fc_animation(&f).pose==FC_POSE_THROW && fc_animation(&f).tick<11);
      assert(!f.override_y); /* A whiff reaches; it never launches or spins. */
    }
  }
  /* Kikoken has its own yellow core and multiple animation frames. */
  memset(pixels,0,sizeof(pixels));
  fc_sprite((FcCanvas){pixels,192,160,192},FC_CHUNLI,FC_POSE_SHOT,0,96,80,1,1,2,0);
  int yellow=0;
  for(unsigned i=0;i<192*160;i++)if(((pixels[i]>>16)&255)>200 && ((pixels[i]>>8)&255)>160 && (pixels[i]&255)<160)yellow++;
  assert(yellow>=4 && fc_sprite_duration(FC_CHUNLI,FC_POSE_SHOT)>=12);
  fc_assets_free();
  puts("Animation: all 12 fighters, both facings, strike timing, air landing, jumps, specials and super freeze passed.");
  return 0;
}
