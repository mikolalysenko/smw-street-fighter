/* Exercise the external asset boundary and sprite clipping, without game data. */
#include "assets.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *path="build/test-sprites.dat";
static void word(FILE *f,unsigned n) { fputc(n&255,f);fputc((n>>8)&255,f); }
static void fixture(int width,int truncate,int trailing) {
  FILE *f=fopen(path,"wb");assert(f);fwrite("WWA1",1,4,f);word(f,1);
  const unsigned header[]={0,0,6,(unsigned)width,2,1,2};
  for(unsigned i=0;i<7;i++)word(f,header[i]);
  const unsigned pixels[]={0xffff0000,0,0xff00ff00,0xff0000ff};
  for(int i=0;i<(truncate?3:4);i++){word(f,pixels[i]);word(f,pixels[i]>>16);}
  if(trailing)fputc(1,f);
  fclose(f);
}
int main(void) {
  uint32_t p[8];for(int i=0;i<8;i++)p[i]=0x12345678;
  fixture(2,0,0);assert(fc_assets_load(path));assert(fc_assets_ready());
  FcCanvas c={p+2,2,2,2};
  fc_sprite(c,0,0,0,1,2,1,1,1,0);
  assert(p[2]==0xffff0000 && p[3]==0x12345678 && p[4]==0xff00ff00 && p[5]==0xff0000ff);
  fc_sprite(c,0,0,0,1,2,-1,1,1,0);
  assert(p[3]==0xffff0000 && p[4]==0xff0000ff && p[5]==0xff00ff00);
  for(int y=-4;y<5;y++)for(int x=-4;x<5;x++)
    fc_sprite(c,0,0,0,x,y,-1,1,2,0xabcdef);
  assert(p[0]==0x12345678 && p[1]==0x12345678 && p[6]==0x12345678 && p[7]==0x12345678);
  fixture(2,1,0);assert(!fc_assets_load(path));assert(!fc_assets_ready());
  fixture(513,0,0);assert(!fc_assets_load(path));
  fixture(2,0,1);assert(!fc_assets_load(path));
  fixture(2,0,0);assert(fc_assets_load(path));fc_assets_free();assert(!fc_assets_ready());
  remove(path);puts("Sprite import: transparency, facing, clipping, malformed/truncated data and cleanup passed.");
  return 0;
}
