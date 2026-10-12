#ifndef WORLD_WARRIOR_FINALBOSS_H
#define WORLD_WARRIOR_FINALBOSS_H
#include "draw.h"
enum { WW_FINAL_INTRO,WW_FINAL_ARRIVAL,WW_FINAL_DEMON,WW_FINAL_FALL,WW_FINAL_FIGHT,WW_FINAL_END };
void ww_final_reset(void);
bool ww_final_step(unsigned slot);
void ww_final_tiles(unsigned slot);
void ww_final_render(FcCanvas canvas);
bool ww_final_controls(unsigned slot);
bool ww_final_cinematic(void);
int ww_final_phase(void);
int ww_final_tick(void);
int ww_final_target(unsigned shot,FcTarget *target);
void ww_final_hit_shot(unsigned shot);
void ww_final_defeated(void);
#endif
