#ifndef WORLD_WARRIOR_DRAW_H
#define WORLD_WARRIOR_DRAW_H
#include "fight.h"
#include <stddef.h>
typedef struct { uint32_t *pixels; int w,h,pitch; } FcCanvas;
void fc_rect(FcCanvas c,int x,int y,int w,int h,uint32_t color);
void fc_text(FcCanvas c,int x,int y,const char *text,uint32_t color,int scale);
void fc_center(FcCanvas c,int y,const char *text,uint32_t color,int scale);
void fc_fighter(FcCanvas c,const FcFight *f,int x,int feet,int scale,uint32_t tint);
void fc_effects(FcCanvas c,const FcFight *f,float camera_x,float camera_y);
void fc_hud(FcCanvas c,const FcFight *f);
void fc_battle_hud(FcCanvas c,const FcFight *f,const FcOpponent *opponent);
void fc_selection(FcCanvas c,int choice,unsigned tick,bool compact);
void fc_title(FcCanvas c,unsigned tick,unsigned high_score);
void fc_score_hud(FcCanvas c,unsigned score,unsigned high_score,bool boss);
void fc_pause(FcCanvas c,int choice,bool boss_shortcut);
void fc_continue(FcCanvas c,int character,int ticks);
void fc_level_clear(FcCanvas c,int character,bool perfect);
void fc_dim(FcCanvas c,int amount);
#endif
