#include "assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { MAX_FRAMES=4096 };
typedef struct { int character,pose,ticks,w,h,ax,ay; uint32_t *pixels; } Frame;
static Frame frames[MAX_FRAMES];
static unsigned count;
static int read16(FILE *f) {
  int a=fgetc(f),b=fgetc(f);return a<0||b<0?-1:a|(b<<8);
}
void fc_assets_free(void) {
  for(unsigned i=0;i<count;i++)free(frames[i].pixels);
  memset(frames,0,sizeof(frames));count=0;
}
bool fc_assets_ready(void) { return count>0; }
bool fc_assets_load(const char *path) {
  fc_assets_free();FILE *f=fopen(path,"rb");if(!f)return false;
  char magic[4];int n;
  if(fread(magic,1,4,f)!=4 || memcmp(magic,"WWA1",4) || (n=read16(f))<1 || n>MAX_FRAMES)goto fail;
  for(int i=0;i<n;i++) {
    Frame *p=&frames[count++];p->character=read16(f);p->pose=read16(f);p->ticks=read16(f);
    p->w=read16(f);p->h=read16(f);p->ax=read16(f);p->ay=read16(f);
    if(p->character<0 || p->character>=FC_ROSTER_COUNT || p->pose<0 || p->pose>=FC_POSE_COUNT ||
       p->ticks<1 || p->ticks>600 || p->w<1 || p->w>512 || p->h<1 || p->h>512 ||
       p->ax<0 || p->ax>512 || p->ay<0 || p->ay>512)goto fail;
    p->pixels=malloc((size_t)p->w*p->h*4);if(!p->pixels)goto fail;
    for(int j=0;j<p->w*p->h;j++) {
      int lo=read16(f),hi=read16(f);if(lo<0||hi<0)goto fail;
      p->pixels[j]=(uint32_t)lo|((uint32_t)hi<<16);
    }
  }
  if(fgetc(f)!=EOF)goto fail;
  fclose(f);return true;
fail:
  fclose(f);fc_assets_free();return false;
}
void fc_sprite(FcCanvas c,int character,int pose,unsigned tick,int x,int feet,
               int facing,int num,int den,uint32_t tint) {
  if(num<1||den<1)return;
  unsigned duration=0;
  for(unsigned i=0;i<count;i++)if(frames[i].character==character && frames[i].pose==pose)duration+=(unsigned)frames[i].ticks;
  if(!duration)return;
  bool once=pose==FC_POSE_KO || pose==FC_POSE_JUMP || pose==FC_POSE_WIN ||
    pose==FC_POSE_PUNCH || pose==FC_POSE_KICK || pose==FC_POSE_AIR_PUNCH ||
    pose==FC_POSE_AIR_KICK || pose==FC_POSE_CROUCH_PUNCH || pose==FC_POSE_CROUCH_KICK ||
    pose==FC_POSE_RISE || pose==FC_POSE_FIRE;
  if(once && tick>=duration)tick=duration-1;else tick%=duration;
  const Frame *p=NULL;
  for(unsigned i=0;i<count;i++)if(frames[i].character==character && frames[i].pose==pose) {
    if(tick<(unsigned)frames[i].ticks){p=&frames[i];break;}tick-=(unsigned)frames[i].ticks;
  }
  if(!p)return;
  int w=(p->w*num+den-1)/den,h=(p->h*num+den-1)/den;
  int left=x-(facing>0?p->ax:p->w-p->ax)*num/den,top=feet-p->ay*num/den;
  for(int dy=0;dy<h;dy++)for(int dx=0;dx<w;dx++) {
    int sx=dx*den/num,sy=dy*den/num;if(facing<0)sx=p->w-1-sx;
    int px=left+dx,py=top+dy;
    if(px<0||py<0||px>=c.w||py>=c.h)continue;
    uint32_t color=p->pixels[sy*p->w+sx];
    if(color>>24) c.pixels[py*c.pitch+px]=tint?0xff000000|tint:color;
  }
}
