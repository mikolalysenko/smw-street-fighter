/* Browser host for the same patched SMW engine used by the native game.
 * Simulation, original PPU rendering and SPC music run in WASM. JavaScript
 * owns presentation, input, audio scheduling and browser save persistence. */
#include <SDL.h>
#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "assets/smw_assets.h"
#include "smw_rtl.h"
#include "common_cpu_infra.h"
#include "config.h"
#include "util.h"
#include "smw_spc_player.h"
#include "snes/snes.h"
#include "snes/ppu.h"
#include "variables.h"
#include "ww_smw.h"
#include "ww_targets.h"
#include "ww_score.h"
#include "ww_finalboss.h"
#include "audio.h"

bool g_debug_flag,g_want_dump_memmap_flags;
bool g_new_ppu=true,g_other_image=true;
struct SpcPlayer *g_spc_player;
int g_got_mismatch_count;
Config g_config;
const uint8 *g_asset_ptrs[kNumberOfAssets];
uint32 g_asset_sizes[kNumberOfAssets];
static uint8 pixels[256*240*4],other_pixels[256*240*4],screen[256*224*4];
static int16 music[801*2];
static float audio[801*2];
static unsigned ticks;
static bool running;

void NORETURN Die(const char *error) { fprintf(stderr,"%s\n",error);abort(); }
void Warning(const char *error) { fprintf(stderr,"%s\n",error); }
void RtlApuLock(void) {}
void RtlApuUnlock(void) {}
MemBlk FindInAssetArray(int asset,int idx) {
  return FindIndexInMemblk((MemBlk){g_asset_ptrs[asset],g_asset_sizes[asset]},idx);
}
const uint8 *FindPtrInAsset(int asset,uint32 addr) {
  return FindAddrInMemblk((MemBlk){g_asset_ptrs[asset],g_asset_sizes[asset]},addr);
}
static uint32 read32(const uint8 *p) {
  return (uint32)p[0]|(uint32)p[1]<<8|(uint32)p[2]<<16|(uint32)p[3]<<24;
}
static void load_assets(void) {
  size_t length;uint8 *data=ReadWholeFile("smw_assets.dat",&length);
  static const uint8 signature[]={kAssets_Sig};
  if(!data || length<88+kNumberOfAssets*4 || memcmp(data,signature,48) ||
     read32(data+80)!=kNumberOfAssets)Die("Missing or incompatible original SMW assets");
  size_t offset=88+kNumberOfAssets*4+read32(data+84);
  for(unsigned i=0;i<kNumberOfAssets;i++) {
    uint32 size=read32(data+88+i*4);offset=(offset+3)&~(size_t)3;
    if(offset>length || size>length-offset)Die("Corrupt SMW assets");
    g_asset_ptrs[i]=data+offset;g_asset_sizes[i]=size;offset+=size;
  }
}
EMSCRIPTEN_KEEPALIVE int ww_web_init(void) {
  if(running)return 1;
  SDL_Init(SDL_INIT_EVENTS|SDL_INIT_TIMER);
  load_assets();if(!SnesInit(kRom,kRom_SIZE))return 0;
  g_spc_player=SmwSpcPlayer_Create();g_spc_player->initialize(g_spc_player);
  PpuBeginDrawing(g_snes->ppu,pixels,256*4,0);
  PpuBeginDrawing(g_my_ppu,other_pixels,256*4,0);
  mkdir("saves",0777);RtlReadSram();ww_init();running=true;return 1;
}
EMSCRIPTEN_KEEPALIVE int ww_web_step(uint32 inputs) {
  SDL_Event event;while(SDL_PollEvent(&event))if(event.type==SDL_QUIT)running=false;
  if(!running)return 0;
  if(ww_before_frame(&inputs))RtlRunFrame(inputs|(1u<<30));
  g_rtl_game_info->draw_ppu_frame();
  memcpy(screen,other_pixels,sizeof(screen));ww_render(screen,256*4,256,224);
  /* This game does not expose the original recorder's incompatible states. */
  if(++ticks%3600==0)RtlClearKeyLog();
  return 1;
}
EMSCRIPTEN_KEEPALIVE uint8 *ww_web_pixels(void) { return screen; }
EMSCRIPTEN_KEEPALIVE float *ww_web_audio(void) {
  if(fc_audio_music_active())memset(music,0,sizeof(music));
  else RtlRenderAudio(music,801,2);
  fc_audio_mix(audio,801);
  for(int i=0;i<801*2;i++) {
    float v=music[i]/32768.0f*0.65f+audio[i];
    audio[i]=v>1?1:v<-1?-1:v;
  }
  return audio;
}
EMSCRIPTEN_KEEPALIVE uint8 *ww_web_sram(void) { return g_sram; }
EMSCRIPTEN_KEEPALIVE int ww_web_sram_size(void) { return g_sram_size; }
/* Read-only diagnostics used by the smoke test and the browser status. */
EMSCRIPTEN_KEEPALIVE int ww_web_state(int field) {
  const FcFight *f=ww_fight_state();const FcOpponent *o=ww_opponent();
  switch(field) {
    case 0:return misc_game_mode;case 1:return f->hp;case 2:return f->meter;
    case 3:return player_xpos;case 4:return player_ypos;case 5:return misc_intro_level_flag;
    case 6:return ow_level_number_lo;case 7:return f->move;case 8:return ww_enabled();
    case 9:return o->hp;case 10:return o->max_hp;case 11:return f->events;
    case 12:return player_in_air_flag;case 13:return (int8)player_yspeed;
    case 14: {int n=0;for(int i=0;i<FC_MAX_SHOTS;i++)n+=f->shots[i].ttl>0;return n;}
    case 15:return player_current_state;case 16:return fc_audio_selection_active();
    case 17:return f->crouching_attack;
    case 18:return ww_score();case 19:return ww_high_score();
    case 20:return f->character;case 21:return ww_practicing();case 22:return ww_final_phase();
    default:return 0;
  }
}
