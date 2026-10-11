#ifndef WORLD_WARRIOR_TARGETS_H
#define WORLD_WARRIOR_TARGETS_H
#include "fight.h"
enum { WW_TARGET_COUNT=42 };
void ww_targets_reset(void);
void ww_target_spawned(unsigned slot);
void ww_targets_collect(FcFight *fight, FcTarget *targets);
void ww_targets_apply(FcFight *fight, FcTarget *targets);
void ww_scenery_hits(FcFight *fight);
const FcOpponent *ww_opponent(void);
#endif
