#ifndef WORLD_WARRIOR_ENDING_H
#define WORLD_WARRIOR_ENDING_H
#include "draw.h"
void ww_ending_reset(void);
void ww_ending_win(int character);
bool ww_ending_finish_level(void);
bool ww_ending_tick(uint32_t edge);
bool ww_ending_render(FcCanvas canvas);
int ww_ending_character(void);
#endif
