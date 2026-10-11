#ifndef WORLD_WARRIOR_AUDIO_H
#define WORLD_WARRIOR_AUDIO_H
#include <stdint.h>
#include "fight.h"
void fc_audio_init(const char *directory);
void fc_audio_context(const FcFight *fight);
void fc_audio_play(uint32_t events);
/* The web host mixes effects into the original SPC stream at 48 kHz. */
void fc_audio_mix(float *stereo,int frames);
void fc_audio_selection(bool active);
bool fc_audio_selection_active(void);
void fc_audio_shutdown(void);
#endif
