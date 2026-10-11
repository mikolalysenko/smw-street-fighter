#include "audio.h"
#include <SDL.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
static double energy(int frames) {
  float buffer[1602];double total=0;
  for(int t=0;t<frames;t++) {
    fc_audio_mix(buffer,801);
    for(int i=0;i<1602;i++){assert(isfinite(buffer[i]));total+=buffer[i]*buffer[i];}
  }
  return total;
}
int main(void) {
  assert(SDL_Init(SDL_INIT_AUDIO)==0);
  fc_audio_init("local/sf2/audio");
  assert(energy(1)==0);
  fc_audio_play(FC_SND_BOSS_KO);assert(energy(240)>1);
  energy(300);assert(energy(2)==0);
  fc_audio_selection(true);assert(fc_audio_selection_active());assert(energy(240)>1);
  assert(energy(2400)>1); /* Run past the 38-second loop boundary. */
  fc_audio_selection(false);assert(energy(10)==0);
  fc_audio_music(FC_MUSIC_CONTINUE);assert(fc_audio_music_active());assert(!fc_audio_selection_active());
  assert(energy(240)>1);fc_audio_music(FC_MUSIC_NONE);assert(energy(10)==0);
  fc_audio_play(FC_SND_KO);assert(energy(180)>1);
  fc_audio_shutdown();SDL_Quit();puts("Audio: KO voice, fighter death, selection loop, continue music and stop passed.");
}
