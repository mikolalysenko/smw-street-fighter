#include "ww_score.h"
#include "ww_smw.h"
#include "smw_rtl.h"
#include "variables.h"
#include "funcs.h"
#include <string.h>

/* Original USA SMW uses bytes 0..857 for its three files and their backups.
 * A versioned record in the unused SRAM tail travels with the same .srm file
 * and browser export without changing any native file/checksum bytes. */
enum { RECORD_OFFSET=0x7f0,RECORD_SIZE=16,MAX_SCORE=9999990 };
static uint32_t read32(const uint8_t *p) {
  return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static void write32(uint8_t *p,uint32_t n) {
  for(int i=0;i<4;i++)p[i]=(uint8_t)(n>>(i*8));
}
uint32_t ww_score(void) {
  uint32_t score=player_mario_score_lo|((uint32_t)player_mario_score_mid<<8)|
    ((uint32_t)player_mario_score_hi<<16);
  return score>MAX_SCORE/10?MAX_SCORE:score*10;
}
uint32_t ww_high_score(void) {
  if(!g_sram || g_sram_size<RECORD_OFFSET+RECORD_SIZE)return 0;
  const uint8_t *p=g_sram+RECORD_OFFSET;
  uint32_t score=read32(p+8);
  if(memcmp(p,"SMW2HI\0\1",8) || read32(p+12)!=~score || score>MAX_SCORE || score%10)return 0;
  return score;
}
void ww_combat_score(const FcFight *f) {
  if(!f->reward_count)return;
  uint32_t score=ww_score()/10+(unsigned)f->score_gain/10;
  if(score>MAX_SCORE/10)score=MAX_SCORE/10;
  player_mario_score_lo=score;player_mario_score_mid=score>>8;player_mario_score_hi=score>>16;
  int lives=misc_1up_handler+f->lives_gain;
  int capacity=98-player_current_life_count;
  if(capacity<0)capacity=0;
  if(lives>capacity)lives=capacity;
  misc_1up_handler=(uint8)lives;
  if(f->lives_gain)timer_give1up=0;
  static const int points[]={200,400,800,1000,2000,4000,8000};
  int count=f->reward_count<6?f->reward_count:6;
  for(int i=0;i<count;i++) {
    const FcReward *r=&f->rewards[i];int id=6;
    if(r->award<0)id=12-r->award;
    else for(int j=0;j<7;j++)if(points[j]==r->award){id=6+j;break;}
    int k=CheckAvailableScoreSpriteSlot();
    score_spr_spriteid[k]=(uint8)id;
    uint16 x=(uint16)(int)r->x,y=(uint16)(int)(r->y-8);
    score_spr_xpos_lo[k]=x;score_spr_xpos_hi[k]=x>>8;
    score_spr_ypos_lo[k]=y;score_spr_ypos_hi[k]=y>>8;
    score_spr_layer_index[k]=0;
    /* Credit immediately above. Native awards occur at timer 42; begin
     * below that so rendering a popup cannot award the same hit twice. */
    score_spr_yspeed[k]=41;
  }
}
void ww_update_high_score(void) {
  /* Attract-mode Mario can score too; only a real player's run qualifies. */
  if(!ww_enabled() || ww_practicing() || misc_intro_level_flag || !g_sram || g_sram_size<RECORD_OFFSET+RECORD_SIZE)return;
  uint32_t score=ww_score();if(score<=ww_high_score())return;
  uint8_t *p=g_sram+RECORD_OFFSET;
  memcpy(p,"SMW2HI\0\1",8);write32(p+8,score);write32(p+12,~score);
  RtlWriteSram();
}
