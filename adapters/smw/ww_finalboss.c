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
static int attack,attack_ticks,wave,tier,attack_facing;
static float retreat_x;
static int wave_damage;
static bool active,captured,grappled;
static float car_x,car_y,fall_x,fall_y;
static uint32_t bowser[64*32];
extern Ppu *g_ppu;

void ww_final_reset(void) {
  active=captured=grappled=false;phase=ticks=intro_ticks=serial=fire_pose=peach_ticks=0;slot=-1;
  attack=WW_AKUMA_RETREAT;attack_ticks=wave=tier=0;attack_facing=1;retreat_x=160;
  wave_damage=0;
  memset(shots,0,sizeof(shots));memset(bowser,0,sizeof(bowser));
}
bool ww_final_controls(unsigned k) { return active && slot==(int)k; }
bool ww_final_cinematic(void) { return active && phase>=WW_FINAL_ARRIVAL && phase<=WW_FINAL_FALL; }
int ww_final_phase(void) { return active?phase:-1; }
int ww_final_tick(void) { return ticks; }
int ww_final_attack(void) { return attack; }
int ww_final_attack_tick(void) { return attack_ticks; }
bool ww_final_vulnerable(void) { return phase==WW_FINAL_FIGHT && wave_damage<12; }
int ww_final_limit_damage(int damage,int hp) {
  int floor=hp>48?48:hp>24?24:0;
  if(damage>6)damage=6;
  if(damage>12-wave_damage)damage=12-wave_damage;
  if(damage>hp-floor)damage=hp-floor;
  if(damage<0)damage=0;
  wave_damage+=damage;
  if(hp-damage==floor && floor)wave_damage=12;
  return damage;
}

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
  /* Translate the native Mode 7 entrance as a whole by 52 of 224 pixels.
   * This includes the distant approach, without accumulating a Y offset. */
  if(phase==WW_FINAL_INTRO)mirror_m7_ypos-=52;
}

static void fire(float angle,float speed) {
  for(int i=0;i<12;i++)if(!shots[i].life) {
    shots[i]=(DemonShot){car_x+17+attack_facing*20,car_y+20,cosf(angle)*speed,sinf(angle)*speed,150,++serial};
    break;
  }
  fire_pose=20;
}
static void projectiles(void);
static void set_attack(int state) {
  attack=state;attack_ticks=0;fire_pose=0;
  if(state==WW_AKUMA_AIR || state==WW_AKUMA_HORIZONTAL || state==WW_AKUMA_UPPERCUT || state==WW_AKUMA_BURST)wave_damage=0;
}
static void approach(float x,float y,float rate) {car_x+=(x-car_x)*rate;car_y+=(y-car_y)*rate;}
void ww_final_grapple(bool held) {
  if(phase!=WW_FINAL_FIGHT)return;
  if(held) {
    grappled=true;
    /* The carried copy includes the whole car. Suppress its native Mode 7
     * tilemap while ownership belongs to the grab, without running its AI. */
    memset(misc_mode7_boss_tilemap,0xff,96);
  } else if(grappled) {
    grappled=false;misc_m7_rotation=0;
    car_x=player_xpos>144?152:player_xpos+24;car_y=88;
    set_attack(WW_AKUMA_RECOVER);peach_ticks=75;
  }
}
void ww_final_grapple_art(FcCanvas c) {
  if(!g_ppu)return;
  for(int y=0;y<96 && y<c.h;y++)for(int x=0;x<64 && x<c.w;x++) {
    int sx=240+x,sy=272+y;
    int tile=g_ppu->vram[(sy/8)*128+sx/8]&255;
    int index=g_ppu->vram[tile*64+(sy&7)*8+(sx&7)]>>8;
    if(!index)continue;
    uint16 color=g_ppu->cgram[index];
    c.pixels[y*c.pitch+x]=0xff000000|((color&31)*255/31<<16)|(((color>>5)&31)*255/31<<8)|((color>>10)*255/31);
  }
  fc_sprite(c,0,FC_POSE_AKUMA_PILOT,0,33,30,attack_facing,1,2,0);
}
static void peach_help(unsigned k) {
  /* Peach stands at the rear of the same car. Spawn at her hand after the
   * car's new position is applied, and only during quiet recovery windows. */
  int x=(int)car_x+40,y=(int)car_y+5;
  Spr07C_PrincessPeach_GFXRt_DrawPeach((uint8)k,0x70,
    (PointU8){(uint8)(x-mirror_current_layer1_xpos),(uint8)(y-mirror_current_layer1_ypos)},
    peach_ticks>=28 && peach_ticks<44?2:0);
  if(peach_ticks==32) {
    for(int j=7;j>=2;j--)if(!spr_current_status[j]) {
      spr_current_status[j]=8;spr_spriteid[j]=0x74;
      InitializeNormalSpriteRAMTables((uint8)j);
      SetSprXYPos((uint8)j,x+5,y+12);spr_yspeed[j]=-64;
      spr_xspeed[j]=player_xpos<(uint16)x?-18:18;spr_table157c[j]=player_xpos<(uint16)x;
      io_sound_ch1=16;break;
    }
  }
  ++peach_ticks;
}
static void attack_step(unsigned k) {
  const FcOpponent *boss=ww_opponent();
  int hp=boss->active?boss->hp:72;
  tier=hp<=24?2:hp<=48?1:0;
  ++attack_ticks;projectiles();if(fire_pose)--fire_pose;
  if(attack==WW_AKUMA_RETREAT) {
    if(attack_ticks==1){retreat_x=player_xpos<128?172:24;attack_facing=retreat_x>96?-1:1;}
    approach(retreat_x,72,0.075f);
    if(attack_ticks>=50)set_attack(WW_AKUMA_PAUSE);
  } else if(attack==WW_AKUMA_PAUSE) {
    approach(retreat_x,72,0.12f);
    if(attack_ticks>=70-tier*10) {
      int next_move=wave%3==0?WW_AKUMA_AIR:wave%3==1?WW_AKUMA_HORIZONTAL:WW_AKUMA_UPPERCUT;
      if(tier==2 && wave%4==3)next_move=WW_AKUMA_BURST;
      ++wave;set_attack(next_move);
    }
  } else if(attack==WW_AKUMA_AIR || attack==WW_AKUMA_BURST) {
    approach(retreat_x,attack==WW_AKUMA_BURST?62:70,0.1f);
    int count=attack==WW_AKUMA_BURST?4:2+tier;
    if(attack_ticks>=22 && (attack_ticks-22)%20==0 && (attack_ticks-22)/20<count) {
      /* Zanku Hadoken: fixed downward diagonals, never homing shots.
       * Short volleys leave space to move and jump between them. */
      float angle=attack_facing>0?0.78f:2.36f;
      fire(angle,2.0f+tier*0.3f);
      if(attack==WW_AKUMA_BURST)fire(angle+attack_facing*0.26f,2.4f);
      fc_audio_play(FC_SND_AKUMA_SUPER);
    }
    if(attack_ticks>=22+count*20+20){set_attack(WW_AKUMA_RECOVER);peach_ticks=0;}
  } else if(attack==WW_AKUMA_HORIZONTAL) {
    float travel=attack_ticks<24?0:(attack_ticks-24)*(1.25f+tier*0.3f);
    if(travel>140)travel=140;
    approach(retreat_x+attack_facing*travel,92,0.15f);
    if(attack_ticks==28 || attack_ticks==58 || (tier>0 && attack_ticks==88)) {
      fire(attack_facing>0?0.10f:3.04f,2.3f+tier*0.35f);fc_audio_play(FC_SND_AKUMA_SUPER);
    }
    if(attack_ticks>=120){set_attack(WW_AKUMA_RECOVER);peach_ticks=0;}
  } else if(attack==WW_AKUMA_UPPERCUT) {
    if(attack_ticks==1)retreat_x=player_xpos<32?32:player_xpos>192?192:player_xpos;
    if(attack_ticks<=30)approach(retreat_x,100,0.12f);
    else if(attack_ticks<=49)car_y-=3.6f+tier*0.35f;
    else approach(retreat_x,96,0.07f);
    if(attack_ticks==31)fc_audio_play(FC_SND_AKUMA_SUPER);
    if(tier>0 && attack_ticks==52) {
      fire(0.9f,2.2f);fire(2.24f,2.2f);
    }
    if(attack_ticks>=100){set_attack(WW_AKUMA_RECOVER);peach_ticks=0;}
  } else {
    approach(car_x,88,0.08f);
    /* A real opening after every wave. No attacks spawn during recovery,
     * retreat or pause, even in the low-health phase. */
    if(wave%2==1 && peach_ticks<75)peach_help(k);
    if(attack_ticks>=105-tier*10)set_attack(WW_AKUMA_RETREAT);
  }
  if(car_x<16)car_x=16;if(car_x>188)car_x=188;
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
      car_x=(int16)GetSprXPos(k);car_y=(int16)GetSprYPos(k)+52;
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
    attack_step(k);
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

/* Project Bowser's actual red hair through the same Mode 7 matrix as his
 * head. The hat therefore scales, rotates and enters with him, instead of
 * appearing at an unrelated unscaled sprite coordinate. */
static void draw_bowser_hat(FcCanvas c) {
  if(!g_ppu)return;
  const int16 *m=g_ppu->m7matrix;
  float det=(float)m[0]*m[3]-(float)m[1]*m[2];
  if(fabsf(det)<1)return;
  int xc=(int16)(m[4]<<3)>>3,yc=(int16)(m[5]<<3)>>3;
  int h=((int16)(m[6]<<3)>>3)-xc,v=((int16)(m[7]<<3)>>3)-yc;
  h=(h&0x2000)?h|~1023:h&1023;v=(v&0x2000)?v|~1023:v&1023;
  float sum=0,top=10000;int count=0;
  for(int y=272;y<304;y++)for(int x=240;x<304;x++) {
    int tile=g_ppu->vram[(y/8)*128+x/8]&255;
    int index=g_ppu->vram[tile*64+(y&7)*8+(x&7)]>>8;
    if(!index)continue;
    uint16 color=g_ppu->cgram[index];int r=color&31,g=(color>>5)&31,b=(color>>10)&31;
    if(r<12 || r<g*1.5f || r<b*1.8f)continue;
    float px=256*(m[3]*(x-xc)-m[1]*(y-yc))/det-h;
    float py=256*(-m[2]*(x-xc)+m[0]*(y-yc))/det-v;
    if(PPU_m7xFlip(g_ppu))px=255-px;if(PPU_m7yFlip(g_ppu))py=255-py;
    sum+=px;if(py<top)top=py;++count;
  }
  if(!count)return;
  float scale=256/sqrtf(fabsf(det));int x=(int)(sum/count),y=(int)(top+3*scale);
  if(scale<0.2f || scale>3 || x<-24 || x>c.w+24 || y<-12 || y>c.h)return;
  fc_sprite(c,0,FC_POSE_BISON_HAT,0,x,y,1,(int)(scale*100),100,0);
}

void ww_final_render(FcCanvas c) {
  if(!active || slot<0 || misc_game_mode!=0x14)return;
  int x=(int16)(GetSprXPos(slot)-mirror_current_layer1_xpos);
  int y=(int16)(GetSprYPos(slot)-mirror_current_layer1_ypos);
  if(phase<=WW_FINAL_ARRIVAL)draw_bowser_hat(c);
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
    if(!grappled && (phase!=WW_FINAL_END || !spra0_bowser_end_of_battle_flag)) {
      int pose=fire_pose?FC_POSE_AKUMA_FIRE:FC_POSE_AKUMA_PILOT;
      unsigned frame=fire_pose?20-fire_pose:ticks;
      int facing=phase==WW_FINAL_FIGHT?attack_facing:player_xpos<x?-1:1;
      if(phase==WW_FINAL_FIGHT && fire_pose && (attack==WW_AKUMA_AIR || attack==WW_AKUMA_BURST)) {
        pose=FC_POSE_AKUMA_AIR_FIRE;frame=20-fire_pose;
      }
      if(phase==WW_FINAL_FIGHT && attack==WW_AKUMA_UPPERCUT && attack_ticks>24 && attack_ticks<65) {
        pose=FC_POSE_AKUMA_RISE;frame=(attack_ticks-25)*24/40;
      }
      fc_sprite(c,0,pose,frame,x+17,y+30,facing,1,2,0);
    }
    for(int i=0;i<12;i++)if(shots[i].life)
      fc_sprite(c,0,FC_POSE_AKUMA_SHOT,ticks,(int)shots[i].x,(int)shots[i].y,shots[i].vx>0?-1:1,1,2,0);
  }
}
