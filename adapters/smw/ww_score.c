#include "ww_score.h"
#include "ww_smw.h"
#include "smw_rtl.h"
#include "variables.h"
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
void ww_update_high_score(void) {
  /* Attract-mode Mario can score too; only a real player's run qualifies. */
  if(!ww_enabled() || misc_intro_level_flag || !g_sram || g_sram_size<RECORD_OFFSET+RECORD_SIZE)return;
  uint32_t score=ww_score();if(score<=ww_high_score())return;
  uint8_t *p=g_sram+RECORD_OFFSET;
  memcpy(p,"SMW2HI\0\1",8);write32(p+8,score);write32(p+12,~score);
  RtlWriteSram();
}
