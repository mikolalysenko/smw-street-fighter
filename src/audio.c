#include "audio.h"
#include <SDL.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

enum { RATE=48000, SOUND_COUNT=41, VOICE_COUNT=12 };
typedef struct { float *data; int length; } Sample;
typedef struct { int sound,position; bool active; } Voice;
static SDL_AudioDeviceID device;
static SDL_mutex *voice_mutex;
static bool shared_device;
static Sample samples[SOUND_COUNT];
static Voice voices[VOICE_COUNT];
static int next_voice;
static int character,move,move_tick;
static bool loaded;
static SDL_atomic_t music_track;
static int music_position;

void fc_audio_mix(float *out,int frames) {
  if(voice_mutex)SDL_LockMutex(voice_mutex);
  for(int i=0;i<frames;i++) {
    float v=0;
    int track=SDL_AtomicGet(&music_track);
    if(track && samples[19+track].length) {
      Sample *s=&samples[19+track];
      if(music_position>=s->length && track==FC_MUSIC_SELECT)music_position=0;
      if(music_position<s->length)v=s->data[music_position++]*0.7f;
    }
    for(int j=0;j<VOICE_COUNT;j++)if(voices[j].active) {
      Voice *voice=&voices[j];Sample *s=&samples[voice->sound];
      if(voice->position<s->length)v+=s->data[voice->position++];else voice->active=false;
    }
    if(v>0.85f)v=0.85f;if(v<-0.85f)v=-0.85f;
    out[i*2]=out[i*2+1]=v*0.5f;
  }
  if(voice_mutex)SDL_UnlockMutex(voice_mutex);
}
void fc_audio_use_shared_device(void) { shared_device=true; }
void fc_audio_mix_s16(int16_t *out,int frames,float music_gain) {
  float effects[1024];
  while(frames>0) {
    int count=frames<512?frames:512;fc_audio_mix(effects,count);
    for(int i=0;i<count*2;i++) {
      float value=out[i]*music_gain+effects[i]*32768.0f;
      out[i]=(int16_t)(value>32767?32767:value<-32768?-32768:value);
    }
    frames-=count;out+=count*2;
  }
}
#if !defined(__EMSCRIPTEN__) && !defined(FC_AUDIO_MANUAL)
static void mix(void *unused,Uint8 *stream,int length) {
  (void)unused;fc_audio_mix((float*)stream,length/(int)(2*sizeof(float)));
}
#endif
static Sample synth(int sound) {
  const float durations[]={0.12f,0.20f,0.27f,0.85f,0.38f,0.70f,0.14f,0.09f};
  Sample s={NULL,(int)(durations[sound]*RATE)};
  s.data=calloc((size_t)s.length,sizeof(float));if(!s.data){s.length=0;return s;}
  float phase=0;uint32_t noise=0x123456u;
  for(int i=0;i<s.length;i++) {
    float t=(float)i/RATE,u=(float)i/s.length,env=(1-u)*(1-u);
    float freq=100,v=0;noise=noise*1664525u+1013904223u;
    float n=(float)(noise&65535)/32768-1;
    switch(sound) {
      case 0:freq=220-150*u;v=n*0.65f;break;
      case 1:freq=145-85*u;v=n*(u<0.10f?1.0f:0.22f);break;
      case 2:freq=170-100*u;v=n*0.25f;break;
      case 3:freq=190-145*u+8*sinf(t*45);v=0.20f*sinf(t*2800)*sinf(t*1100);break;
      case 4:freq=320+600*u;v=n*0.4f;break;
      case 5:freq=140+1500*u*u;v=0.35f*sinf(t*2100)+n*0.18f;break;
      case 6:freq=640-120*u;v=n*0.5f;break;
      case 7:freq=660+(u>0.5f?220:0);break;
    }
    phase+=freq*6.2831853f/RATE;
    v+=sinf(phase)*0.65f;
    if(sound==3)v+=sinf(phase*2.03f)*0.22f;
    s.data[i]=v*env*0.30f*(i<96?(float)i/96:1);
  }
  return s;
}
static bool load_sample(const char *dir,const char *name,Sample *dest) {
  if(!dir)return false;
  char path[4096];if(snprintf(path,sizeof(path),"%s/%s.wav",dir,name)>=(int)sizeof(path))return false;
  SDL_AudioSpec spec;Uint8 *bytes=NULL;Uint32 length=0;
  if(!SDL_LoadWAV(path,&spec,&bytes,&length))return false;
  SDL_AudioCVT cvt;
  if(length>16*1024*1024 || SDL_BuildAudioCVT(&cvt,spec.format,spec.channels,spec.freq,AUDIO_F32SYS,1,RATE)<0) {
    SDL_FreeWAV(bytes);return false;
  }
  cvt.len=(int)length;cvt.buf=SDL_malloc((size_t)length*cvt.len_mult);
  if(!cvt.buf){SDL_FreeWAV(bytes);return false;}
  SDL_memcpy(cvt.buf,bytes,length);SDL_FreeWAV(bytes);
  if(SDL_ConvertAudio(&cvt)<0){SDL_free(cvt.buf);return false;}
  int size=cvt.needed?cvt.len_cvt:cvt.len;
  float *copy=malloc((size_t)size);
  if(!copy){SDL_free(cvt.buf);return false;}
  SDL_memcpy(copy,cvt.buf,(size_t)size);SDL_free(cvt.buf);
  free(dest->data);*dest=(Sample){copy,size/(int)sizeof(float)};
  return true;
}
void fc_audio_init(const char *directory) {
  if(loaded)return;
  voice_mutex=SDL_CreateMutex();
  static const char *names[]={"swing","hit","hurt","ko","special","super","block","select",
    "hadouken","shoryuken","tatsumaki","honda","sonic","bird","chun_hurt","chun_ko","confirm","you","lose","boss_ko","player_select","continue","perfect","alpha_charge","akuma_super","akuma_warp",
    "blanka","electric","zangief","yoga_fire","yoga_flame","vega","tiger","tiger_uppercut","psycho","psycho_super","shinku","alpha_hadouken","kikoken","lightning_kick","chun_attack"};
  for(int i=0;i<SOUND_COUNT;i++) {
    if(!directory && i<8)samples[i]=synth(i);
    load_sample(directory,names[i],&samples[i]);
    /* These SND rips arrive about 6–9 dB hotter than the other voices.
     * Trim before summing so they neither dominate nor hit the limiter. */
    float gain=i==38?0.45f:i==39?0.45f:i==40?0.38f:1.0f;
    if(gain!=1.0f)for(int j=0;j<samples[i].length;j++)samples[i].data[j]*=gain;
  }
  loaded=true;
#if !defined(__EMSCRIPTEN__) && !defined(FC_AUDIO_MANUAL)
  if(!shared_device) {
  SDL_AudioSpec wanted={0};wanted.freq=RATE;wanted.format=AUDIO_F32SYS;
  wanted.channels=2;wanted.samples=512;wanted.callback=mix;
  device=SDL_OpenAudioDevice(NULL,0,&wanted,NULL,0);
  if(device)SDL_PauseAudioDevice(device,0);
  else fprintf(stderr,"Audio unavailable: %s\n",SDL_GetError());
  }
#endif
}
void fc_audio_context(const FcFight *f) { character=f->character;move=f->move;move_tick=f->move_tick; }
bool fc_audio_music_active(void) { return SDL_AtomicGet(&music_track)!=FC_MUSIC_NONE; }
bool fc_audio_selection_active(void) { return SDL_AtomicGet(&music_track)==FC_MUSIC_SELECT; }
void fc_audio_selection(bool active) { fc_audio_music(active?FC_MUSIC_SELECT:FC_MUSIC_NONE); }
void fc_audio_music(int track) {
  if(track<FC_MUSIC_NONE || track>FC_MUSIC_CONTINUE)return;
  if(track==SDL_AtomicGet(&music_track))return;
  if(voice_mutex)SDL_LockMutex(voice_mutex);
  music_position=0;SDL_AtomicSet(&music_track,track);
  if(voice_mutex)SDL_UnlockMutex(voice_mutex);
}
void fc_audio_play(uint32_t events) {
  if(!loaded || !events)return;
  if(voice_mutex)SDL_LockMutex(voice_mutex);
  for(int i=0;i<12;i++)if(events&(1u<<i)) {
    int sound=i==11?25:i==10?24:i==9?22:i==8?19:i;
    if(character==FC_CHUNLI && i==2)sound=14;
    if(character==FC_CHUNLI && i==3)sound=15;
    if((i==2 || i==3) && character==FC_BLANKA)sound=26;
    if((i==2 || i==3) && character==FC_ZANGIEF)sound=28;
    if((i==2 || i==3) && character==FC_VEGA)sound=31;
    if(i==5) {
      sound=23;
      if(character==FC_RYU && samples[36].data) {
        voices[next_voice]=(Voice){36,0,true};next_voice=(next_voice+1)%VOICE_COUNT;
      }
    }
    if(i==4) {
      sound=character==FC_HONDA?11:character==FC_GUILE?12:character==FC_CHUNLI?13:
        move==FC_RISING?9:move==FC_SPIN?10:8;
      if(character==FC_KEN && move==FC_SUPER_MOVE)sound=9;
      if(character==FC_RYU && move==FC_SUPER_MOVE)sound=37;
      if(character==FC_CHUNLI && move==FC_PROJECTILE)sound=38;
      if(character==FC_CHUNLI && (move==FC_FLURRY || move==FC_SUPER_MOVE)) {
        sound=39;
        /* Lightning Legs has its own short cry and repeated kick swishes;
         * Spinning Bird Kick's spoken call belongs only to that move. */
        if(move_tick<=1 && samples[40].data) {
          voices[next_voice]=(Voice){40,0,true};next_voice=(next_voice+1)%VOICE_COUNT;
        }
      }
      if(character==FC_BLANKA)sound=move==FC_ELECTRIC?27:26;
      if(character==FC_ZANGIEF)sound=28;
      if(character==FC_DHALSIM)sound=move==FC_PROJECTILE?29:30;
      if(character==FC_BALROG)sound=0;
      if(character==FC_VEGA)sound=31;
      if(character==FC_SAGAT)sound=move==FC_PROJECTILE?32:33;
      if(character==FC_BISON)sound=move==FC_SUPER_MOVE?35:34;
    }
    if(!samples[sound].data && i<8)sound=i;
    if(!samples[sound].data)continue;
    voices[next_voice]=(Voice){sound,0,true};next_voice=(next_voice+1)%VOICE_COUNT;
  }
  if(voice_mutex)SDL_UnlockMutex(voice_mutex);
}
void fc_audio_shutdown(void) {
  if(device)SDL_CloseAudioDevice(device);device=0;loaded=false;
  if(voice_mutex)SDL_DestroyMutex(voice_mutex);voice_mutex=NULL;
  for(int i=0;i<SOUND_COUNT;i++){free(samples[i].data);samples[i]=(Sample){0};}
  SDL_memset(voices,0,sizeof(voices));next_voice=0;
  SDL_AtomicSet(&music_track,0);music_position=0;
}
