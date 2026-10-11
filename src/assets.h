#ifndef WORLD_WARRIOR_ASSETS_H
#define WORLD_WARRIOR_ASSETS_H
#include "draw.h"
enum { FC_POSE_IDLE,FC_POSE_WALK,FC_POSE_JUMP,FC_POSE_DUCK,FC_POSE_PUNCH,
  FC_POSE_KICK,FC_POSE_FIRE,FC_POSE_RISE,FC_POSE_SPIN,FC_POSE_RUSH,
  FC_POSE_FLURRY,FC_POSE_HURT,FC_POSE_KO,FC_POSE_GUARD,FC_POSE_SHOT,FC_POSE_PORTRAIT,FC_POSE_MAP,
  FC_POSE_FONT,FC_POSE_BIG_FONT,FC_POSE_KO_MARK,FC_POSE_WIN,
  FC_POSE_AIR_PUNCH,FC_POSE_AIR_KICK,FC_POSE_COUNT };
bool fc_assets_load(const char *path);
void fc_assets_free(void);
bool fc_assets_ready(void);
void fc_sprite(FcCanvas c,int character,int pose,unsigned tick,int x,int feet,
               int facing,int numerator,int denominator,uint32_t tint);
#endif
