#include "ww_title.h"
#include "assets.h"
#include "smw_rtl.h"
#include "variables.h"
#include "snes/ppu.h"
#include <string.h>

enum { LOGO_W=208,LOGO_H=80,MAP_COLS=26,MAP_ROWS=10 };
static uint32_t letters[LOGO_W*LOGO_H],overlay[256*224];
static uint16_t original_map[MAP_COLS*MAP_ROWS];
static unsigned ticks,brightness,map_base;
static bool active,saved_map;

void ww_title_reset(void) { ticks=brightness=0;active=saved_map=false; }
void ww_title_tick(void) { ++ticks; }
void ww_title_begin(Ppu *ppu) {
  /* The title demo fades back through modes 2/3 before restarting. */
  active=fc_assets_ready() && ((misc_game_mode>=5 && misc_game_mode<=7) ||
    (active && (misc_game_mode==2 || misc_game_mode==3)));
  saved_map=false;
  brightness=PPU_forcedBlank(ppu)?0:PPU_brightness(ppu);
  if(!active || !brightness)return;
  map_base=PPU_bgTilemapAdr(ppu,2);
  unsigned tiles=PPU_bgTileAdr(ppu,2);
  /* Copy the original 2bpp title lettering from BG3, with transparency and
   * its own palettes. No scenery or moving demo sprites enter this image. */
  for(int y=0;y<LOGO_H;y++)for(int x=0;x<LOGO_W;x++) {
    uint16_t tile=ppu->vram[(map_base+(4+y/8)*32+3+x/8)&0x7fff];
    int row=(tile&0x8000)?7-y%8:y%8,col=(tile&0x4000)?x%8:7-x%8;
    uint16_t bits=ppu->vram[(tiles+(tile&0x3ff)*8+row)&0x7fff];
    unsigned index=((bits>>col)&1)|(((bits>>(col+8))&1)<<1);
    uint32_t color=0;
    if(index) {
      unsigned rgb=ppu->cgram[((tile>>10)&7)*4+index];
      unsigned r=rgb&31,g=(rgb>>5)&31,b=(rgb>>10)&31;
      color=0xff000000|(((r<<3)|(r>>2))<<16)|(((g<<3)|(g>>2))<<8)|((b<<3)|(b>>2));
    }
    letters[y*LOGO_W+x]=color;
  }
  /* Suppress only the old lettering during this render. Restore every VRAM
   * word immediately afterward, so the save menu/game state stays native. */
  for(int y=0;y<MAP_ROWS;y++)for(int x=0;x<MAP_COLS;x++) {
    unsigned address=(map_base+(y+4)*32+x+3)&0x7fff;
    original_map[y*MAP_COLS+x]=ppu->vram[address];ppu->vram[address]=0x38fc;
  }
  saved_map=true;
}
void ww_title_end(Ppu *ppu) {
  if(!saved_map)return;
  for(int y=0;y<MAP_ROWS;y++)for(int x=0;x<MAP_COLS;x++)
    ppu->vram[(map_base+(y+4)*32+x+3)&0x7fff]=original_map[y*MAP_COLS+x];
  saved_map=false;
}
bool ww_title_render(FcCanvas canvas,unsigned high_score) {
  if(!active)return false;
  if(!brightness)return true;
  memset(overlay,0,sizeof(overlay));FcCanvas title={overlay,256,224,256};
  for(int y=0;y<LOGO_H*4/5;y++)for(int x=0;x<LOGO_W*4/5;x++)
    overlay[(34+y)*256+20+x]=letters[(y*5/4)*LOGO_W+x*5/4];
  fc_title(title,ticks,high_score);
  for(int y=0;y<224 && y<canvas.h;y++)for(int x=0;x<256 && x<canvas.w;x++) {
    uint32_t color=overlay[y*256+x];
    if(color>>24)canvas.pixels[y*canvas.pitch+x]=0xff000000|
      ((((color>>16)&255)*brightness/15)<<16)|
      ((((color>>8)&255)*brightness/15)<<8)|((color&255)*brightness/15);
  }
  return true;
}
