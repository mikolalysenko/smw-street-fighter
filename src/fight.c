#include "fight.h"
#include <string.h>

const FcCharacter fc_roster[FC_ROSTER_COUNT] = {
  {"RYU", "ANSATSUKEN", "HADOUKEN", "SHORYUKEN", "SHINKU HADOUKEN", 0x64e7ff,0xe8f3eb,0xefb381,100,2.3f},
  {"E. HONDA", "SUMO", "SUMO HEADBUTT", "HUNDRED HAND SLAP", "SUPER ONI MUSOU", 0x88adff,0x456fc0,0xeab382,125,1.85f},
  {"CHUN-LI", "KUNG FU", "LIGHTNING LEGS", "SPINNING BIRD KICK", "SENRETSUKYAKU", 0xffcf65,0x4284ee,0xf5c398,90,2.75f},
  {"GUILE", "AIR FORCE", "SONIC BOOM", "FLASH KICK", "SONIC HURRICANE", 0x9bef91,0x82a65c,0xe8b684,110,2.2f},
  {"KEN", "ANSATSUKEN", "HADOUKEN", "SHORYUKEN", "SHORYUREPPA", 0xff7948,0xe63220,0xf2b078,100,2.5f},
  {"BLANKA", "WILD FIGHTING", "ROLLING ATTACK", "ELECTRIC THUNDER", "GROUND SHAVE ROLL", 0xa6ff42,0xe98f19,0x8ac940,115,2.6f},
  {"ZANGIEF", "WRESTLING", "DOUBLE LARIAT", "SCREW PILEDRIVER", "FINAL ATOMIC BUSTER", 0xff6655,0xc62518,0xe8ad75,145,1.8f},
  {"DHALSIM", "YOGA", "YOGA FIRE", "YOGA FLAME", "YOGA INFERNO", 0xffb93b,0xffcd35,0xba8355,95,2.0f},
  {"BALROG", "BOXING", "DASH STRAIGHT", "DASH UPPERCUT", "CRAZY BUFFALO", 0x80baff,0x4d87d5,0xad7146,125,2.4f},
  {"VEGA", "NINJUTSU", "ROLLING CRYSTAL FLASH", "FLYING BARCELONA", "ROLLING IZUNA DROP", 0xdb96ff,0x8145cd,0xe2ae78,95,2.9f},
  {"SAGAT", "MUAY THAI", "TIGER SHOT", "TIGER UPPERCUT", "TIGER GENOCIDE", 0xffa846,0x7840c5,0xd8ac78,120,2.2f},
  {"M. BISON", "PSYCHO POWER", "PSYCHO CRUSHER", "HEAD PRESS", "PSYCHO CRUSHER", 0xc485ff,0xd82c22,0xe4a47b,125,2.6f}
};

static int imin(int a,int b) { return a<b?a:b; }
static int imax(int a,int b) { return a>b?a:b; }
static void dec(int *v) { if (*v>0) --*v; }
bool fc_overlap(FcBox a,FcBox b) {
  return a.x<b.x+b.w && a.x+a.w>b.x && a.y<b.y+b.h && a.y+a.h>b.y;
}
void fc_meter(FcFight *f,int amount) { f->meter=imax(0,imin(300,f->meter+amount)); }
void fc_init(FcFight *f,int character) {
  memset(f,0,sizeof(*f));
  f->character=character>=0 && character<FC_ROSTER_COUNT?character:FC_RYU;
  f->hp=fc_roster[f->character].vitality;
  f->facing=1;
}
void fc_clear_actions(FcFight *f) {
  int character=f->character,hp=f->hp,meter=f->meter;
  fc_init(f,character); f->hp=hp; f->meter=meter;
}
static void sparks(FcFight *f,float x,float y,uint32_t color,int count) {
  for(int n=0;n<count;n++) {
    for(int i=0;i<FC_MAX_SPARKS;i++) if(!f->sparks[i].ttl) {
      unsigned seed=(f->tick+1)*1103515245u+(unsigned)(i+n*7)*12345u;
      f->sparks[i]=(FcSpark){x,y,((int)(seed%91)-45)*0.07f,
        -1.0f-(float)((seed>>8)%40)*0.09f,14+(int)(seed%12),color};
      break;
    }
  }
}
void fc_explode(FcFight *f,float x,float y,bool large) {
  int slot=0;
  for(int i=1;i<FC_MAX_BLASTS;i++)if(f->blasts[i].ttl<f->blasts[slot].ttl)slot=i;
  f->blasts[slot]=(FcBlast){x,y,24,large};
  sparks(f,x,y,0xfff4b0,large?24:12);
  sparks(f,x,y,0xff7a24,large?24:12);
}
void fc_ko(FcFight *f) {
  if(f->ko_timer) return;
  f->hp=0; f->move=FC_IDLE; f->guarding=false; f->buffered=FC_IDLE;
  f->ko_timer=1; f->hitstop=12; f->stun=0;
  f->override_x=f->override_y=true;
  f->velocity_x=-f->facing*2.5f; f->velocity_y=-5.5f;
  f->events|=FC_SND_KO;
  sparks(f,f->body.box.x+8,f->body.box.y+12,0xff846c,22);
}
bool fc_hurt(FcFight *f,int damage,int from_direction) {
  if(f->hp<=0 || f->invuln || f->flash || f->hitstop || fc_psycho_flight(f) || damage<=0) return false;
  bool blocked=f->guarding && (from_direction==f->facing);
  if(blocked) damage=imax(1,damage/8);
  f->hp=imax(0,f->hp-damage);
  f->combo=f->combo_timer=f->combo_award=0;
  fc_meter(f,blocked?3:8);
  f->stun=blocked?8:22; f->invuln=blocked?14:60;
  f->hitstop=blocked?3:7; f->move=FC_IDLE; f->buffered=FC_IDLE;
  f->override_x=f->override_y=true;
  f->velocity_x=-from_direction*(blocked?1.0f:2.8f);
  f->velocity_y=blocked?0:-3.4f;
  f->events|=blocked?FC_SND_BLOCK:FC_SND_HURT;
  sparks(f,f->body.box.x+8,f->body.box.y+12,blocked?0x85ddff:0xffecae,12);
  if(!f->hp) fc_ko(f);
  return true;
}

/* Stored directions are absolute numpad positions. Mirroring is applied at
 * recognition time, so a platformer changing facing does not corrupt history. */
static uint8_t direction(uint32_t b) {
  int x=!!(b&FC_RIGHT)-!!(b&FC_LEFT),y=!!(b&FC_UP)-!!(b&FC_DOWN);
  return (uint8_t)(5+x+3*y);
}
bool fc_motion(const FcFight *f,const uint8_t *sequence,int count,int window) {
  int want=count-1;
  for(int i=f->motion_count-1;i>=0 && want>=0;i--) {
    if(f->tick-f->motion[i].tick>(uint32_t)window) break;
    int d=f->motion[i].direction;
    if(f->facing<0) d=((d-1)/3)*3+3-(d-1)%3;
    if(d==5) continue;
    if(d==sequence[want]) --want;
    else if(want==count-1) return false;
    else if(want<count-1 && d!=sequence[want+1]) return false;
  }
  return want<0;
}
static void record_input(FcFight *f,uint32_t b) {
  uint8_t d=direction(b);
  if(!f->motion_count || f->motion[f->motion_count-1].direction!=d) {
    if(f->motion_count==32) {
      memmove(f->motion,f->motion+1,31*sizeof(f->motion[0])); --f->motion_count;
    }
    f->motion[f->motion_count++]=(FcDirection){f->tick,d,(int8_t)f->facing};
  }
  if(b&FC_LEFT) { f->charge_left=imin(60,f->charge_left+1); f->left_grace=10; }
  else if(!f->left_grace) f->charge_left=0;
  else --f->left_grace;
  if(b&FC_RIGHT) { f->charge_right=imin(60,f->charge_right+1); f->right_grace=10; }
  else if(!f->right_grace) f->charge_right=0;
  else --f->right_grace;
  if(b&FC_DOWN) { f->charge_down=imin(60,f->charge_down+1); f->down_grace=10; }
  else if(!f->down_grace) f->charge_down=0;
  else --f->down_grace;
}
static FcMove special_move(const FcFight *f,bool secondary) {
  switch(f->character) {
    case FC_HONDA:return secondary?FC_FLURRY:FC_RUSH;
    case FC_CHUNLI:return secondary?FC_SPIN:FC_FLURRY;
    case FC_GUILE:return secondary?FC_RISING:FC_PROJECTILE;
    case FC_BLANKA:return secondary?FC_ELECTRIC:FC_RUSH;
    case FC_ZANGIEF:return secondary?FC_THROW:FC_SPIN;
    case FC_DHALSIM:return secondary?FC_FLAME:FC_PROJECTILE;
    case FC_BALROG:return secondary?FC_RISING:FC_RUSH;
    case FC_VEGA:return secondary?FC_DIVE:FC_RUSH;
    case FC_BISON:return secondary?FC_DIVE:FC_RUSH;
    default:return secondary?FC_RISING:FC_PROJECTILE;
  }
}
static FcMove request(FcFight *f,uint32_t b,uint32_t edge) {
  static const uint8_t qcf[]={2,3,6},qcb[]={2,1,4},dp[]={6,2,3},double_qcf[]={2,3,6,2,3,6},
    hcf[]={4,1,2,3,6},circle[]={6,2,4,8};
  if((edge&FC_SUPER) || ((edge&FC_PUNCH) && fc_motion(f,double_qcf,6,32)))
    return f->meter>=100?FC_SUPER_MOVE:FC_IDLE;
  if(edge&FC_SPECIAL) {
    if((f->character==FC_RYU || f->character==FC_KEN) && (b&FC_DOWN) && !(b&FC_UP))return FC_SPIN;
    if(f->character==FC_DHALSIM && (b&FC_DOWN))return FC_TELEPORT;
    if(f->character==FC_BLANKA && (b&FC_UP))return FC_RISING;
    return special_move(f,!!(b&(FC_UP|FC_DOWN)));
  }
  if(edge&FC_PUNCH) {
    if(f->character==FC_ZANGIEF && fc_motion(f,circle,4,32))return FC_THROW;
    if(f->character==FC_DHALSIM && fc_motion(f,hcf,5,26))return FC_FLAME;
    if((f->character==FC_RYU || f->character==FC_KEN || f->character==FC_SAGAT) && fc_motion(f,dp,3,20)) return FC_RISING;
    if((f->character==FC_RYU || f->character==FC_KEN || f->character==FC_SAGAT || f->character==FC_DHALSIM) && fc_motion(f,qcf,3,20)) return FC_PROJECTILE;
    if((f->character==FC_GUILE || f->character==FC_HONDA || f->character==FC_BLANKA ||
        f->character==FC_BALROG || f->character==FC_VEGA || f->character==FC_BISON) &&
       (((b&FC_RIGHT) && f->charge_left>=30) || ((b&FC_LEFT) && f->charge_right>=30))) {
      f->charge_left=f->charge_right=0;
      return special_move(f,false);
    }
    if(f->character==FC_HONDA && ++f->mash_count>=3) {
      f->mash_count=0; return FC_FLURRY;
    }
    if(f->character==FC_BLANKA && ++f->mash_count>=3) { f->mash_count=0;return FC_ELECTRIC; }
    f->mash_timer=24;
    return FC_JAB;
  }
  if(edge&FC_KICK) {
    if(f->character==FC_RYU || f->character==FC_KEN) {
      /* Walking back changes a platformer's facing. Recognize this motion
       * relative to the direction Ryu faced when the player pressed Down. */
      int facing=f->facing;
      for(int i=f->motion_count-1;i>=0;i--) {
        if(f->tick-f->motion[i].tick>20)break;
        if(f->motion[i].direction==2){f->facing=f->motion[i].facing;break;}
      }
      if(fc_motion(f,qcb,3,20))return FC_SPIN;
      f->facing=facing;
    }
    if(f->character==FC_GUILE && f->charge_down>=30 && (b&FC_UP)) {
      f->charge_down=0; return FC_RISING;
    }
    if(f->charge_down>=30 && (b&FC_UP) &&
       (f->character==FC_BLANKA || f->character==FC_BISON || f->character==FC_VEGA)) {
      f->charge_down=0;return f->character==FC_BLANKA?FC_RISING:FC_DIVE;
    }
    if(f->character==FC_CHUNLI && ++f->mash_count>=3) {
      f->mash_count=0; return FC_FLURRY;
    }
    f->mash_timer=24;
    return FC_ROUNDHOUSE;
  }
  return FC_IDLE;
}
static void start(FcFight *f,FcMove move) {
  f->air_attack=!f->body.grounded || (f->previous&FC_JUMP);
  f->crouching_attack=f->buffered_crouch && !f->air_attack && (move==FC_JAB || move==FC_ROUNDHOUSE);
  f->move=move; f->move_tick=0; f->move_hit=false;
  f->melee_mask=0; f->guarding=false; f->buffered=FC_IDLE; f->buffer_timer=0;
  f->motion_count=0;
  if(move==FC_SUPER_MOVE) {
    f->super_level=imin(3,f->meter/100);
    fc_meter(f,-f->super_level*100);
    f->flash=18; f->invuln=42;
    f->events|=FC_SND_SUPER;
  } else if(move>=FC_PROJECTILE) {
    f->events|=FC_SND_SPECIAL;
    if(move==FC_RISING) f->invuln=12;
  } else f->events|=FC_SND_SWING;
}
static void hit(FcFight *f,FcTarget *t,int damage,bool super) {
  t->hp=imax(0,t->hp-damage); t->hit=true;
  t->knock_x=f->facing*(super?6.5f:3.5f); t->knock_y=super?-6:-4;
  f->move_hit=true;
  if(!fc_psycho_flight(f))f->hitstop=imax(f->hitstop,super?7:4);
  if(f->combo<9999)++f->combo;
  f->combo_timer=90;
  static const int ladder[]={200,400,800,1000,2000,4000,8000,-1,-2,-3};
  int award=ladder[imin(f->combo-1,9)];f->combo_award=award;
  if(award>0)f->score_gain+=award;else f->lives_gain-=award;
  f->rewards[f->reward_count++%6]=(FcReward){t->box.x+t->box.w/2,t->box.y,award};
  if(!super) fc_meter(f,12);
  f->events|=FC_SND_HIT;
  sparks(f,t->box.x+t->box.w/2,t->box.y+t->box.h/2,
    super?0xadeeff:0xffe59b,super?16:9);
}
static void shot(FcFight *f,bool super) {
  for(int i=0;i<FC_MAX_SHOTS;i++) if(!f->shots[i].ttl) {
    float h=super?48+f->super_level*8:20;
    float w=super?48:20;
    f->shots[i]=(FcShot){
      {f->body.box.x+f->body.box.w/2+(f->facing>0?0:-w),
       f->body.box.y+f->body.box.h-h+(super?12:2),w,h},
      f->facing*(super?5.6f:3.8f),95,super?3*f->super_level:1,super?64:1,super,0,false};
    return;
  }
}
bool fc_busy(const FcFight *f) { return f->move!=FC_IDLE || f->stun || f->hp<=0; }
bool fc_psycho_flight(const FcFight *f) {
  return f->character==FC_BISON && f->move==FC_SUPER_MOVE && f->super_level==3 && f->hp>0;
}
const char *fc_move_name(const FcFight *f) {
  if(f->hp<=0) return "K.O.";
  if(f->stun) return f->guarding?"GUARD":"HIT STUN";
  if(f->move==FC_SUPER_MOVE) return fc_roster[f->character].super_name;
  if((f->character==FC_RYU || f->character==FC_KEN) && f->move==FC_SPIN)return "TATSUMAKI";
  if(f->move==FC_TELEPORT)return "YOGA TELEPORT";
  if(f->move==FC_THROW || f->move==FC_ELECTRIC || f->move==FC_FLAME || f->move==FC_DIVE)
    return fc_roster[f->character].secondary;
  if(f->move==FC_RISING || f->move==FC_SPIN || (f->move==FC_FLURRY && f->character==FC_HONDA))
    return fc_roster[f->character].secondary;
  if(f->move>=FC_PROJECTILE) return fc_roster[f->character].special;
  if(f->move==FC_JAB) return "PUNCH";
  if(f->move==FC_ROUNDHOUSE) return "KICK";
  return "";
}
void fc_tick(FcFight *f,uint32_t buttons,FcBody body,FcTarget *targets,int count) {
  float dx=body.box.x-f->body.box.x,dy=body.box.y-f->body.box.y;
  if(dx<0)dx=-dx;
  if(!f->flash && !f->hitstop) {
    bool intent=(buttons&(FC_LEFT|FC_RIGHT)) && !(buttons&FC_DOWN);
    f->moving=f->body_valid && body.grounded && intent && dx>0.1f && dx<16;
    if(f->moving){f->walk_distance+=dx;f->walk_pause=0;}
    else if(!intent || !body.grounded || dx>=16 || ++f->walk_pause>2)f->walk_distance=0;
    else if(f->walk_distance>0)f->moving=true;
    f->vertical_motion=f->body_valid?dy:0;
    if(body.grounded) {
      if(f->body_valid && !f->body.grounded && f->air_attack &&
          (f->move==FC_JAB || f->move==FC_ROUNDHOUSE)) {
        f->move=FC_IDLE;f->move_tick=0;f->air_attack=false;
      }
      f->air_tick=f->fall_tick=0;
    } else {
      if(!f->body_valid || f->body.grounded) {
        f->air_tick=f->fall_tick=0;
        f->jump_forward=dx>0.1f || (buttons&(FC_LEFT|FC_RIGHT));
      }
      if(dy<0 && f->fall_tick)f->air_tick=f->fall_tick=0;
      ++f->air_tick;
      if(dy>0 || f->fall_tick)++f->fall_tick;
      if(f->move==FC_JAB || f->move==FC_ROUNDHOUSE)f->air_attack=true;
    }
    ++f->animation_tick;
  }
  f->body_valid=true;
  f->events=0; f->body=body; ++f->tick;
  f->reward_count=f->score_gain=f->lives_gain=0;
  uint32_t edge=buttons&~f->previous; f->previous=buttons;
  if(!fc_busy(f) && body.facing) f->facing=body.facing;
  record_input(f,buttons);
  FcMove req=request(f,buttons,edge);
  if(req) { f->buffered=req; f->buffer_timer=7; f->buffered_crouch=!!(buttons&FC_DOWN); }
  /* Hitstop and the super flash freeze both combat and the host's physics.
   * Buffered inputs survive the freeze; velocities survive until consumed. */
  f->frozen=false;
  if(f->flash || f->hitstop) {
    if(f->flash) --f->flash; else --f->hitstop;
    f->frozen=true; return;
  }
  f->override_x=f->override_y=false;
  dec(&f->invuln); dec(&f->combo_timer); dec(&f->mash_timer);
  if(fc_psycho_flight(f) && f->combo)f->combo_timer=90;
  if(!f->combo_timer) f->combo=0;
  if(!f->mash_timer) f->mash_count=0;
  for(int i=0;i<FC_MAX_BLASTS;i++)dec(&f->blasts[i].ttl);
  for(int i=0;i<FC_MAX_SPARKS;i++) if(f->sparks[i].ttl) {
    FcSpark *s=&f->sparks[i]; --s->ttl; s->x+=s->vx; s->y+=s->vy; s->vy+=0.18f;
  }
  if(f->hp<=0) {
    if(f->ko_timer==1)f->override_x=f->override_y=true;
    ++f->ko_timer; return;
  }
  if(f->stun) {
    --f->stun; f->override_x=true;
    /* Keep the initial launch impulse through hitstop, then apply once. */
    f->velocity_x*=0.91f;
    if(f->stun==21 || (f->guarding && f->stun==7)) f->override_y=true;
  } else {
    bool normal=f->move==FC_JAB || f->move==FC_ROUNDHOUSE;
    bool cancel=f->move_hit && ((normal && f->buffered>=FC_PROJECTILE) || f->buffered==FC_SUPER_MOVE);
    if(f->buffer_timer && (f->move==FC_IDLE || cancel)) {
      if(f->buffered!=FC_SUPER_MOVE || f->meter>=100) start(f,f->buffered);
      else f->buffer_timer=0;
    }
    f->guarding=(buttons&FC_GUARD) && f->move==FC_IDLE && body.grounded;
    if(f->flash) { f->frozen=true; return; }
    if(f->move) {
      ++f->move_tick;
      int t=f->move_tick,begin=4,end=7,total=17,damage=1;
      float reach=18,height=18;
      bool super=f->move==FC_SUPER_MOVE,melee=true;
      /* Normals keep platform movement. Airborne projectiles keep momentum. */
      f->override_x=false;
      switch(f->move) {
        case FC_ROUNDHOUSE:begin=7;end=12;total=26;reach=29;damage=2;break;
        case FC_PROJECTILE:
          melee=false;total=25;
          if(body.grounded && t<=10){f->override_x=true;f->velocity_x=0;}
          if(t==6)shot(f,false);break;
        case FC_RISING:
          f->override_x=true;f->velocity_x=0;
          begin=2;end=20;total=34;height=42;damage=2;
          if(t==2) { f->override_y=true;f->velocity_y=-5.4f; }
          if(t<=18) f->velocity_x=f->facing*1.2f;
          if(f->character==FC_BLANKA){reach=32;height=44;}
          break;
        case FC_SPIN:
          f->override_x=true;f->velocity_x=0;
          begin=4;end=31;total=40;reach=30;damage=1;
          if(t==4 && body.grounded) { f->override_y=true;f->velocity_y=-3; }
          if(t<31)f->velocity_x=f->facing*2.2f;
          if(f->character==FC_ZANGIEF){reach=64;f->velocity_x=0;if(t%9==0)f->melee_mask=0;}
          break;
        case FC_RUSH:
          f->override_x=true;f->velocity_x=0;
          begin=7;end=29;total=39;reach=23;damage=3;
          if(t>=begin && t<=end)f->velocity_x=f->facing*4.0f;
          if(f->character==FC_BLANKA || f->character==FC_BISON) {
            height=36;reach=40;f->invuln=2;
            if(!body.grounded){f->override_y=true;f->velocity_y=0;}
          }
          break;
        case FC_THROW:
          begin=3;end=24;total=44;reach=50;height=54;damage=6;
          f->override_x=true;f->velocity_x=0;f->invuln=2;
          if(t==4){f->override_y=true;f->velocity_y=-5.0f;}
          break;
        case FC_TELEPORT:
          melee=false;total=24;f->invuln=2;
          f->override_x=true;f->velocity_x=t>5 && t<19?f->facing*6:0;
          f->override_y=true;f->velocity_y=0;
          break;
        case FC_ELECTRIC:
          begin=1;end=40;total=48;reach=70;height=46;damage=1;
          f->override_x=true;f->velocity_x=0;
          if(t%7==0)f->melee_mask=0;
          break;
        case FC_FLAME:
          begin=6;end=33;total=42;reach=86;height=42;damage=2;
          f->override_x=true;f->velocity_x=0;
          if(t%9==0)f->melee_mask=0;
          break;
        case FC_DIVE:
          begin=3;end=40;total=48;reach=36;height=52;damage=4;
          f->override_x=true;f->velocity_x=f->facing*(t<20?2.0f:3.4f);
          if(t==3){f->override_y=true;f->velocity_y=-6.5f;f->invuln=15;}
          if(t==22){f->override_y=true;f->velocity_y=5.2f;}
          break;
        case FC_FLURRY:
          if(body.grounded){f->override_x=true;f->velocity_x=0;}
          begin=3;end=37;total=46;reach=30;damage=1;
          if(t%8==0)f->melee_mask=0;
          break;
        case FC_SUPER_MOVE:
          f->override_x=body.grounded;f->velocity_x=0;
          begin=1;end=44+f->super_level*8;total=end+8;reach=56;height=64;damage=3;
          if(f->character==FC_RYU) { melee=false;if(t==3)shot(f,true); }
          else if(f->character==FC_GUILE) {
            reach=64;height=52;
            if(t==3 || t==17 || (t==31 && f->super_level>1))shot(f,true);
          } else if(f->character==FC_KEN || f->character==FC_SAGAT) {
            reach=50;height=72;f->override_x=true;f->velocity_x=f->facing*2.8f;
            if(t==3 || t==23 || (t==43 && f->super_level>1)){f->override_y=true;f->velocity_y=-5.7f;}
          } else if(f->character==FC_DHALSIM) {
            reach=110;height=72;if(t==3 || t==24)shot(f,true);
          } else if(f->character==FC_ZANGIEF) {
            reach=110;height=90;damage=5;f->invuln=2;
            if(t==3 || t==32){f->override_y=true;f->velocity_y=-4.8f;}
          } else if(t<=end){f->override_x=true;f->velocity_x=f->facing*(f->character==FC_HONDA?5.2f:4.3f);}
          if(fc_psycho_flight(f)) {
            begin=1;end=1199;total=1200;reach=280;height=240;damage=999;
            f->override_x=f->override_y=true;f->velocity_x=f->facing*7.5f;
            f->velocity_y=((buttons&FC_DOWN)?3.0f:0)-((buttons&FC_UP)?3.0f:0);
            f->invuln=2;f->melee_mask=0;
          }
          if(t==1)f->events|=FC_SND_SPECIAL;
          if(t%8==0)f->melee_mask=0;
          break;
        default:break;
      }
      if(f->character==FC_DHALSIM && f->move<=FC_ROUNDHOUSE)reach+=38;
      if(f->character==FC_VEGA && f->move<=FC_ROUNDHOUSE)reach+=12;
      if(f->crouching_attack && body.grounded && f->move<=FC_ROUNDHOUSE) {
        height=f->move==FC_JAB?14:9;f->override_x=true;f->velocity_x=0;
      }
      if(melee && t>=begin && t<=end) {
        FcBox box={body.box.x+(f->facing>0?body.box.w-8:8-reach),
          body.box.y+body.box.h-height,reach,height+(super?12:4)};
        if(f->move==FC_ELECTRIC || (f->character==FC_ZANGIEF &&
            (f->move==FC_SPIN || f->move==FC_SUPER_MOVE || f->move==FC_THROW)))box.x=body.box.x+body.box.w/2-reach/2;
        if(fc_psycho_flight(f))box=(FcBox){body.box.x-130,body.box.y-100,280,260};
        for(int i=0;i<count && i<FC_MAX_TARGETS;i++) {
          uint64_t bit=UINT64_C(1)<<i;
          if(targets[i].active && targets[i].hp>0 && !(f->melee_mask&bit) && fc_overlap(box,targets[i].box)) {
            f->melee_mask|=bit; hit(f,&targets[i],damage,super);
          }
        }
      }
      if(t>=total) { f->move=FC_IDLE;f->move_tick=0;f->crouching_attack=f->air_attack=false; }
    }
  }
  dec(&f->buffer_timer);
  if(!f->buffer_timer)f->buffered=FC_IDLE;
  for(int j=0;j<FC_MAX_SHOTS;j++) {
    FcShot *p=&f->shots[j];if(!p->ttl)continue;
    float old_x=p->box.x; p->box.x+=p->vx; --p->ttl;
    FcBox sweep=p->box;
    if(p->vx>0) { sweep.x=old_x;sweep.w+=p->vx; } else sweep.w-=p->vx;
    for(int i=0;i<count && i<FC_MAX_TARGETS && p->pierce;i++) {
      uint64_t bit=UINT64_C(1)<<i;
      if(targets[i].active && targets[i].hp>0 && !(p->hit_mask&bit) && fc_overlap(sweep,targets[i].box)) {
        p->hit_mask|=bit;hit(f,&targets[i],p->damage,p->super);--p->pierce;
        targets[i].knock_x=(p->vx>0?1:-1)*(p->super?6.5f:3.5f);
        if(!p->pierce)p->ttl=0;
      }
    }
  }
}
