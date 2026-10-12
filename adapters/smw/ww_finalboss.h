#ifndef WORLD_WARRIOR_FINALBOSS_H
#define WORLD_WARRIOR_FINALBOSS_H
#include "draw.h"
enum { WW_FINAL_INTRO,WW_FINAL_ARRIVAL,WW_FINAL_DEMON,WW_FINAL_FALL,WW_FINAL_FIGHT,WW_FINAL_END };
enum { WW_AKUMA_RETREAT,WW_AKUMA_PAUSE,WW_AKUMA_AIR,WW_AKUMA_HORIZONTAL,WW_AKUMA_UPPERCUT,WW_AKUMA_BURST,WW_AKUMA_RECOVER };
void ww_final_reset(void);
bool ww_final_step(unsigned slot);
void ww_final_tiles(unsigned slot);
void ww_final_render(FcCanvas canvas);
bool ww_final_controls(unsigned slot);
bool ww_final_cinematic(void);
int ww_final_phase(void);
int ww_final_tick(void);
int ww_final_attack(void);
int ww_final_attack_tick(void);
bool ww_final_vulnerable(void);
int ww_final_limit_damage(int damage,int hp);
void ww_final_grapple(bool held);
void ww_final_grapple_art(FcCanvas canvas);
int ww_final_target(unsigned shot,FcTarget *target);
void ww_final_hit_shot(unsigned shot);
void ww_final_defeated(void);
#endif
