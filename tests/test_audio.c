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
static void expect_sample(const char *name,FcMove move,float gain) {
  /* Compare the mixer with the actual imported clip: silence/energy alone
   * would miss an accidental mapping back to Spinning Bird Kick or a grunt. */
  char path[128];snprintf(path,sizeof(path),"local/sf2/audio/%s.wav",name);
  SDL_AudioSpec spec;Uint8 *bytes;Uint32 length;
  assert(SDL_LoadWAV(path,&spec,&bytes,&length));
  SDL_AudioCVT cvt;assert(SDL_BuildAudioCVT(&cvt,spec.format,spec.channels,spec.freq,AUDIO_F32SYS,1,48000)>=0);
  cvt.len=(int)length;cvt.buf=SDL_malloc(length*cvt.len_mult);assert(cvt.buf);
  SDL_memcpy(cvt.buf,bytes,length);SDL_FreeWAV(bytes);assert(SDL_ConvertAudio(&cvt)==0);
  int size=cvt.needed?cvt.len_cvt:cvt.len;float *expected=(float*)cvt.buf;
  FcFight f={0};f.character=FC_CHUNLI;f.move=move;f.move_tick=8;fc_audio_context(&f);
  fc_audio_play(FC_SND_SPECIAL);
  float actual[2048];fc_audio_mix(actual,1024);assert(size/4>=1024);
  for(int i=0;i<1024;i++) {
    float v=fmaxf(-0.85f,fminf(0.85f,expected[i]*gain))*0.5f;
    assert(fabsf(actual[i*2]-v)<0.00001f && actual[i*2]==actual[i*2+1]);
  }
  SDL_free(cvt.buf);energy(180);
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
  fc_audio_play(FC_SND_PERFECT);assert(energy(180)>1);
  FcFight f={0};f.move=FC_SUPER_MOVE;fc_audio_context(&f);
  fc_audio_play(FC_SND_SUPER);assert(energy(180)>1);
  fc_audio_play(FC_SND_SPECIAL);assert(energy(180)>1); /* Shinku release voice. */
  f.character=FC_CHUNLI;f.move=FC_PROJECTILE;fc_audio_context(&f);
  fc_audio_play(FC_SND_SPECIAL);assert(energy(180)>1);
  expect_sample("kikoken",FC_PROJECTILE,0.45f);
  expect_sample("lightning_kick",FC_FLURRY,0.45f);
  expect_sample("bird",FC_SPIN,1.0f);
  f.character=FC_BISON;f.move=FC_SUPER_MOVE;fc_audio_context(&f);
  fc_audio_play(FC_SND_SPECIAL);assert(energy(180)>1);
  int16_t mixed[2400];
  for(int i=0;i<2400;i++)mixed[i]=10000;
  fc_audio_mix_s16(mixed,1200,0.65f);
  for(int i=0;i<2400;i++)assert(mixed[i]==6500);
  fc_audio_play(FC_SND_BOSS_KO);
  int changed=0;
  for(int t=0;t<20;t++) {
    for(int i=0;i<2400;i++)mixed[i]=10000;
    fc_audio_mix_s16(mixed,1200,0);
    for(int i=0;i<2400;i++)changed+=mixed[i]!=0;
  }
  assert(changed>100);
  fc_audio_shutdown();SDL_Quit();puts("Audio: KO voice, fighter death, selection loop, continue music and stop passed.");
}
