#ifndef WORLD_WARRIOR_TITLE_H
#define WORLD_WARRIOR_TITLE_H
#include "draw.h"
struct Ppu;
void ww_title_reset(void);
void ww_title_tick(void);
void ww_title_begin(struct Ppu *ppu);
void ww_title_end(struct Ppu *ppu);
bool ww_title_render(FcCanvas canvas,unsigned high_score);
#endif
