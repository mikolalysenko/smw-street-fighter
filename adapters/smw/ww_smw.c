#include "ww_smw.h"
#include "fight.h"
#include "draw.h"
#include "audio.h"
#include "assets.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <SDL.h>
#include <stdlib.h>
#include <string.h>

static FcFight fight;
static bool initialized,chosen,was_level;
static int menu,choice,pause_choice;
static uint32_t previous;
static bool slot_alive[12];
void WW_OriginalPlayerDraw(void);

void ww_init(void) {
  fc_init(&fight,FC_RYU);fight.meter=100;initialized=true;
  chosen=was_level=false;menu=choice=pause_choice=0;previous=0;
  memset(slot_alive,0,sizeof(slot_alive));
  const char *sprites=getenv("WW_SPRITES");
  fc_assets_load(sprites?sprites:"sf2/sprites.dat");
  const char *audio=getenv("WW_AUDIO_DIR");
  fc_audio_init(audio?audio:"sf2/audio");
}
const FcFight *ww_fight_state(void) { return &fight; }
bool ww_enabled(void) { return initialized && chosen && misc_game_mode==0x14; }
static uint32_t decode(uint32_t b) {
  return ((b&(1u<<6))?FC_LEFT:0)|((b&(1u<<7))?FC_RIGHT:0)|
    ((b&(1u<<4))?FC_UP:0)|((b&(1u<<5))?FC_DOWN:0)|
    ((b&1)?FC_JUMP:0)|((b&2)?FC_PUNCH:0)|((b&(1u<<8))?FC_KICK:0)|
    ((b&(1u<<9))?FC_SPECIAL:0)|((b&(1u<<11))?FC_SUPER:0)|
    ((b&(1u<<10))?FC_GUARD:0)|((b&8)?FC_START:0)|((b&4)?FC_BACK:0);
}
static bool attackable(uint8_t id) {
  /* Explicit first-slice whitelist: koopas, bob-ombs, goombas, beetles,
   * spinies, fish, bullet bills, dino rhinos/torches. Never indiscriminately
   * kill every sprite: keys, platforms, switches, Yoshi and bosses are sprites. */
  return id<=0x0d || id==0x0f || id==0x10 || id==0x11 ||
    (id>=0x13 && id<=0x18) || id==0x1c || id==0x6e || id==0x6f;
}
static FcBody body(void) {
  /* Keep SMW's compact terrain collision hull; the combat sprite is larger.
   * Using the engine's clipping function also follows ducking and Yoshi. */
  CollInfo ci={0};StandardSpriteToSpriteCollisionChecks_GetMarioClipping(&ci);
  FcBody b={{(float)(ci.r0|(ci.r8<<8)),(float)(ci.r1|(ci.r9<<8)),ci.r2,ci.r3},
    player_facing_direction?1:-1,!player_in_air_flag};
  return b;
}
static void quit(void) { SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e); }
bool ww_before_frame(uint32_t *inputs) {
  if(!initialized)return true;
  uint32_t b=decode(*inputs),edge=b&~previous;previous=b;
  bool in_level=misc_game_mode==0x14;
  if(!in_level) {
    if(was_level){fc_clear_actions(&fight);memset(slot_alive,0,sizeof(slot_alive));}
    was_level=false;menu=0;
    if((b&(FC_START|FC_BACK))==(FC_START|FC_BACK))quit();
    return true;
  }
  if(!was_level) {
    was_level=true;
    int meter=fight.hp>0?fight.meter:100;
    fc_init(&fight,choice);fight.meter=meter;
    if(!chosen)menu=1;
  }
  if(menu==1) {
    ++fight.tick;
    if(edge&(FC_LEFT|FC_RIGHT|FC_UP|FC_DOWN)) {
      int delta=(edge&FC_LEFT)?3:(edge&FC_RIGHT)?1:2;
      choice=(choice+delta)%FC_ROSTER_COUNT;fc_audio_play(FC_SND_SELECT);
    }
    if(edge&(FC_JUMP|FC_START|FC_PUNCH)) {
      /* Switching fighter in a paused level keeps the health percentage. */
      int hp=fight.hp*fc_roster[choice].vitality/fc_roster[fight.character].vitality;
      int meter=fight.meter;fc_init(&fight,choice);fight.hp=hp;fight.meter=meter;
      fight.previous=b;chosen=true;menu=0;fc_audio_context(&fight);fc_audio_play(FC_SND_SELECT);
    }
    if(edge&FC_BACK){if(chosen)menu=2;else quit();}
    return false;
  }
  if(menu==2) {
    if(edge&(FC_UP|FC_DOWN))pause_choice=(pause_choice+((edge&FC_DOWN)?1:2))%3;
    if(edge&FC_BACK)menu=0;
    if(edge&(FC_START|FC_JUMP|FC_PUNCH)) {
      if(pause_choice==2)quit();else menu=pause_choice==1?1:0;
    }
    fight.previous=b;return false;
  }
  if((edge&(FC_START|FC_BACK)) && player_current_state==0 && !timer_end_level &&
     !misc_intro_level_flag && !misc_display_message && !flag_sprites_locked) {
    menu=2;pause_choice=0;return false;
  }
  if(!fight.hp && player_current_state==9) {
    ++fight.tick;++fight.ko_timer;
    return true;
  }
  if(timer_end_level || misc_display_message || player_current_state!=0 || flag_pause || flag_sprites_locked) {
    fight.previous=b;
    return true;
  }
  /* No cape/fire form; pickups and the underlying level logic still execute. */
  player_current_power_up=1;
  FcTarget targets[12]={0};
  for(int i=0;i<12;i++) {
    bool alive=spr_current_status[i]==8 && attackable(spr_spriteid[i]);
    if(!slot_alive[i]) {
      uint64_t mask=~(UINT64_C(1)<<i);fight.melee_mask&=mask;
      for(int j=0;j<FC_MAX_SHOTS;j++)fight.shots[j].hit_mask&=mask;
    }
    slot_alive[i]=alive;
    if(alive) {
      CollInfo ci={0};StandardSpriteToSpriteCollisionChecks_GetSpriteClippingB((uint8)i,&ci);
      targets[i]=(FcTarget){{(float)(ci.r0|(ci.r8<<8)),(float)(ci.r1|(ci.r9<<8)),ci.r2,ci.r3},1,true,false,0,0};
    }
  }
  fc_tick(&fight,b,body(),targets,12);
  fc_audio_context(&fight);
  for(int i=0;i<12;i++)if(targets[i].hit) {
    spr_current_status[i]=2;
    spr_xspeed[i]=(uint8)(int8)(targets[i].knock_x*16);
    spr_yspeed[i]=(uint8)(int8)(targets[i].knock_y*16);
    GivePoints((uint8)i,1);slot_alive[i]=false;
  }
  fc_audio_play(fight.events);
  /* Reserve combat buttons; keep B jump and all directions. Automatic run
   * preserves SMW's long jumps without a second held face button. */
  *inputs&=~(2u|4u|8u|(1u<<8)|(1u<<9)|(1u<<10)|(1u<<11));
  if(b&(FC_LEFT|FC_RIGHT))*inputs|=2;
  if(fc_busy(&fight) || fight.guarding)*inputs&=~(0xf0u|1u|2u);
  return !fight.frozen;
}
bool ww_damage(void) {
  if(!ww_enabled())return false;
  if(player_current_state || timer_end_level || timer_star_power || timer_player_hurt)return true;
  int direction=-fight.facing;
  if(spr_current_slotid<12 && spr_current_status[spr_current_slotid])
    direction=GetSprXPos(spr_current_slotid)>player_xpos?1:-1;
  fight.body=body();fight.events=0;
  if(fc_hurt(&fight,12,direction)) {
    timer_player_hurt=(uint8)fight.invuln;
    if(!fight.hp)DamagePlayer_Kill();
  }
  fc_audio_play(fight.events);fight.events=0;
  return true;
}
void ww_on_death(void) {
  if(!ww_enabled())return;
  fight.body=body();fc_ko(&fight);fc_audio_play(fight.events);fight.events=0;
}
void ww_velocity(void) {
  if(!ww_enabled() || player_current_state || timer_end_level || menu)return;
  if(fight.override_x)player_xspeed=(uint8)(int8)(fight.velocity_x*16);
  if(fight.override_y) {
    player_yspeed=(uint8)(int8)(fight.velocity_y*16);
    if(fight.velocity_y<0)player_in_air_flag=11;
    fight.override_y=false;
  }
}
void ww_player_draw(void) {
  if(!ww_enabled()){WW_OriginalPlayerDraw();return;}
  /* Run palette/star/pose side effects, suppress only the OAM writes made by
   * PlayerDraw. Original terrain and enemy rendering stay on the SNES PPU. */
  uint8 old_oam[512],old_sizes[128];
  memcpy(old_oam,oam_buf,sizeof(old_oam));memcpy(old_sizes,sprites_oamtile_size_buffer,sizeof(old_sizes));
  WW_OriginalPlayerDraw();
  memcpy(oam_buf,old_oam,sizeof(old_oam));memcpy(sprites_oamtile_size_buffer,old_sizes,sizeof(old_sizes));
}
void ww_render(uint8_t *pixels,size_t pitch,int width,int height) {
  if(!initialized || misc_game_mode!=0x14)return;
  FcCanvas c={(uint32_t*)pixels,width,height,(int)(pitch/4)};
  if(menu==1){fc_selection(c,choice,fight.tick,true);return;}
  if(!chosen)return;
  int x=(int16)(player_xpos-mirror_current_layer1_xpos)+8;
  int feet=(int16)(player_ypos-mirror_current_layer1_ypos)+32;
  if(player_hide_player_tile_flags!=0xff)fc_fighter(c,&fight,x,feet,1,0);
  fc_effects(c,&fight,mirror_current_layer1_xpos,mirror_current_layer1_ypos);
  fc_hud(c,&fight);
  if(menu==2)fc_pause(c,pause_choice);
}
