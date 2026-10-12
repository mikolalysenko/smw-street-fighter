#include "ww_targets.h"
#include "ww_finalboss.h"
#include "ww_smw.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include "consts.h"
#include "snes/ppu.h"
#include <math.h>
#include <string.h>

typedef struct { int tag,hp,max_hp,cooldown; bool alive,boss,dead; } TargetState;
static TargetState state[WW_TARGET_COUNT];
static FcOpponent opponent;
static bool boss_ko;
static unsigned invalidated;
static struct { int slot,tag,x,y,w,h; bool captured; uint32_t pixels[256*256]; } grab={.slot=-1};
typedef struct {int x,y,id;} PipeTile;
static struct { int x,y,w,h,count; PipeTile tile[48]; } pipe_grab;
static uint32_t pipe_painted_tick;
extern Ppu *g_ppu;
void WW_OriginalHandleSprite(uint8 k);
static struct { int x,y; uint32_t until; } tile_hits[64];
static unsigned map_tile(int x,int y) {
  if(x<0 || y<0)return 0x25;
  unsigned o;
  if(misc_level_layout_flags&1) {
    if(x>=512 || (y>>8)>=misc_screens_in_lvl || (y>>8)>=14)return 0x25;
    o=GetLevelLayoutPtr_Vertical(y>>8)+(y&0xf0)+((x&255)>>4)+((x>>8)<<8);
  } else {
    if(y>=432 || (x>>8)>=misc_screens_in_lvl || (x>>8)>=32)return 0x25;
    o=GetLevelLayoutPtr_Horizontal(x>>8)+(y&0xf0)+((x&255)>>4)+((y>>8)<<8);
  }
  return o<0x10000?g_ram[o]+256*g_ram[o+0x10000]:0x25;
}
static bool pipe_tile(unsigned id) { return id>=0x133 && id<=0x13f; }
static void pipe_tiles(bool hidden) {
  /* Move the art, keeping the native collision and exit metadata intact.
   * A temporary invisible pipe cannot drop its rider or erase a required
   * route, and the exact original tiles return after the slam or cancel. */
  uint8 old_layer=misc_current_layer_being_processed;misc_current_layer_being_processed=0;
  for(int i=0;i<pipe_grab.count;i++) {
    int x=pipe_grab.tile[i].x,y=pipe_grab.tile[i].y;
    unsigned address=0x2000+((x&0x100)?0x400:0)+((y&0x100)?0x800:0)+((y&0xf0)<<2)+((x&0xf0)>>3);
    GenTileArgs gta={0};gta.r6=address>>8;gta.r7=address;
    gta.r12=x;gta.r14=y;gta.r8=x-16;gta.r10=y-16;
    GenericPage01Tile_00C0FB(&gta,2*(hidden?0x25:pipe_grab.tile[i].id));
  }
  misc_current_layer_being_processed=old_layer;
}
static bool find_pipe(const FcFight *f) {
  if(grab.slot==WW_PIPE_TARGET)return true;
  pipe_grab.count=0;
  if(!fc_piledriver(f) || f->grab_slam || misc_nmito_use_flag&0x80)return false;
  int x0=((int)f->body.box.x-32)&~15,y0=((int)f->body.box.y-32)&~15;
  for(int y=y0;y<y0+96 && !pipe_grab.count;y+=16)for(int x=x0;x<x0+80;x+=16) {
    int id=map_tile(x,y);if(!pipe_tile(id))continue;
    pipe_grab.tile[0]=(PipeTile){x,y,id};pipe_grab.count=1;break;
  }
  if(!pipe_grab.count)return false;
  int left=pipe_grab.tile[0].x,right=left+16,top=pipe_grab.tile[0].y,bottom=top+16;
  for(int i=0;i<pipe_grab.count && pipe_grab.count<48;i++)for(int d=0;d<4;d++) {
    int x=pipe_grab.tile[i].x+(d==0?16:d==1?-16:0),y=pipe_grab.tile[i].y+(d==2?16:d==3?-16:0);
    if(x<left-192 || x>=left+192 || y<top-192 || y>=top+192)continue;
    int id=map_tile(x,y);if(!pipe_tile(id))continue;
    bool seen=false;for(int j=0;j<pipe_grab.count;j++)if(pipe_grab.tile[j].x==x && pipe_grab.tile[j].y==y)seen=true;
    if(seen || pipe_grab.count==48)continue;
    pipe_grab.tile[pipe_grab.count++]=(PipeTile){x,y,id};
    if(x<left)left=x;if(x+16>right)right=x+16;if(y<top)top=y;if(y+16>bottom)bottom=y+16;
  }
  if(right-left>256 || bottom-top>256){pipe_grab.count=0;return false;}
  pipe_grab.x=left;pipe_grab.y=top;pipe_grab.w=right-left;pipe_grab.h=bottom-top;return true;
}
static void capture_pipe(void) {
  memset(grab.pixels,0,sizeof(grab.pixels));grab.w=pipe_grab.w;grab.h=pipe_grab.h;
  for(int i=0;i<pipe_grab.count;i++) {
    const uint16 *tiles=Lm_GetMap16RomAddr(pipe_grab.tile[i].id);
    for(int y=0;y<16;y++)for(int x=0;x<16;x++) {
      unsigned tile=tiles[(x/8)*2+y/8];
      int sx=tile&0x4000?7-(x&7):x&7,sy=tile&0x8000?7-(y&7):y&7;
      unsigned addr=(PPU_bgTileAdr(g_ppu,0)+(tile&0x3ff)*16+sy)&0x7fff;
      uint32 bits=((uint32)g_ppu->vram[addr]|((uint32)g_ppu->vram[(addr+8)&0x7fff]<<16))>>(7-sx);
      int p=(bits&1)|((bits>>7)&2)|((bits>>14)&4)|((bits>>21)&8);
      if(!p)continue;
      uint16 color=g_ppu->cgram[((tile>>10)&7)*16+p];
      int dx=pipe_grab.tile[i].x-pipe_grab.x+x,dy=pipe_grab.tile[i].y-pipe_grab.y+y;
      grab.pixels[dy*256+dx]=0xff000000|((color&31)*255/31<<16)|(((color>>5)&31)*255/31<<8)|((color>>10)*255/31);
    }
  }
  grab.captured=true;
}

void ww_targets_reset(void) {
  memset(state,0,sizeof(state));opponent=(FcOpponent){0};boss_ko=false;invalidated=0;
  memset(tile_hits,0,sizeof(tile_hits));
  grab.slot=-1;grab.captured=false;
  pipe_grab.count=0;
  pipe_painted_tick=UINT32_MAX;
}
void ww_target_spawned(unsigned k) {
  if(grab.slot==(int)k)grab.slot=-1;
  if(k<12) { state[k]=(TargetState){0};invalidated|=1u<<k; }
}
void ww_grapple_sync(FcFight *f) {
  int slot=fc_piledriver(f)?f->grabbed:-1;
  if(grab.slot==WW_PIPE_TARGET) {
    if(slot==WW_PIPE_TARGET){if(pipe_painted_tick!=f->tick)pipe_tiles(true);pipe_painted_tick=f->tick;return;}
    pipe_tiles(false);pipe_grab.count=0;grab.slot=-1;grab.captured=false;
  }
  if(slot==WW_PIPE_TARGET && pipe_grab.count && g_ppu) {
    grab.slot=WW_PIPE_TARGET;grab.tag=0;capture_pipe();pipe_tiles(true);pipe_painted_tick=f->tick;return;
  }
  if(grab.slot>=0 && (slot!=grab.slot || spr_spriteid[grab.slot]!=grab.tag)) {
    /* Required objects and interrupted grabs go back to their original
     * place. Ordinary defeated enemies have already been removed. */
    if(spr_current_status[grab.slot] && spr_spriteid[grab.slot]==grab.tag) {
      if(ww_final_controls(grab.slot))ww_final_grapple(false);
      SetSprXYPos((uint8)grab.slot,grab.x,grab.y);
      spr_xspeed[grab.slot]=spr_yspeed[grab.slot]=0;
      spr_decrementing_table154c[grab.slot]=30;
    }
    grab.slot=-1;grab.captured=false;
  }
  if(slot>=0 && slot<12 && grab.slot<0 && spr_current_status[slot]>=8) {
    grab.slot=slot;grab.tag=spr_spriteid[slot];grab.x=GetSprXPos(slot);grab.y=GetSprYPos(slot);
    grab.captured=false;grab.w=grab.h=0;
  }
  if(grab.slot>=0 && grab.captured) {
    if(!ww_final_controls(grab.slot))SetSprXYPos((uint8)grab.slot,player_xpos+8-grab.w/2,player_ypos-grab.h/2);
    spr_decrementing_table154c[grab.slot]=30;
  }
}
bool ww_grapple_sprite(uint8_t k) {
  if(grab.slot!=k)return false;
  if(ww_final_controls(k)) {
    if(!grab.captured) {
      memset(grab.pixels,0,sizeof(grab.pixels));grab.w=64;grab.h=96;
      ww_final_grapple_art((FcCanvas){grab.pixels,64,96,256});grab.captured=true;
    }
    ww_final_grapple(true);return true;
  }
  if(grab.captured)return true;
  uint8 *oam=(uint8 *)oam_buf;
  uint8 old[512],sizes[128];memcpy(old,oam_buf,sizeof(old));memcpy(sizes,sprites_oamtile_size_buffer,sizeof(sizes));
  uint8 locked=flag_sprites_locked;flag_sprites_locked=1;
  WW_OriginalHandleSprite(k);flag_sprites_locked=locked;
  int left=512,top=512,right=-512,bottom=-512;
  bool used[128]={0};
  for(int i=0;i<128;i++)if(memcmp(old+4*i,oam+4*i,4) && oam[4*i+1]!=0xf0) {
    int x=oam[4*i]-(sprites_oamtile_size_buffer[i]&1?256:0),y=oam[4*i+1];
    int size=sprites_oamtile_size_buffer[i]&2?16:8;
    if(y>224)y-=256;
    if(x<left)left=x;if(y<top)top=y;if(x+size>right)right=x+size;if(y+size>bottom)bottom=y+size;
    used[i]=true;
  }
  memset(grab.pixels,0,sizeof(grab.pixels));grab.w=right-left;grab.h=bottom-top;
  if(g_ppu && grab.w>0 && grab.w<=256 && grab.h>0 && grab.h<=256) {
    /* Capture all OAM tiles, including large multi-tile sprites, using their
     * original VRAM art and palettes. No enemy-specific substitute sprites. */
    for(int i=127;i>=0;i--)if(used[i]) {
      int x=oam[4*i]-(sprites_oamtile_size_buffer[i]&1?256:0),y=oam[4*i+1];
      if(y>224)y-=256;
      int size=sprites_oamtile_size_buffer[i]&2?16:8,tile=oam[4*i+2],flags=oam[4*i+3];
      int base=flags&1?PPU_objTileAdr2(g_ppu):PPU_objTileAdr1(g_ppu);
      for(int dy=0;dy<size;dy++)for(int dx=0;dx<size;dx++) {
        int sx=flags&64?size-1-dx:dx,sy=flags&128?size-1-dy:dy;
        int t=(((tile>>4)+(sy>>3))<<4)|(((tile&15)+(sx>>3))&15);
        int addr=(base+t*16+(sy&7))&0x7fff;
        uint32 bits=((uint32)g_ppu->vram[addr]|((uint32)g_ppu->vram[(addr+8)&0x7fff]<<16))>>(7-(sx&7));
        int pixel=(bits&1)|((bits>>7)&2)|((bits>>14)&4)|((bits>>21)&8);
        if(!pixel)continue;
        uint16 color=g_ppu->cgram[128+((flags>>1)&7)*16+pixel];
        grab.pixels[(y-top+dy)*256+x-left+dx]=0xff000000|((color&31)*255/31<<16)|(((color>>5)&31)*255/31<<8)|((color>>10)*255/31);
      }
    }
  } else grab.w=grab.h=0;
  memcpy(oam_buf,old,sizeof(old));memcpy(sprites_oamtile_size_buffer,sizes,sizeof(sizes));
  grab.captured=true;return true;
}
void ww_grapple_render(FcCanvas c,bool front) {
  const FcFight *f=ww_fight_state();
  if(grab.slot<0 || f->grabbed!=grab.slot || !grab.captured || !grab.w || !grab.h)return;
  float angle=f->grab_tick*0.3f,depth=sinf(angle);
  if((depth>=0)!=front)return;
  float squeeze=cosf(angle),radius=grab.w>48?22:13;
  int center=(int16)(player_xpos-mirror_current_layer1_xpos)+8+(int)(radius*sinf(angle));
  int bottom=(int16)(player_ypos-mirror_current_layer1_ypos)+(grab.h>64?56:11);
  int width=(int)(grab.w*fabsf(squeeze));if(width<3)width=3;
  if(grab.w>=64 && width<grab.w/2)width=grab.w/2;
  for(int y=0;y<grab.h;y++)for(int x=0;x<width;x++) {
    int sx=x*grab.w/width;if(squeeze<0)sx=grab.w-1-sx;
    uint32 color=grab.pixels[(grab.h-1-y)*256+sx];
    int px=center-width/2+x,py=bottom-grab.h+y;
    if(color && px>=0 && px<c.w && py>=0 && py<c.h)c.pixels[py*c.pitch+px]=color;
  }
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
    case 0x8c:case 0x8d:case 0x8e:case 0x8f:case 0x9c:case 0xa3:
    case 0xb1:case 0xb7:case 0xb8:case 0xb9:case 0xba:case 0xbb:
    case 0xc0:case 0xc1:case 0xc4:case 0xc6:case 0xc8:
      return true;
    default:return id>=0xc9; /* Unknown controllers stay functional too. */
  }
}
static int boss_max(int k) {
  if(spr_spriteid[k]==0xa0 && ww_final_controls(k) && ww_final_phase()>=WW_FINAL_FIGHT)return 72;
  return spr_spriteid[k]==0xa9?6:spr_spriteid[k]==0xa0?30:18;
}
static const char *boss_name(int k) {
  switch(spr_spriteid[k]) {
    case 0xa0:return ww_final_controls(k) && ww_final_phase()>=WW_FINAL_FALL?"AKUMA":"BOWSER";
    case 0xa9:return "REZNOR";case 0xc5:return "BIG BOO";
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
    if(ww_final_controls(k))return ww_final_phase()>=WW_FINAL_END?0:maximum;
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
  if(spr_spriteid[k]==0x9e || spr_spriteid[k]==0xa3) {
    /* Native code temporarily moves the sprite from the pivot to the ball,
     * draws/collides there, then restores the pivot. Use the same ROM sine
     * table and rounding so combat follows the swinging end of the chain. */
    int angle=(spr_table151c[k]<<8)|spr_table1602[k],offset[2];
    for(int axis=0;axis<2;axis++) {
      int phase=(angle+axis*128)&511,sine=kCircleCoordinates[phase&255];
      int distance=sine>=256?spr_table187b[k]:(sine*spr_table187b[k]+128)>>8;
      offset[axis]=(phase&256)?-distance:distance;
    }
    b.x+=offset[0];b.y+=offset[1];
  } else if(spr_spriteid[k]==0x29) {
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
    } else if(k<42) {
      int j=k-22;tag=cluster_spr_spriteid[j];alive=tag!=0;
      box=(FcBox){cluster_spr_xpos_lo[j]+256*cluster_spr_xpos_hi[j]-4,
        cluster_spr_ypos_lo[j]+256*cluster_spr_ypos_hi[j]-4,24,24};
    } else if(k<WW_PIPE_TARGET) {
      FcTarget shot={0};tag=ww_final_target(k-42,&shot);alive=tag!=0;box=shot.box;
    } else {
      alive=find_pipe(f);tag=alive?1+pipe_grab.x+pipe_grab.y*8192:0;
      box=(FcBox){pipe_grab.x,pipe_grab.y,pipe_grab.w,pipe_grab.h};
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
    bool mount=k<12 && (spr_current_status[k]==11 || (spr_spriteid[k]==0x35 && player_riding_yoshi_flag) ||
      (ww_final_controls(k) && (ww_final_phase()<WW_FINAL_FIGHT || !ww_final_vulnerable())));
    bool ungrabbable=fc_piledriver(f) && k!=WW_PIPE_TARGET && (k>=12 ||
      (spr_spriteid[k]==0xa0 && f->move!=FC_SUPER_MOVE) || spr_spriteid[k]==0xa9 || spr_spriteid[k]==0x29);
    targets[k]=(FcTarget){box,s->hp,!mount && !ungrabbable && !s->dead && !s->cooldown,false,0,0};
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
      ww_final_defeated();
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
    TargetState *s=&state[k];FcTarget *t=&targets[k];
    if(k<12 && ww_final_controls(k)) {
      int damage=ww_final_limit_damage(s->hp-t->hp,s->hp);
      t->hp=s->hp-damage;
      if(!damage)continue;
    }
    s->hp=t->hp;
    fc_explode(f,t->box.x+t->box.w/2,t->box.y+t->box.h/2,s->boss || f->move==FC_SUPER_MOVE);
    if(s->hp>0) { if(k<12 && ww_final_controls(k))s->cooldown=8;continue; }
    if(k==WW_PIPE_TARGET){s->hp=1;s->cooldown=30;continue;}
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
        spr_current_status[k]=0;
      } else if(k<22)ext_spr_spriteid[k-12]=0;
      else if(k<42)cluster_spr_spriteid[k-22]=0;
      else ww_final_hit_shot(k-42,(f->move==FC_JAB || f->move==FC_ROUNDHOUSE) &&
        (f->melee_mask&(UINT64_C(1)<<k)));
      s->alive=false;
    }
  }
  update_opponent(f);
}
int ww_hit_finalboss(FcFight *f,int damage) {
  if(!ww_final_vulnerable())return 0;
  for(int k=0;k<12;k++)if(ww_final_controls(k) && state[k].alive && !state[k].dead && !state[k].cooldown) {
    int hp=state[k].hp;FcTarget targets[WW_TARGET_COUNT]={0};
    targets[k]=(FcTarget){normal_box(k),hp>damage?hp-damage:0,true,true,0,0};
    ww_targets_apply(f,targets);
    return hp-state[k].hp;
  }
  return 0;
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
      /* SMW's first Map16 page is pass-through scenery (bushes, water,
       * background details). Only solid foreground tiles get an impact. */
      if(tile<0x100)continue;
      unsigned h=((unsigned)(x/16)*13u+(unsigned)(y/16)*7u)%64;
      if(tile_hits[h].until>f->tick && tile_hits[h].x==x && tile_hits[h].y==y)return true;
      tile_hits[h].x=x;tile_hits[h].y=y;tile_hits[h].until=f->tick+45;
      fc_explode(f,x+8,y+8,large);
      return true;
    }
  return false;
}
static bool strike_block(FcFight *f,int x,int y) {
  unsigned tile=map_tile(x,y);
  if(tile<0x100)return false;
  /* An existing bounce owns this block until it restores its Map16 tile.
   * Do not dispense its contents twice when another strike overlaps it. */
  for(int i=0;i<4;i++)if(bounce_spr_spriteid[i] &&
      ((bounce_spr_xpos_lo[i]|(bounce_spr_xpos_hi[i]<<8))&~15)==x &&
      ((bounce_spr_ypos_lo[i]|(bounce_spr_ypos_hi[i]<<8))&~15)==y)return true;
  uint16 old_x=blocks_xpos,old_y=blocks_ypos;
  uint8 old_layer=misc_current_layer_being_processed,old_tile=blocks_map16_to_generate;
  blocks_xpos=x;blocks_ypos=y;misc_current_layer_being_processed=0;
  if(tile==0x130 || tile==0x132) {
    SpawnBrickPieces(0);blocks_map16_to_generate=2;GenerateTile();
    io_sound_ch3=7;
  } else if((tile>=0x111 && tile<=0x12d) || tile==0x16a || tile==0x16b) {
    /* The normal upward-hit path preserves coins, powerups, switches,
     * vines and item memory. Side-bounce blocks use their native direction. */
    CheckIfBlockWasHit((uint8)tile,tile==0x12a || tile==0x12b?(f->facing>0?1:2):0);
  }
  blocks_xpos=old_x;blocks_ypos=old_y;
  misc_current_layer_being_processed=old_layer;blocks_map16_to_generate=old_tile;
  fc_explode(f,x+8,y+8,false);f->events|=FC_SND_HIT;
  return true;
}
static void normal_blocks(FcFight *f) {
  bool kick=f->move==FC_ROUNDHOUSE;
  int reach=kick?29:18;
  if(f->character==FC_DHALSIM)reach=kick?(f->air_attack?88:f->crouching_attack?40:82):
    f->air_attack?72:f->crouching_attack?88:96;
  if(f->character==FC_VEGA)reach+=12;
  int feet=(int)(f->body.box.y+f->body.box.h+f->sprite_y_offset);
  int top=feet-(f->crouching_attack?(kick?9:14):32);
  int edge=(int)(f->body.box.x+(f->facing>0?f->body.box.w:0));
  int first=(edge+(f->facing>0?0:-1))&~15;
  for(int distance=0;distance<reach;distance+=16) {
    int x=first+f->facing*distance;bool solid=false;
    for(int y=top&~15;y<feet-1;y+=16)solid=strike_block(f,x,y)||solid;
    if(solid)break; /* A punch cannot collect items through a wall. */
  }
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
  if((f->move==FC_JAB && f->move_tick==4) || (f->move==FC_ROUNDHOUSE && f->move_tick==7))
    normal_blocks(f);
  else if(f->move>FC_ROUNDHOUSE && f->move!=FC_PROJECTILE && !fc_piledriver(f) && f->move_tick==4) {
    FcBox b=f->body.box;
    FcBox fist={b.x+(f->facing>0?b.w+16:-16),b.y+b.h/2,1,1};
    blast_tiles(f,fist,f->move==FC_SUPER_MOVE);
  }
}
