#ifndef WORLD_WARRIOR_SMW_H
#define WORLD_WARRIOR_SMW_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "fight.h"
void ww_init(void);
void ww_set_debug(bool enabled);
bool ww_debug_enabled(void);
int ww_menu_state(void);
bool ww_before_frame(uint32_t *inputs);
void ww_render(uint8_t *pixels,size_t pitch,int width,int height);
bool ww_damage(void);
bool ww_take_damage(int amount,int direction);
bool ww_perfect_clear(void);
bool ww_yoshi_tongue_pressed(void);
bool ww_powerup(uint8_t sprite);
bool ww_psycho_position(void);
bool ww_psycho_collision(void);
bool ww_fast_scroll(void);
void ww_carried_position(uint8_t slot);
bool ww_keyhole_contact(uint8_t slot);
void ww_on_death(void);
void ww_velocity(void);
void ww_position_end(void);
void ww_player_draw(void);
void ww_begin_frame(void);
bool ww_hide_native_hud(void);
bool ww_enabled(void);
bool ww_practicing(void);
const FcFight *ww_fight_state(void);
#endif
