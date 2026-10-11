#ifndef WORLD_WARRIOR_AUDIO_H
#define WORLD_WARRIOR_AUDIO_H
#include <stdint.h>
#include "fight.h"
void fc_audio_init(const char *directory);
void fc_audio_context(const FcFight *fight);
void fc_audio_play(uint32_t events);
void fc_audio_shutdown(void);
#endif
