#ifndef WORLD_WARRIOR_SCORE_H
#define WORLD_WARRIOR_SCORE_H
#include <stdint.h>
#include "fight.h"
uint32_t ww_score(void);
uint32_t ww_high_score(void);
void ww_update_high_score(void);
void ww_combat_score(const FcFight *fight);
const char *ww_save_directory(void);
#endif
