#include "ww_targets.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <string.h>

typedef struct { int tag,hp,max_hp,cooldown; bool alive,boss,dead; } TargetState;
static TargetState state[WW_TARGET_COUNT];
static FcOpponent opponent;
static bool boss_ko;
static unsigned invalidated;
static struct { int x,y; uint32_t until; } tile_hits[64];

void ww_targets_reset(void) {
  memset(state,0,sizeof(state));opponent=(FcOpponent){0};boss_ko=false;invalidated=0;
  memset(tile_hits,0,sizeof(tile_hits));
}
void ww_target_spawned(unsigned k) {
  if(k<12) { state[k]=(TargetState){0};invalidated|=1u<<k; }
}
const FcOpponent *ww_opponent(void) { return &opponent; }
static void clear_mask(FcFight *f,int k) {
  uint64_t mask=~(UINT64_C(1)<<k);f->melee_mask&=mask;
  for(int j=0;j<FC_MAX_SHOTS;j++)f->shots[j].hit_mask&=mask;
}
static bool is_boss(int k) {
  int id=spr_spriteid[k];
  return id==0xa0 || id==0xa9 || id==0xc5 ||
    (id==0x29 && (spr_table00c2[k]<5 || !spr_table1570[k]));
}
/* These objects still take hits and explode. Their underlying interaction
 * survives: a key, ride, exit or switch must never disappear from a puzzle. */
static bool essential(int id) {
  if((id>=0x55 && id<=0x63) || (id>=0x74 && id<=0x84))return true;
  switch(id) {
    case 0x0e:case 0x19:case 0x2c:case 0x2f:case 0x35:case 0x3e:
    case 0x41:case 0x42:case 0x43:case 0x45:case 0x49:case 0x4a:
    case 0x52:case 0x53:case 0x54:case 0x6b:case 0x6c:case 0x87:case 0x88:
    case 0x8c:case 0x8d:case 0x8e:case 0x8f:case 0x9c:
    case 0xb1:case 0xb7:case 0xb8:case 0xb9:case 0xba:case 0xbb:
    case 0xc0:case 0xc1:case 0xc4:case 0xc6:case 0xc8:
      return true;
    default:return id>=0xc9; /* Unknown controllers stay functional too. */
  }
}
static int boss_max(int k) { return spr_spriteid[k]==0xa9?6:spr_spriteid[k]==0xa0?30:18; }
static const char *boss_name(int k) {
  switch(spr_spriteid[k]) {
    case 0xa0:return "BOWSER";case 0xa9:return "REZNOR";case 0xc5:return "BIG BOO";
    default: {
      static const char *names[]={"MORTON","ROY","LUDWIG","IGGY","LARRY","WENDY","LEMMY"};
      return names[spr_table00c2[k]%7];
    }
  }
}
/* Reflect native stomps, thrown shells and the original death states in the
 * same health bar. Combat never replaces the engine's exit/cutscene logic. */
static int native_hp(int k,int maximum) {
  int id=spr_spriteid[k];
  if(id==0xa9)return spr_table151c[k]?0:maximum;
  if(id==0xc5)return spr_table00c2[k]==6?0:maximum*(3-(spr_table1534[k]>3?3:spr_table1534[k]))/3;
  if(id==0xa0) {
    int phase=spr_table151c[k];
    if(phase>=4 && phase<=6)return 0;
    if(phase>=7 && phase<=9 && spr_table187b[k]<=2)
      return maximum*((9-phase)*2+spr_table187b[k])/6;
    return maximum;
  }
  int kind=spr_table00c2[k];
  if(kind==3 || kind==4)return spr_table160e[k] || spr_decrementing_table163e[k]?0:maximum;
  int hits=kind<3?spr_table1626[k]:spr_table1534[k];
  if((kind<3 && spr_table151c[k]>=4) || (kind>=5 && spr_table151c[k]>=5))return 0;
  return maximum*(3-(hits>3?3:hits))/3;
}
static FcBox normal_box(int k) {
  CollInfo ci={0};StandardSpriteToSpriteCollisionChecks_GetSpriteClippingB((uint8)k,&ci);
  FcBox b={(float)(int16)(ci.r0|(ci.r8<<8)),(float)(int16)(ci.r1|(ci.r9<<8)),ci.r2,ci.r3};
  /* Several invincible props have no useful damage hull. Give them one. */
  if(b.w<8 || b.h<8)b=(FcBox){GetSprXPos(k),GetSprYPos(k),16,16};
  if(spr_spriteid[k]==0x29) {
    int kind=spr_table00c2[k];
    if(kind==3 || kind==4) {
      PointU16 pt=Spr029_KoopaKid_IggyLarry_01FD50((uint8)k);
      b=(FcBox){(int16)((uint8)(pt.x-12)+mirror_current_layer1_xpos),
        (int16)((uint8)(pt.y+80)+mirror_current_layer1_ypos),28,36};
    } else b=(FcBox){GetSprXPos(k)-12,GetSprYPos(k),32,40};
  } else if(spr_spriteid[k]==0xa0)b=(FcBox){GetSprXPos(k)-16,GetSprYPos(k)-8,80,80};
  else if(spr_spriteid[k]==0xa9)b=(FcBox){GetSprXPos(k)-16,GetSprYPos(k)-32,40,40};
  else if(spr_spriteid[k]==0xc5)b=(FcBox){GetSprXPos(k)-8,GetSprYPos(k)-8,64,64};
  else if(spr_spriteid[k]==0xac || spr_spriteid[k]==0xad)
    b=(FcBox){GetSprXPos(k),GetSprYPos(k),16,80};
  else if(spr_spriteid[k]==0x89)
    b=(FcBox){mirror_current_layer1_xpos+48-(int16)GetSprXPos(k),
      (int16)mirror_current_layer1_ypos-128,144,144+spr_ypos_lo[k]};
  /* Boss cameras and actors wrap through $ffff above the screen. Match
   * the signed world coordinates used by the player and projectile hulls. */
  b.x=(int16)(int)b.x;b.y=(int16)(int)b.y;
  /* Generous padding makes small, fast and oddly shaped sprites hittable. */
  b.x-=3;b.y-=3;b.w+=6;b.h+=6;return b;
}
static void update_opponent(FcFight *f) {
  int hp=0,maximum=0;const char *name=NULL;
  for(int k=0;k<12;k++)if(state[k].boss) {
    hp+=state[k].hp;maximum+=state[k].max_hp;name=boss_name(k);
  }
  if(!maximum)return;
  opponent.active=true;opponent.name=name;opponent.hp=hp;opponent.max_hp=maximum;
  if(!hp && !boss_ko) {
    boss_ko=true;opponent.ko_timer=180;f->events|=FC_SND_BOSS_KO;
  }
}
void ww_targets_collect(FcFight *f,FcTarget *targets) {
  memset(targets,0,sizeof(*targets)*WW_TARGET_COUNT);
  if(opponent.ko_timer>0)--opponent.ko_timer;
  for(int k=0;k<WW_TARGET_COUNT;k++) {
    TargetState *s=&state[k];int tag;bool alive;FcBox box;
    if(k<12) {
      tag=spr_spriteid[k]+1;
      alive=spr_current_status[k]>=8 && spr_current_status[k]<=11;
      box=alive?normal_box(k):(FcBox){0};
    } else if(k<22) {
      int j=k-12;tag=ext_spr_spriteid[j];alive=tag!=0;
      box=(FcBox){ext_spr_xpos_lo[j]+256*ext_spr_xpos_hi[j]-4,
        ext_spr_ypos_lo[j]+256*ext_spr_ypos_hi[j]-4,16,16};
    } else {
      int j=k-22;tag=cluster_spr_spriteid[j];alive=tag!=0;
      box=(FcBox){cluster_spr_xpos_lo[j]+256*cluster_spr_xpos_hi[j]-4,
        cluster_spr_ypos_lo[j]+256*cluster_spr_ypos_hi[j]-4,24,24};
    }
    box.x=(int16)(int)box.x;box.y=(int16)(int)box.y;
    if(!alive) {
      if(s->boss && (s->dead || timer_end_level || misc_currently_active_boss_end_cutscene))s->hp=0;
      else *s=(TargetState){0};
      s->alive=false;continue;
    }
    if(!s->alive || s->tag!=tag || (k<12 && (invalidated&(1u<<k)))) {
      bool boss=k<12 && is_boss(k);int hp=boss?boss_max(k):1;
      *s=(TargetState){tag,hp,hp,0,true,boss,false};clear_mask(f,k);
    }
    if(s->boss) {
      int hp=native_hp(k,s->max_hp);if(hp<s->hp)s->hp=hp;
      if(!s->hp)s->dead=true;
    }
    if(s->cooldown)--s->cooldown;
    targets[k]=(FcTarget){box,s->hp,!s->dead && !s->cooldown,false,0,0};
  }
  invalidated=0;
}
static void defeat_boss(int k) {
  switch(spr_spriteid[k]) {
    case 0xa9:
      /* Keep all four platforms, especially slot 7: it runs the exit timer. */
      spr_table151c[k]=1;spr_decrementing_table1558[k]=0;break;
    case 0xc5:
      spr_table1534[k]=3;spr_table00c2[k]=6;DespawnNonBossSprites();break;
    case 0xa0:
      /* Bowser's native death leads into the clown-car/Peach ending. */
      spr_table151c[k]=4;spr_xspeed[k]=spr_yspeed[k]=0;
      /* A super can reach him during an offscreen swoop. The native rise
       * uses an unsigned Y comparison and would never finish from $ffxx. */
      if((int16)GetSprYPos(k)<16)SetSprYPos((uint8)k,16);
      bowserbattle_var14b5=bowserbattle_var14b1=bowserbattle_var14b6=0;break;
    case 0x29: {
      int kind=spr_table00c2[k];
      if(kind<3) {
        spr_table1626[k]=3;Spr029_KoopaKid_MortonRoyLudwig_01CFD0((uint8)k);
      } else if(kind<5) {
        PointU16 pt=Spr029_KoopaKid_IggyLarry_01FD50((uint8)k);
        SetSprXYPos((uint8)k,pt.x,pt.y);spr_table160e[k]=1;
        spr_xspeed[k]=0;spr_yspeed[k]=16;
      } else {
        spr_table1534[k]=3;spr_table151c[k]=5;spr_yspeed[k]=0;
      }
      break;
    }
  }
}
void ww_targets_apply(FcFight *f,FcTarget *targets) {
  for(int k=0;k<WW_TARGET_COUNT;k++)if(targets[k].hit) {
    TargetState *s=&state[k];FcTarget *t=&targets[k];s->hp=t->hp;
    fc_explode(f,t->box.x+t->box.w/2,t->box.y+t->box.h/2,s->boss || f->move==FC_SUPER_MOVE);
    if(s->hp>0)continue;
    if(s->boss) { s->dead=true;defeat_boss(k); }
    else if(k<12 && essential(spr_spriteid[k])) { s->hp=1;s->cooldown=30; }
    else {
      if(k<12) {
        if(spr_spriteid[k]==0x89) {
          SetSprXYPos((uint8)k,256,0);
          Spr089_Layer3Smasher_UpdatePos((uint8)k);
        }
        if(spr_spriteid[k]==0xac || spr_spriteid[k]==0xad)
          for(int y=12;y<80;y+=24)fc_explode(f,t->box.x+t->box.w/2,t->box.y+y,false);
        /* Do not call per-species death code: many formerly invincible
         * actors have none. Our explosion replaces their drawing safely. */
        spr_current_status[k]=0;GivePoints((uint8)k,1);
      } else if(k<22)ext_spr_spriteid[k-12]=0;
      else cluster_spr_spriteid[k-22]=0;
      s->alive=false;
    }
  }
  update_opponent(f);
}
/* Terrain gets the same explosion treatment without erasing doors, pipe
 * entrances, question-block contents or the ground needed to reach an exit.
 * Read the native Map16 layout directly; never borrow collision scratch RAM. */
static bool blast_tiles(FcFight *f,FcBox box,bool large) {
  for(int y=(int)box.y&~15;y<box.y+box.h;y+=16)
    for(int x=(int)box.x&~15;x<box.x+box.w;x+=16) {
      if(x<0 || y<0)continue;
      unsigned offset;
      if(misc_level_layout_flags&1) {
        if(x>=512 || (y>>8)>=misc_screens_in_lvl || (y>>8)>=14)continue;
        offset=GetLevelLayoutPtr_Vertical(y>>8)+(y&0xf0)+((x&255)>>4)+((x>>8)<<8);
      } else {
        if(y>=432 || (x>>8)>=misc_screens_in_lvl || (x>>8)>=32)continue;
        offset=GetLevelLayoutPtr_Horizontal(x>>8)+(y&0xf0)+((x&255)>>4)+((y>>8)<<8);
      }
      if(offset>=0x10000)continue;
      unsigned tile=g_ram[offset]+256*g_ram[0x10000+offset];
      if(tile==0x25)continue;
      unsigned h=((unsigned)(x/16)*13u+(unsigned)(y/16)*7u)%64;
      if(tile_hits[h].until>f->tick && tile_hits[h].x==x && tile_hits[h].y==y)return true;
      tile_hits[h].x=x;tile_hits[h].y=y;tile_hits[h].until=f->tick+45;
      fc_explode(f,x+8,y+8,large);
      return true;
    }
  return false;
}
void ww_scenery_hits(FcFight *f) {
  if(f->frozen)return;
  for(int i=0;i<FC_MAX_SHOTS;i++) {
    FcShot *p=&f->shots[i];
    if(!p->ttl || p->scenery_hit)continue;
    /* One impact at the projectile's nose, not a burning trail along every
     * floor tile touched by the forgiving combat hitbox. */
    FcBox nose={p->box.x+(p->vx>0?p->box.w:0),p->box.y+p->box.h/2,1,1};
    p->scenery_hit=blast_tiles(f,nose,p->super);
  }
  if(f->move && f->move!=FC_PROJECTILE && f->move_tick==4) {
    FcBox b=f->body.box;
    FcBox fist={b.x+(f->facing>0?b.w+16:-16),b.y+b.h/2,1,1};
    blast_tiles(f,fist,f->move==FC_SUPER_MOVE);
  }
}
