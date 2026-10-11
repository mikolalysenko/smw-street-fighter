#include "draw.h"
#include "assets.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Small original 5x7 bitmap alphabet. Each integer is one row. */
static const uint8_t letters[36][7]={
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{31,4,4,4,4,4,31},
 {7,2,2,2,2,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14}
};
void fc_rect(FcCanvas c,int x,int y,int w,int h,uint32_t color) {
  int x2=x+w,y2=y+h;if(x<0)x=0;if(y<0)y=0;
  if(x2>c.w)x2=c.w;if(y2>c.h)y2=c.h;
  for(int j=y;j<y2;j++)for(int i=x;i<x2;i++)c.pixels[j*c.pitch+i]=0xff000000|color;
}
static void frame(FcCanvas c,int x,int y,int w,int h,uint32_t color) {
  fc_rect(c,x,y,w,1,color);fc_rect(c,x,y+h-1,w,1,color);
  fc_rect(c,x,y,1,h,color);fc_rect(c,x+w-1,y,1,h,color);
}
void fc_text(FcCanvas c,int x,int y,const char *s,uint32_t color,int scale) {
  for(;*s;s++,x+=6*scale) {
    int ch=toupper((unsigned char)*s),idx=ch>='A'&&ch<='Z'?ch-'A':ch>='0'&&ch<='9'?ch-'0'+26:-1;
    if(idx>=0) {
      for(int row=0;row<7;row++)for(int col=0;col<5;col++)
        if(letters[idx][row]&(1<<(4-col)))fc_rect(c,x+col*scale,y+row*scale,scale,scale,color);
    } else if(ch=='-' || ch=='+' || ch=='=') {
      fc_rect(c,x,y+3*scale,5*scale,scale,color);
      if(ch=='+')fc_rect(c,x+2*scale,y+scale,scale,5*scale,color);
      if(ch=='=')fc_rect(c,x,y+5*scale,5*scale,scale,color);
    } else if(ch=='.' || ch==':') {
      fc_rect(c,x+2*scale,y+6*scale,scale,scale,color);
      if(ch==':')fc_rect(c,x+2*scale,y+2*scale,scale,scale,color);
    } else if(ch=='/' || ch=='>') {
      for(int j=0;j<5;j++)fc_rect(c,x+(ch=='/'?4-j:(j<3?j:4-j))*scale,y+(j+1)*scale,scale,scale,color);
    } else if(ch=='!') {fc_rect(c,x+2*scale,y,scale,4*scale,color);fc_rect(c,x+2*scale,y+6*scale,scale,scale,color);}
  }
}
void fc_center(FcCanvas c,int y,const char *s,uint32_t color,int scale) {
  fc_text(c,(c.w-(int)strlen(s)*6*scale+scale)/2,y,s,color,scale);
}
void fc_dim(FcCanvas c,int amount) {
  for(int y=0;y<c.h;y++)for(int x=0;x<c.w;x++) {
    uint32_t p=c.pixels[y*c.pitch+x];
    c.pixels[y*c.pitch+x]=0xff000000|((((p>>16)&255)*(unsigned)amount/256)<<16)|
      ((((p>>8)&255)*(unsigned)amount/256)<<8)|((p&255)*(unsigned)amount/256);
  }
}
void fc_fighter(FcCanvas c,const FcFight *f,int x,int feet,int scale,uint32_t tint) {
  int pose=FC_POSE_IDLE;unsigned tick=f->tick;
  if(f->hp<=0){pose=FC_POSE_KO;tick=(unsigned)f->ko_timer;}
  else if(f->stun){pose=FC_POSE_HURT;tick=(unsigned)(22-f->stun);}
  else if(f->victory){pose=FC_POSE_WIN;tick=f->move_tick;}
  else if(f->guarding)pose=FC_POSE_GUARD;
  else if(f->move!=FC_IDLE) {
    static const int poses[]={FC_POSE_IDLE,FC_POSE_PUNCH,FC_POSE_KICK,FC_POSE_FIRE,
      FC_POSE_RISE,FC_POSE_SPIN,FC_POSE_RUSH,FC_POSE_FLURRY,FC_POSE_FIRE};
    pose=poses[f->move];tick=(unsigned)f->move_tick;
    if(!f->body.grounded && f->move==FC_JAB)pose=FC_POSE_AIR_PUNCH;
    if(!f->body.grounded && f->move==FC_ROUNDHOUSE)pose=FC_POSE_AIR_KICK;
    if(f->move==FC_SUPER_MOVE)pose=f->character==FC_HONDA?FC_POSE_RUSH:
      f->character==FC_CHUNLI?FC_POSE_FLURRY:FC_POSE_FIRE;
  } else if(!f->body.grounded){pose=FC_POSE_JUMP;tick=f->air_tick;}
  else if(f->previous&FC_DOWN)pose=FC_POSE_DUCK;
  else if(f->moving){pose=FC_POSE_WALK;tick=(unsigned)(f->walk_distance*0.65f);}
  if(f->invuln && !f->stun && f->move==FC_IDLE && f->hp>0 && ((f->tick/4)%2))return;
  fc_sprite(c,f->character,pose,tick,x,feet,f->facing,scale,2,tint);
}
void fc_effects(FcCanvas c,const FcFight *f,float camx,float camy) {
  if(f->flash) {
    fc_dim(c,110);
    for(int i=0;i<12;i++) {
      int x=(i*43+(int)f->tick*4)%c.w;
      fc_rect(c,x,38+i*9,18+i*2,1,0x3e598f);
    }
    fc_center(c,c.h/2-26,"SUPER ART",0x7cecff,2);
    fc_center(c,c.h/2-7,fc_roster[f->character].super_name,0xffffff,1);
  }
  for(int i=0;i<FC_MAX_SHOTS;i++)if(f->shots[i].ttl) {
    const FcShot *p=&f->shots[i];int x=(int)(p->box.x-camx),y=(int)(p->box.y-camy);
    int w=(int)p->box.w,h=(int)p->box.h;
    fc_sprite(c,f->character,FC_POSE_SHOT,f->tick,x+w/2,y+h/2,p->vx>0?1:-1,p->super?2:1,2,0);
  }
  for(int i=0;i<FC_MAX_SPARKS;i++)if(f->sparks[i].ttl) {
    const FcSpark *s=&f->sparks[i];
    fc_rect(c,(int)(s->x-camx),(int)(s->y-camy),s->ttl>12?3:2,2,s->color);
  }
  for(int i=0;i<FC_MAX_BLASTS;i++)if(f->blasts[i].ttl) {
    const FcBlast *b=&f->blasts[i];int age=24-b->ttl;
    int r=(b->large?10:5)+age/2,x=(int)(b->x-camx),y=(int)(b->y-camy);
    uint32_t color=age<6?0xffffdd:age<13?0xffd22b:0xff6720;
    if(age<7) {
      fc_rect(c,x-r,y-r/2,r*2,r,color);fc_rect(c,x-r/2,y-r,r,r*2,color);
      fc_rect(c,x-r/3,y-r/3,r*2/3,r*2/3,0xffffff);
    } else {
      fc_rect(c,x-r/2,y-r,r,2,color);fc_rect(c,x-r/2,y+r,r,2,color);
      fc_rect(c,x-r,y-r/2,2,r,color);fc_rect(c,x+r,y-r/2,2,r,color);
      for(int dx=-1;dx<=1;dx+=2)for(int dy=-1;dy<=1;dy+=2)
        fc_rect(c,x+dx*(r*3/4),y+dy*(r*3/4),3,3,color);
    }
  }
}
/* Original SF2 letter art, scaled to the SNES 256-pixel playfield. */
static void arcade_text(FcCanvas c,int x,int y,const char *s,bool big) {
  for(;*s;s++) {
    int ch=toupper((unsigned char)*s),i=ch>='A'&&ch<='Z'?ch-'A':ch>='0'&&ch<='9'?ch-'0'+26:-1;
    if(i>=0 && fc_assets_ready())
      fc_sprite(c,0,big?FC_POSE_BIG_FONT:FC_POSE_FONT,(unsigned)i,x,y,1,big?3:2,big?4:3,0);
    else {char glyph[2]={(char)ch,0};fc_text(c,x,y+1,glyph,0xffdd65,1);}
    x+=big?(ch=='W'?19:13):8;
  }
}
static void arcade_center(FcCanvas c,int y,const char *s,bool big) {
  int width=0;for(const char *p=s;*p;p++)width+=big?(*p=='W'?19:13):8;
  arcade_text(c,(c.w-width)/2,y,s,big);
}
static void life_bar(FcCanvas c,int x,int y,int w,int hp,int maximum,bool reverse) {
  if(maximum<1)maximum=1;if(hp<0)hp=0;if(hp>maximum)hp=maximum;
  fc_rect(c,x-2,y-2,w+4,13,0x141034);frame(c,x-1,y-1,w+2,11,0xffffee);
  fc_rect(c,x,y,w,9,0xdf211b);
  int filled=w*hp/maximum,left=reverse?x+w-filled:x;
  fc_rect(c,left,y,filled,9,0xffdb00);fc_rect(c,left,y,filled,2,0xffff85);
  fc_rect(c,left,y+7,filled,2,0xdf9100);
}
void fc_hud(FcCanvas c,const FcFight *f) { fc_battle_hud(c,f,NULL); }
void fc_battle_hud(FcCanvas c,const FcFight *f,const FcOpponent *o) {
  char text[64];const FcCharacter *r=&fc_roster[f->character];
  int w=(c.w-48)/2;bool boss=o && o->active;
  fc_rect(c,0,0,c.w,39,0x101035);
  arcade_text(c,9,3,r->name,false);
  life_bar(c,9,17,w,f->hp,r->vitality,false);
  if(boss) {
    arcade_text(c,c.w-9-(int)strlen(o->name)*8,3,o->name,false);
    life_bar(c,c.w-9-w,17,w,o->hp,o->max_hp,true);
  } else arcade_text(c,c.w-9-8*8,17,"1P WORLD",false);
  fc_sprite(c,0,FC_POSE_KO_MARK,0,c.w/2-14,13,1,3,4,0);
  /* Alpha-style metal bezel, three bright stocks and lettering laid over
   * the fill. The full meter flashes gold without covering the playfield. */
  int gy=c.h-19;
  fc_rect(c,6,gy,176,16,0x07091c);frame(c,7,gy+1,173,14,0x6169a5);
  fc_rect(c,10,gy+3,143,10,0x273063);
  for(int i=0;i<3;i++) {
    int fill=f->meter-i*100;if(fill<0)fill=0;if(fill>100)fill=100;
    int left=11+i*47,filled=45*fill/100;
    uint32_t color=f->meter==300 && (f->tick/12)%2?0xffd438:0x287ded;
    fc_rect(c,left,gy+4,filled,8,color);fc_rect(c,left,gy+4,filled,2,0x8fefff);
    fc_rect(c,left,gy+10,filled,2,0x173ea7);
    fc_rect(c,left+45,gy+3,2,10,0x090c30);
  }
  fc_text(c,17,gy+6,"SUPER",0x08082d,1);fc_text(c,16,gy+5,"SUPER",0xffffff,1);
  fc_text(c,157,gy+5,"LV",0xffee87,1);
  snprintf(text,sizeof(text),"%d",f->meter/100);arcade_text(c,171,gy+3,text,false);
  if(f->meter==300)fc_text(c,188,gy+5,"MAX",0xffe366,1);
  if(f->combo>1) {
    snprintf(text,sizeof(text),"%d HIT",f->combo);arcade_text(c,9,40,text,false);
  }
  if(f->move>=FC_PROJECTILE && !f->flash)fc_center(c,52,fc_move_name(f),0xfff48c,1);
  if(f->hp<=0 || (boss && o->ko_timer>0)) {
    fc_dim(c,170);
    fc_sprite(c,0,FC_POSE_KO_MARK,0,c.w/2-65,c.h/2-32,1,3,1,0);
    fc_text(c,c.w/2+45,c.h/2-25,"!!",0xffe450,3);
    arcade_center(c,c.h/2+30,f->hp<=0?"YOU LOSE":"YOU WIN",true);
  }
}
static void menu_background(FcCanvas c) {
  fc_rect(c,0,0,c.w,c.h,0x080c51);
  for(int y=0;y<c.h;y+=4)fc_rect(c,0,y,c.w,2,0x101563);
  frame(c,4,4,c.w-8,c.h-8,0x3854a1);
}
void fc_selection(FcCanvas c,int choice,unsigned tick,bool compact) {
  (void)compact;menu_background(c);
  arcade_center(c,8,"PLAYER SELECT",true);
  fc_sprite(c,0,FC_POSE_MAP,0,(c.w-192)/2,29,1,1,1,0);
  int start=(c.w-232)/2;
  for(int i=0;i<FC_ROSTER_COUNT;i++) {
    int x=start+i*58;bool selected=i==choice;
    fc_rect(c,x,145,54,53,selected?0x2979c8:0x131d59);
    frame(c,x-1,144,56,55,selected?0xffff8a:0x617ccb);
    fc_sprite(c,i,FC_POSE_PORTRAIT,0,x+27,184,1,1,4,0);
    const char *name=i==FC_HONDA?"HONDA":i==FC_CHUNLI?"CHUNLI":fc_roster[i].name;
    arcade_text(c,x+(54-(int)strlen(name)*8)/2,187,name,false);
    if(selected && (tick/20)%2==0)fc_text(c,x+2,146,"1P",0xffee22,1);
  }
  fc_center(c,c.h-17,"DPAD SELECT  A / START CONFIRM",0xffffd5,1);
}
void fc_pause(FcCanvas c,int choice) {
  fc_dim(c,70);fc_rect(c,c.w/2-99,c.h/2-56,198,114,0x0c155c);
  frame(c,c.w/2-99,c.h/2-56,198,114,0xffd52b);
  arcade_center(c,c.h/2-43,"PAUSE",true);
  const char *items[]={"RESUME","PLAYER SELECT","QUIT GAME"};
  for(int i=0;i<3;i++) {
    if(choice==i)fc_rect(c,c.w/2-90,c.h/2-12+i*21,180,16,0x274ba1);
    arcade_center(c,c.h/2-9+i*21,items[i],false);
    if(choice==i)fc_text(c,c.w/2-87,c.h/2-8+i*21,">",0xffff55,1);
  }
}
void fc_continue(FcCanvas c,int choice,int ticks) {
  if(ticks<30)return;
  fc_rect(c,20,155,c.w-40,61,0x0c155c);frame(c,20,155,c.w-40,61,0xffd52b);
  arcade_center(c,160,"CONTINUE?",true);
  const char *items[]={"RETRY LEVEL","RETURN TO MAP"};
  for(int i=0;i<2;i++) {
    if(choice==i)fc_rect(c,30,181+i*16,c.w-60,14,0x274ba1);
    arcade_center(c,183+i*16,items[i],false);
    if(choice==i)fc_text(c,33,184+i*16,">",0xffff55,1);
  }
}
void fc_level_clear(FcCanvas c,int character) {
  /* Cover the original MARIO/COURSE CLEAR heading, preserving the native
   * time bonus, save event and walk-off sequence underneath. */
  fc_rect(c,35,48,c.w-70,54,0x0c155c);frame(c,35,48,c.w-70,54,0xe9d162);
  fc_sprite(c,character,FC_POSE_WIN,40,54,99,1,1,3,0);
  arcade_center(c,54,fc_roster[character].name,true);
  arcade_center(c,78,"COURSE CLEAR",false);
}
