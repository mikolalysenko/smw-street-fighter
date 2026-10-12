#ifndef WORLD_WARRIOR_TARGETS_H
#define WORLD_WARRIOR_TARGETS_H
#include "fight.h"
#include "draw.h"
enum { WW_PIPE_TARGET=54,WW_TARGET_COUNT=55 };
void ww_targets_reset(void);
void ww_target_spawned(unsigned slot);
void ww_targets_collect(FcFight *fight, FcTarget *targets);
void ww_targets_apply(FcFight *fight, FcTarget *targets);
void ww_scenery_hits(FcFight *fight);
const FcOpponent *ww_opponent(void);
int ww_hit_finalboss(FcFight *fight,int damage);
void ww_grapple_sync(FcFight *fight);
bool ww_grapple_sprite(uint8_t slot);
void ww_grapple_render(FcCanvas canvas,bool front);
#endif
