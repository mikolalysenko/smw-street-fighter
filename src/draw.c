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
  else if(f->guarding)pose=FC_POSE_GUARD;
  else if(f->move!=FC_IDLE) {
    static const int poses[]={FC_POSE_IDLE,FC_POSE_PUNCH,FC_POSE_KICK,FC_POSE_FIRE,
      FC_POSE_RISE,FC_POSE_SPIN,FC_POSE_RUSH,FC_POSE_FLURRY,FC_POSE_FIRE};
    pose=poses[f->move];tick=(unsigned)f->move_tick;
    if(f->move==FC_SUPER_MOVE)pose=f->character==FC_HONDA?FC_POSE_RUSH:
      f->character==FC_CHUNLI?FC_POSE_FLURRY:FC_POSE_FIRE;
  } else if(!f->body.grounded)pose=FC_POSE_JUMP;
  else if(f->previous&FC_DOWN)pose=FC_POSE_DUCK;
  else if(f->previous&(FC_LEFT|FC_RIGHT))pose=FC_POSE_WALK;
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
}
void fc_hud(FcCanvas c,const FcFight *f) {
  char text[64];const FcCharacter *r=&fc_roster[f->character];
  int width=c.w>=360?132:104;
  fc_rect(c,8,8,width+16,26,0x101b32);frame(c,8,8,width+16,26,0x627287);
  fc_text(c,14,12,r->name,r->color,1);
  snprintf(text,sizeof(text),"%d",f->hp);fc_text(c,width-6,12,text,0xfaf0da,1);
  fc_rect(c,14,23,width,5,0x4f2d3e);
  fc_rect(c,14,23,width*f->hp/r->vitality,5,f->hp>30?0xecb96c:0xf47769);
  fc_rect(c,8,c.h-30,145,22,0x101b32);frame(c,8,c.h-30,145,22,0x576788);
  snprintf(text,sizeof(text),"SUPER %d",f->meter/100);fc_text(c,14,c.h-25,text,0x92e6ff,1);
  for(int i=0;i<3;i++) {
    int fill=f->meter-i*100;if(fill<0)fill=0;if(fill>100)fill=100;
    fc_rect(c,14+i*44,c.h-15,41,3,0x304662);
    fc_rect(c,14+i*44,c.h-15,41*fill/100,3,fill==100?0xffd885:0x75dfff);
  }
  if(f->meter>=100)fc_text(c,75,c.h-25,"RB READY",0xffd885,1);
  if(f->combo>1) {
    snprintf(text,sizeof(text),"%d HIT",f->combo);fc_text(c,c.w-92,43,text,0xffdc8b,2);
  }
  if(f->move>=FC_PROJECTILE && !f->flash)fc_center(c,39,fc_move_name(f),r->color,1);
  if(f->hp<=0) {
    fc_rect(c,0,c.h/2-24,c.w,53,0x182238);
    fc_center(c,c.h/2-18,"K.O.",0xffa47c,4);
    fc_center(c,c.h/2+18,"YOU LOSE",0xf0e8d7,1);
  }
}
void fc_selection(FcCanvas c,int choice,unsigned tick,bool compact) {
  (void)compact;
  fc_rect(c,0,0,c.w,c.h,0x071d67);
  fc_center(c,7,"PLAYER SELECT",0xfff0a0,1);
  fc_sprite(c,0,FC_POSE_MAP,0,(c.w-192)/2,22,1,1,1,0);
  int start=(c.w-232)/2;
  for(int i=0;i<FC_ROSTER_COUNT;i++) {
    int x=start+i*58;bool selected=i==choice;
    fc_rect(c,x,139,54,61,selected?0x3567aa:0x111b4b);
    frame(c,x,139,54,61,selected?0xffcf28:0x8193ba);
    fc_sprite(c,i,FC_POSE_PORTRAIT,0,x+27,180,1,1,4,0);
    fc_text(c,x+3,185,fc_roster[i].name,selected?0xffe050:0xffffff,1);
    if(selected && (tick/20)%2==0)fc_text(c,x+2,141,"1P",0xffdb36,1);
  }
  fc_center(c,c.h-17,"DPAD SELECT  A / START CONFIRM",0xffffff,1);
  fc_center(c,c.h-8,"SMW X STREET FIGHTER II",0x7fb3f3,1);
}
void fc_pause(FcCanvas c,int choice) {
  fc_dim(c,90);fc_rect(c,c.w/2-87,c.h/2-49,174,108,0x142037);
  frame(c,c.w/2-87,c.h/2-49,174,108,0x7a91a1);
  fc_center(c,c.h/2-36,"PAUSED",0xf8d898,2);
  const char *items[]={"RESUME","CHARACTER SELECT","QUIT GAME"};
  for(int i=0;i<3;i++) {
    if(choice==i)fc_rect(c,c.w/2-77,c.h/2-10+i*20,154,15,0x304956);
    fc_center(c,c.h/2-6+i*20,items[i],choice==i?0xffd28a:0xb0c6cd,1);
  }
}
