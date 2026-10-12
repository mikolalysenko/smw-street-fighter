/* Akuma steals the original Mode 7 clown car. The original Bowser loader,
 * roof, Peach power-ups, rescue, fireworks and credits still own progression. */
#include "ww_finalboss.h"
#include "ww_smw.h"
#include "ww_targets.h"
#include "assets.h"
#include "audio.h"
#include "smw_rtl.h"
#include "snes/ppu.h"
#include "variables.h"
#include "funcs.h"
#include <math.h>
#include <string.h>

typedef struct { float x,y,vx,vy; int life,serial; } DemonShot;
static DemonShot shots[12];
static int phase,ticks,intro_ticks,slot,serial,fire_pose,peach_ticks;
static bool active,captured;
static float car_x,car_y,fall_x,fall_y;
static uint32_t bowser[64*32];
extern Ppu *g_ppu;

void ww_final_reset(void) {
  active=captured=false;phase=ticks=intro_ticks=serial=fire_pose=peach_ticks=0;slot=-1;
  memset(shots,0,sizeof(shots));memset(bowser,0,sizeof(bowser));
}
bool ww_final_controls(unsigned k) { return active && slot==(int)k; }
bool ww_final_cinematic(void) { return active && phase>=WW_FINAL_ARRIVAL && phase<=WW_FINAL_FALL; }
int ww_final_phase(void) { return active?phase:-1; }
int ww_final_tick(void) { return ticks; }

static void next(int state) { phase=state;ticks=0; }
static void capture_bowser(void) {
  if(!g_ppu)return;
  /* Last four rows of Bowser's tilemap are his body, separate from the
   * eight clown-car/propeller rows. Decode the original Mode 7 pixels. */
  for(int y=0;y<32;y++)for(int x=0;x<64;x++) {
    int tile=misc_mode7_boss_tilemap[64+(3-y/8)*8+x/8];
    int index=g_ppu->vram[tile*64+(y%8)*8+x%8]>>8;
    uint16 color=g_ppu->cgram[index];
    unsigned r=color&31,g=(color>>5)&31,b=(color>>10)&31;
    bowser[y*64+x]=index?0xff000000|((r*255/31)<<16)|((g*255/31)<<8)|(b*255/31):0;
  }
  captured=true;
}
void ww_final_tiles(unsigned k) {
  if(!ww_final_controls(k))return;
  if(phase==WW_FINAL_ARRIVAL && !captured)capture_bowser();
  if(phase>=WW_FINAL_FALL)memset(misc_mode7_boss_tilemap+64,0xff,32);
}

static void fire(float angle,float speed) {
  for(int i=0;i<12;i++)if(!shots[i].life) {
    shots[i]=(DemonShot){car_x+16,car_y+30,cosf(angle)*speed,sinf(angle)*speed,180,++serial};
    break;
  }
  fire_pose=16;
}
static void projectiles(void) {
  const FcFight *f=ww_fight_state();
  for(int i=0;i<12;i++)if(shots[i].life) {
    DemonShot *p=&shots[i];p->x+=p->vx;p->y+=p->vy;--p->life;
    if(p->x<-40 || p->x>300 || p->y>232 || p->y<-48)p->life=0;
    if(p->life && fc_overlap((FcBox){p->x-7,p->y-7,14,14},f->body.box)) {
      if(ww_take_damage(16,p->vx>0?-1:1))p->life=0;
    }
  }
}
int ww_final_target(unsigned i,FcTarget *t) {
  if(i>=12 || !active || phase!=WW_FINAL_FIGHT || !shots[i].life)return 0;
  DemonShot *p=&shots[i];
  *t=(FcTarget){{p->x-9,p->y-9,18,18},1,true,false,0,0};
  return p->serial;
}
void ww_final_hit_shot(unsigned i) { if(i<12)shots[i].life=0; }
void ww_final_defeated(void) { if(active){next(WW_FINAL_END);memset(shots,0,sizeof(shots));} }

bool ww_final_step(unsigned k) {
  if(!ww_enabled())return false;
  if(!active) { active=true;slot=(int)k;phase=WW_FINAL_INTRO; }
  if(slot!=(int)k)return false;
  if(phase==WW_FINAL_END)return false; /* Native rescue sequence. */
  if(flag_sprites_locked || player_current_state==9)return phase!=WW_FINAL_INTRO;
  ++ticks;
  if(phase==WW_FINAL_INTRO) {
    if(spr_table151c[k]==7 && ++intro_ticks>=75) {
      car_x=(int16)GetSprXPos(k);car_y=(int16)GetSprYPos(k);
      fall_x=car_x-16;fall_y=car_y;
      next(WW_FINAL_ARRIVAL);fc_audio_play(FC_SND_AKUMA_WARP);
      FcFight *f=(FcFight*)ww_fight_state();fc_clear_actions(f);
    }
    return false;
  }
  if(phase==WW_FINAL_ARRIVAL && ticks>=65) {
    next(WW_FINAL_DEMON);fc_audio_play(FC_SND_SUPER|FC_SND_AKUMA_SUPER);
  } else if(phase==WW_FINAL_DEMON) {
    if(ticks>18 && ticks<80 && ticks%4==0)fc_audio_play(FC_SND_HIT);
    if(ticks>=110){next(WW_FINAL_FALL);fc_audio_play(FC_SND_KO);}
  } else if(phase==WW_FINAL_FALL) {
    fall_x+=0.7f;fall_y+=1.0f+ticks*0.13f;
    if(ticks==18)fc_audio_play(FC_SND_AKUMA_WARP);
    if(ticks>=90) {
      next(WW_FINAL_FIGHT);spr_table151c[k]=7;spr_table187b[k]=2;
      ww_target_spawned(k);
    }
  } else if(phase==WW_FINAL_FIGHT) {
    bool angry=ww_opponent()->active && ww_opponent()->hp<36;
    float speed=angry?0.047f:0.034f;
    /* Predictable sweeps alternate with a low dive: safe gaps remain, and
     * every pass brings the pilot within jumping uppercut range. */
    float x=96+sinf(ticks*speed)*83;
    float y=30+sinf(ticks*0.026f)*18;
    int cycle=ticks%360;
    if(cycle>230 && cycle<320)y+=sinf((cycle-230)*3.14159265f/90)*46;
    car_x+=(x-car_x)*0.18f;car_y+=(y-car_y)*0.14f;
    projectiles();if(fire_pose)--fire_pose;
    int spacing=angry?39:58;
    if(ticks>80 && ticks%spacing==0) {
      float angle=atan2f(player_ypos+20-(car_y+30),player_xpos+8-(car_x+16));
      fire(angle,angry?2.4f:1.9f);
      if(angry && ticks%(spacing*3)==0){fire(angle-0.28f,2.1f);fire(angle+0.28f,2.1f);}
      fc_audio_play(FC_SND_AKUMA_SUPER);
    }
    /* Actual Peach OAM and actual native mushroom actors, not facsimiles. */
    peach_ticks=ticks%480;
    if(peach_ticks>=370 && peach_ticks<460) {
      Spr07C_PrincessPeach_GFXRt_DrawHELP((uint8)k,10,32,0);
      if(peach_ticks==405 || peach_ticks==435)Spr0A0_ActivateBowserBattle_03A8D6((uint8)k);
    }
    Spr0A0_ActivateBowserBattle_03B078((uint8)k);
  }
  SetSprXYPos((uint8)k,(uint16)(int)car_x,(uint16)(int)car_y);
  spr_xspeed[k]=spr_yspeed[k]=0;
  misc_m7_angle=0x2020;misc_m7_rotation=0;
  misc_mode7_tilemap_index=30;
  spra0_bowser_propeller_anim_frame_counter=(counter_local_frames>>1)&3;
  spra0_bowser_clown_car_face_anim_frame=phase==WW_FINAL_FIGHT && fire_pose?3:0;
  UpdateMode7SpriteAnimations((uint8)k);ww_final_tiles(k);
  return true;
}

void ww_final_render(FcCanvas c) {
  if(!active || slot<0 || misc_game_mode!=0x14)return;
  int x=(int16)(GetSprXPos(slot)-mirror_current_layer1_xpos);
  int y=(int16)(GetSprYPos(slot)-mirror_current_layer1_ypos);
  if(phase<=WW_FINAL_ARRIVAL) {
    /* Hat follows the original body as Bowser ducks into the car. */
    int duck=spr_table1570[slot]>=10?16:0;
    fc_sprite(c,0,FC_POSE_BISON_HAT,0,x+16,y+duck+3,1,1,1,0);
  }
  if(phase==WW_FINAL_ARRIVAL) {
    int ax=290+(x+16-290)*ticks/65;
    for(int i=3;i>=1;i--)fc_sprite(c,0,FC_POSE_AKUMA_WARP,0,ax+i*14,y+34,-1,1,3,0x36316b);
    fc_sprite(c,0,FC_POSE_AKUMA_WARP,0,ax,y+34,-1,1,3,0);
  } else if(phase==WW_FINAL_DEMON) {
    fc_rect(c,0,29,c.w,c.h-44,0x000000);
    if(ticks<80) {
      int sx=x+((ticks*37)%42)-10,sy=y+12+(ticks*13)%32;
      if(ticks>18 && ticks%4<2) {
        fc_rect(c,sx-9,sy-1,19,3,0xffedb3);fc_rect(c,sx-1,sy-9,3,19,0xffedb3);
        fc_rect(c,sx-4,sy-4,9,9,0xffffff);
      }
    } else fc_sprite(c,0,FC_POSE_AKUMA_DEMON,28,x+16,y+48,1,1,2,0);
  } else if(phase>=WW_FINAL_FALL) {
    if(phase==WW_FINAL_FALL && captured) {
      int left=(int)fall_x,top=(int)fall_y;
      for(int py=0;py<32;py++)for(int px=0;px<64;px++) {
        int dx=left+px,dy=top+py;uint32_t color=bowser[(31-py)*64+px];
        if(color && dx>=0 && dx<c.w && dy>=0 && dy<c.h)c.pixels[dy*c.pitch+dx]=color;
      }
      /* Bison's stolen hat stays behind for one beat, then drops too. */
      if(ticks>12)fc_sprite(c,0,FC_POSE_BISON_HAT,0,left+30,top-8,1,1,1,0);
    }
    if(phase!=WW_FINAL_END || !spra0_bowser_end_of_battle_flag)
      fc_sprite(c,0,fire_pose?FC_POSE_AKUMA_FIRE:FC_POSE_AKUMA_PILOT,
        fire_pose?16-fire_pose:ticks,x+17,y+30,player_xpos<x?-1:1,1,2,0);
    for(int i=0;i<12;i++)if(shots[i].life)
      fc_sprite(c,0,FC_POSE_AKUMA_SHOT,ticks,(int)shots[i].x,(int)shots[i].y,shots[i].vx>0?-1:1,1,2,0);
  }
}
