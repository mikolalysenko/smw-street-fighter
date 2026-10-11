#ifndef WORLD_WARRIOR_SMW_H
#define WORLD_WARRIOR_SMW_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "fight.h"
void ww_init(void);
bool ww_before_frame(uint32_t *inputs);
void ww_render(uint8_t *pixels,size_t pitch,int width,int height);
bool ww_damage(void);
void ww_on_death(void);
void ww_velocity(void);
void ww_player_draw(void);
bool ww_enabled(void);
const FcFight *ww_fight_state(void);
#endif
