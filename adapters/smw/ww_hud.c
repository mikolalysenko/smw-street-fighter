#include "ww_hud.h"
#include "ww_score.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include "assets/smw_assets.h"

static uint32_t items[4][16*16],coin[8*8],star[8*8];
static bool ready;

void ww_hud_reset(void) { ready=false; }
static uint32_t rgb(unsigned color) {
  unsigned r=color&31,g=(color>>5)&31,b=(color>>10)&31;
  return 0xff000000|(((r<<3)|(r>>2))<<16)|(((g<<3)|(g>>2))<<8)|((b<<3)|(b>>2));
}
static void load_icons(void) {
  if(ready || !g_asset_ptrs[0])return;
  /* Decode the original common graphics into private buffers. Reading live
   * VRAM would turn these into boss tiles in Mode 7 and during transitions. */
  uint8 gfx[0x4000];
  DecompressTo(kGraphicsPtrs(0).ptr,gfx);
  static const int tiles[]={0x24,0x26,0x48,0x0e},palettes[]={12,13,8,10};
  for(int item=0;item<4;item++)for(int y=0;y<16;y++)for(int x=0;x<16;x++) {
    int tile=tiles[item]+x/8+(y/8)*16,row=y%8,bit=7-x%8;
    const uint8 *p=gfx+tile*24;
    unsigned index=((p[row*2]>>bit)&1)|(((p[row*2+1]>>bit)&1)<<1)|(((p[16+row]>>bit)&1)<<2);
    unsigned color=index<2?0x7fff:kGlobalPalettes_Objects[(palettes[item]-4)*6+index-2];
    items[item][y*16+x]=index?rgb(color):0;
  }
  DecompressTo(kGraphicsPtrs(0x28).ptr,gfx);
  /* SMW's own status-bar coin and bonus-star glyphs and palettes. */
  for(int icon=0;icon<2;icon++)for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
    const uint8 *p=gfx+(icon?0x64:0x2e)*16;
    unsigned index=((p[y*2]>>(7-x))&1)|(((p[y*2+1]>>(7-x))&1)<<1);
    (icon?star:coin)[y*8+x]=index?rgb(kGlobalPalettes_Layer3[(icon?0:12)+index]):0;
  }
  ready=true;
}
FcStageHud ww_stage_hud(void) {
  load_icons();
  int lives=(int8)player_current_life_count+1;
  FcStageHud h={
    .score=ww_score(),.high_score=ww_high_score(),
    .time=counter_timer_hundreds*100+counter_timer_tens*10+counter_timer_ones,
    .lives=lives<0?0:lives>99?99:lives,
    .coins=player_current_coin_count,
    .bonus_stars=player_current_character?player_luigi_bonus_stars:player_mario_bonus_stars,
    .dragon_coins=counter_yoshi_coins_to_display>5?5:counter_yoshi_coins_to_display,
    .timed=misc_nmito_use_flag!=0xc1,.clearing=timer_end_level!=0,
  };
  if(ready) {
    h.coin_icon=(FcCanvas){coin,8,8,8};h.star_icon=(FcCanvas){star,8,8,8};
    if(player_current_item_box>=1 && player_current_item_box<=4)
      h.reserve_icon=(FcCanvas){items[player_current_item_box-1],16,16,16};
  }
  return h;
}
