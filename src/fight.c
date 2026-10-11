#include "fight.h"
#include <string.h>

const FcCharacter fc_roster[FC_ROSTER_COUNT] = {
  {"RYU", "ANSATSUKEN", "HADOUKEN", "SHORYUKEN", "SHINKU HADOUKEN", 0x64e7ff,0xe8f3eb,0xefb381,100,2.3f},
  {"E. HONDA", "SUMO", "SUMO HEADBUTT", "HUNDRED HAND SLAP", "SUPER ONI MUSOU", 0x88adff,0x456fc0,0xeab382,125,1.85f},
  {"CHUN-LI", "KUNG FU", "LIGHTNING LEGS", "SPINNING BIRD KICK", "SENRETSUKYAKU", 0xffcf65,0x4284ee,0xf5c398,90,2.75f},
  {"GUILE", "AIR FORCE", "SONIC BOOM", "FLASH KICK", "SONIC HURRICANE", 0x9bef91,0x82a65c,0xe8b684,110,2.2f}
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
  if(f->hp<=0 || f->invuln || f->flash || f->hitstop || damage<=0) return false;
  bool blocked=f->guarding && (from_direction==f->facing);
  if(blocked) damage=imax(1,damage/8);
  f->hp=imax(0,f->hp-damage);
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
    f->motion[f->motion_count++]=(FcDirection){f->tick,d};
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
    default:return secondary?FC_RISING:FC_PROJECTILE;
  }
}
static FcMove request(FcFight *f,uint32_t b,uint32_t edge) {
  static const uint8_t qcf[]={2,3,6},dp[]={6,2,3},double_qcf[]={2,3,6,2,3,6};
  if((edge&FC_SUPER) || ((edge&FC_PUNCH) && fc_motion(f,double_qcf,6,32)))
    return f->meter>=100?FC_SUPER_MOVE:FC_IDLE;
  if(edge&FC_SPECIAL) return special_move(f,!!(b&(FC_UP|FC_DOWN)));
  if(edge&FC_PUNCH) {
    if(f->character==FC_RYU && fc_motion(f,dp,3,20)) return FC_RISING;
    if(f->character==FC_RYU && fc_motion(f,qcf,3,20)) return FC_PROJECTILE;
    if((f->character==FC_GUILE || f->character==FC_HONDA) &&
       (((b&FC_RIGHT) && f->charge_left>=30) || ((b&FC_LEFT) && f->charge_right>=30))) {
      f->charge_left=f->charge_right=0;
      return special_move(f,false);
    }
    if(f->character==FC_HONDA && ++f->mash_count>=3) {
      f->mash_count=0; return FC_FLURRY;
    }
    f->mash_timer=24;
    return FC_JAB;
  }
  if(edge&FC_KICK) {
    if(f->character==FC_GUILE && f->charge_down>=30 && (b&FC_UP)) {
      f->charge_down=0; return FC_RISING;
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
  f->move_hit=true; f->hitstop=imax(f->hitstop,super?7:4);
  ++f->combo; f->combo_timer=90;
  if(!super) fc_meter(f,12);
  f->events|=FC_SND_HIT;
  sparks(f,t->box.x+t->box.w/2,t->box.y+t->box.h/2,
    super?0xadeeff:0xffe59b,super?16:9);
}
static void shot(FcFight *f,bool super) {
  for(int i=0;i<FC_MAX_SHOTS;i++) if(!f->shots[i].ttl) {
    float h=super?24+f->super_level*5:12;
    f->shots[i]=(FcShot){
      {f->body.box.x+(f->facing>0?f->body.box.w:-20),
       f->body.box.y+f->body.box.h/2-h/2,super?32:16,h},
      f->facing*(super?5.6f:3.8f),95,super?3*f->super_level:1,super?64:1,super,0};
    return;
  }
}
bool fc_busy(const FcFight *f) { return f->move!=FC_IDLE || f->stun || f->hp<=0; }
const char *fc_move_name(const FcFight *f) {
  if(f->hp<=0) return "K.O.";
  if(f->stun) return f->guarding?"GUARD":"HIT STUN";
  if(f->move==FC_SUPER_MOVE) return fc_roster[f->character].super_name;
  if(f->move==FC_RISING || f->move==FC_SPIN || (f->move==FC_FLURRY && f->character==FC_HONDA))
    return fc_roster[f->character].secondary;
  if(f->move>=FC_PROJECTILE) return fc_roster[f->character].special;
  if(f->move==FC_JAB) return "PUNCH";
  if(f->move==FC_ROUNDHOUSE) return "KICK";
  return "";
}
void fc_tick(FcFight *f,uint32_t buttons,FcBody body,FcTarget *targets,int count) {
  f->events=0; f->body=body; ++f->tick;
  uint32_t edge=buttons&~f->previous; f->previous=buttons;
  if(!fc_busy(f) && body.facing) f->facing=body.facing;
  record_input(f,buttons);
  FcMove req=request(f,buttons,edge);
  if(req) { f->buffered=req; f->buffer_timer=7; }
  /* Hitstop and the super flash freeze both combat and the host's physics.
   * Buffered inputs survive the freeze; velocities survive until consumed. */
  f->frozen=false;
  if(f->flash || f->hitstop) {
    if(f->flash) --f->flash; else --f->hitstop;
    f->frozen=true; return;
  }
  f->override_x=f->override_y=false;
  dec(&f->invuln); dec(&f->combo_timer); dec(&f->mash_timer);
  if(!f->combo_timer) f->combo=0;
  if(!f->mash_timer) f->mash_count=0;
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
      f->override_x=true; f->velocity_x=0;
      switch(f->move) {
        case FC_ROUNDHOUSE:begin=7;end=12;total=26;reach=29;damage=2;break;
        case FC_PROJECTILE:
          melee=false;total=30;if(t==8)shot(f,false);break;
        case FC_RISING:
          begin=2;end=20;total=34;height=42;damage=2;
          if(t==2) { f->override_y=true;f->velocity_y=-5.4f; }
          if(t<=18) f->velocity_x=f->facing*1.2f;
          break;
        case FC_SPIN:
          begin=4;end=31;total=40;reach=30;damage=1;
          if(t==4 && body.grounded) { f->override_y=true;f->velocity_y=-3; }
          if(t<31)f->velocity_x=f->facing*2.2f;
          break;
        case FC_RUSH:
          begin=7;end=29;total=39;reach=23;damage=3;
          if(t>=begin && t<=end)f->velocity_x=f->facing*4.0f;
          break;
        case FC_FLURRY:
          begin=3;end=37;total=46;reach=30;damage=1;
          if(t%8==0)f->melee_mask=0;
          break;
        case FC_SUPER_MOVE:
          begin=2;end=44+f->super_level*8;total=end+12;reach=38;damage=2;
          if(f->character==FC_RYU) { melee=false;if(t==3)shot(f,true); }
          else if(f->character==FC_GUILE) {
            reach=64;height=52;
            if(t==3 || t==17 || (t==31 && f->super_level>1))shot(f,true);
          } else if(t<=end)f->velocity_x=f->facing*(f->character==FC_HONDA?5.2f:3.6f);
          if(t%8==0)f->melee_mask=0;
          break;
        default:break;
      }
      if(melee && t>=begin && t<=end) {
        FcBox box={body.box.x+(f->facing>0?body.box.w-3:3-reach),
          body.box.y+body.box.h-height,reach,height};
        for(int i=0;i<count && i<FC_MAX_TARGETS;i++) {
          uint64_t bit=UINT64_C(1)<<i;
          if(targets[i].active && targets[i].hp>0 && !(f->melee_mask&bit) && fc_overlap(box,targets[i].box)) {
            f->melee_mask|=bit; hit(f,&targets[i],damage,super);
          }
        }
      }
      if(t>=total) { f->move=FC_IDLE;f->move_tick=0; }
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
