#include "fight.h"
#include "draw.h"
#include "audio.h"
#include "assets.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WIDTH=384, HEIGHT=216, ENEMIES=28, LEVEL_END=2440, GROUND=176 };
typedef struct { FcTarget target; float vx,vy; int kind,stun; } Enemy;
typedef struct { float x,y,w; } Platform;
static const Platform platforms[]={ {365,140,78},{525,111,65},{872,139,96},
  {1080,112,78},{1310,141,110},{1720,139,90},{1900,111,96},{2120,141,90} };
typedef struct {
  FcFight fight;
  Enemy enemy[ENEMIES];
  float x,y,vx,vy,camera;
  int facing,coyote,jump_buffer,screen,selected,pause_choice,finish;
  bool grounded,quit;
  uint32_t previous;
  unsigned tick;
} Dojo;
/* screen: 0 character select, 1 game, 2 pause. */
static void begin(Dojo *g) {
  fc_init(&g->fight,g->selected);g->fight.meter=100;
  g->x=64;g->y=GROUND-32;g->vx=g->vy=g->camera=0;
  g->grounded=true;g->facing=1;g->screen=1;g->finish=0;
  g->coyote=g->jump_buffer=0;
  for(int i=0;i<ENEMIES;i++) {
    float x=215+(i/4)*305+(i%4)*35;
    g->enemy[i]=(Enemy){.target={{x,GROUND-18,17,18},i%4==3?3:1,true,false,0,0},.kind=i%4==3};
  }
}
static int remaining(const Dojo *g) {
  int n=0;for(int i=0;i<ENEMIES;i++)if(g->enemy[i].target.hp>0)n++;return n;
}
static bool ground_at(float x) { return !(x>736 && x<776) && !(x>1610 && x<1650); }
static void tick(Dojo *g,uint32_t buttons) {
  uint32_t edge=buttons&~g->previous;g->previous=buttons;++g->tick;
  if(g->screen==0) {
    if(edge&(FC_LEFT|FC_RIGHT|FC_UP|FC_DOWN)) {
      g->selected=(g->selected+((edge&(FC_RIGHT|FC_DOWN))?1:3))%FC_ROSTER_COUNT;
      fc_audio_play(FC_SND_SELECT);
    }
    if(edge&(FC_JUMP|FC_START|FC_PUNCH)) { begin(g);g->fight.previous=buttons;fc_audio_play(FC_SND_SELECT); }
    if(edge&FC_BACK)g->quit=true;
    return;
  }
  if(g->screen==2) {
    if(edge&(FC_UP|FC_DOWN))g->pause_choice=(g->pause_choice+((edge&FC_DOWN)?1:2))%3;
    if(edge&FC_BACK)g->screen=1;
    if(edge&(FC_JUMP|FC_START|FC_PUNCH)) {
      if(g->pause_choice==2)g->quit=true;
      else if(g->pause_choice==1)g->screen=0;
      else g->screen=1;
      g->fight.previous=buttons;
    }
    return;
  }
  if(edge&(FC_START|FC_BACK)) {
    if(g->fight.hp<=0 || g->finish)begin(g);
    else {g->screen=2;g->pause_choice=0;}
    return;
  }
  if(g->finish) {++g->finish;return;}
  FcTarget targets[ENEMIES];
  for(int i=0;i<ENEMIES;i++){targets[i]=g->enemy[i].target;targets[i].hit=false;}
  int horizontal=!!(buttons&FC_RIGHT)-!!(buttons&FC_LEFT);
  if(horizontal && !fc_busy(&g->fight) && !(buttons&FC_GUARD))g->facing=horizontal;
  FcBody body={{g->x,g->y,16,32},g->facing,g->grounded};
  fc_tick(&g->fight,buttons,body,targets,ENEMIES);
  if(g->fight.frozen){fc_audio_play(g->fight.events);return;}
  for(int i=0;i<ENEMIES;i++) {
    Enemy *e=&g->enemy[i];e->target=targets[i];
    if(e->target.hit){e->vx=e->target.knock_x;e->vy=e->target.knock_y;e->stun=22;}
  }
  if(g->fight.override_x)g->vx=g->fight.velocity_x;
  else {
    float desired=(g->fight.hp>0 && !g->fight.guarding)?horizontal*fc_roster[g->fight.character].speed:0;
    g->vx+=(desired-g->vx)*(g->grounded?0.35f:0.14f);
  }
  if(g->fight.override_y)g->vy=g->fight.velocity_y;
  if(g->grounded)g->coyote=6;else if(g->coyote)g->coyote--;
  if(edge&FC_JUMP)g->jump_buffer=6;else if(g->jump_buffer)g->jump_buffer--;
  if(g->jump_buffer && g->coyote && !fc_busy(&g->fight)) {
    g->vy=-6.4f;g->coyote=0;g->jump_buffer=0;g->grounded=false;
  }
  if(!(buttons&FC_JUMP) && g->vy<-3 && g->fight.move==FC_IDLE && !g->fight.stun)g->vy=-3;
  float previous_feet=g->y+32;
  g->x+=g->vx;if(g->x<8)g->x=8;if(g->x>LEVEL_END)g->x=LEVEL_END;
  g->vy+=0.28f;if(g->vy>7)g->vy=7;g->y+=g->vy;g->grounded=false;
  if(g->vy>=0) {
    float floor=10000;
    if(ground_at(g->x+8) && previous_feet<=GROUND+1)floor=GROUND;
    for(size_t i=0;i<sizeof(platforms)/sizeof(platforms[0]);i++) {
      const Platform *p=&platforms[i];
      if(g->x+16>p->x && g->x<p->x+p->w && previous_feet<=p->y+1 && p->y<floor)floor=p->y;
    }
    if(g->y+32>=floor){g->y=floor-32;g->vy=0;g->grounded=true;}
  }
  if(g->y>HEIGHT+32)fc_ko(&g->fight);
  for(int i=0;i<ENEMIES;i++) {
    Enemy *e=&g->enemy[i];if(!e->target.active)continue;
    if(e->stun)e->stun--;
    if(e->target.hp>0 && !e->stun) {
      float distance=g->x-e->target.box.x;
      e->vx=fabsf(distance)<190?(distance>0?0.48f:-0.48f):0;
    }
    e->target.box.x+=e->vx;e->target.box.y+=e->vy;e->vy+=0.25f;
    if(e->target.hp>0 && e->target.box.y+18>=GROUND && ground_at(e->target.box.x+8)) {
      e->target.box.y=GROUND-18;e->vy=0;
    }
    if(e->target.box.y>HEIGHT+50){e->target.active=false;e->target.hp=0;}
    FcBox player={g->x,g->y,16,32};
    if(e->target.hp>0 && !e->stun && fc_overlap(player,e->target.box) && g->fight.hp>0) {
      if(previous_feet<=e->target.box.y+6 && g->vy>0.3f) {
        e->target.hp=0;e->vy=-4;e->vx=2*g->facing;g->vy=-4.5f;
        fc_meter(&g->fight,15);g->fight.events|=FC_SND_HIT;
      } else fc_hurt(&g->fight,e->kind?18:12,e->target.box.x>g->x?1:-1);
    }
  }
  fc_audio_play(g->fight.events);
  float target=g->x-WIDTH*0.35f;if(target<0)target=0;if(target>LEVEL_END-WIDTH+80)target=LEVEL_END-WIDTH+80;
  g->camera+=(target-g->camera)*0.12f;
  if(g->x>=LEVEL_END-30 && g->fight.hp>0 && !remaining(g))g->finish=1;
}
static void hill(FcCanvas c,int cx,int top,int width,uint32_t color) {
  for(int y=top;y<GROUND;y++) {
    int half=width*(y-top)/(GROUND-top)/2;
    fc_rect(c,cx-half,y,half*2+1,1,color);
  }
}
static void render(Dojo *g,FcCanvas c) {
  if(g->screen==0){fc_selection(c,g->selected,g->tick,false);return;}
  for(int y=0;y<HEIGHT;y++) {
    uint32_t color=y<90?0x293e5b:y<135?0x486675:0x709087;
    fc_rect(c,0,y,WIDTH,1,color);
  }
  fc_rect(c,273-(int)(g->camera*0.04f)%40,49,26,26,0xf5c78a);
  for(int i=-1;i<7;i++) {
    int x=i*94-(int)(g->camera*0.18f)%94;
    hill(c,x,72+(i&1)*20,136,0x385464);
    hill(c,x+42,112+(i&1)*9,98,0x3c6b68);
    fc_rect(c,x+22,63,33,4,0xa0b5ac);fc_rect(c,x+28,59,21,4,0xa0b5ac);
  }
  for(int x=0;x<WIDTH;x++) {
    int wx=(int)g->camera+x;
    if(!ground_at((float)wx))continue;
    fc_rect(c,x,GROUND,1,HEIGHT-GROUND,0x70513e);
    fc_rect(c,x,GROUND,1,3,0xb9cf74);fc_rect(c,x,GROUND+3,1,5,0x577e57);
    if(wx%16<2)fc_rect(c,x,GROUND+12+(wx/16%2)*4,2,2,0xa27a4d);
  }
  for(size_t i=0;i<sizeof(platforms)/sizeof(platforms[0]);i++) {
    const Platform *p=&platforms[i];int x=(int)(p->x-g->camera);
    fc_rect(c,x,(int)p->y,(int)p->w,4,0xd9c580);fc_rect(c,x,(int)p->y+4,(int)p->w,5,0x886448);
    for(int j=3;j<p->w;j+=16)fc_rect(c,x+j,(int)p->y+5,2,3,0x4d4a44);
  }
  for(int i=0;i<ENEMIES;i++) {
    Enemy *e=&g->enemy[i];if(!e->target.active)continue;
    int x=(int)(e->target.box.x-g->camera),y=(int)e->target.box.y;
    uint32_t body=e->kind?0x7fb872:0xcf9060;
    fc_rect(c,x+2,y,13,3,0x293441);fc_rect(c,x,y+3,17,10,0x293441);
    fc_rect(c,x+2,y+3,13,8,body);fc_rect(c,x-1,y+8,19,5,body);
    fc_rect(c,x+3,y+8,4,5,0xf5e9c7);fc_rect(c,x+10,y+8,4,5,0xf5e9c7);
    fc_rect(c,x+5,y+9,2,3,0x1a2434);fc_rect(c,x+10,y+9,2,3,0x1a2434);
    fc_rect(c,x+2,y+15,6,3,0x352f32);fc_rect(c,x+10,y+15,6,3,0x352f32);
    if(e->kind){fc_rect(c,x+4,y+1,9,2,0xc5dd98);}
  }
  int gate=(int)(LEVEL_END-20-g->camera);
  fc_rect(c,gate,91,4,85,0xd9d8b3);fc_rect(c,gate+4,95,26,18,0x66c2bd);
  fc_text(c,gate+7,101,"GO",0x173346,1);
  FcFight *f=&g->fight;
  if(f->move==FC_SUPER_MOVE && !f->flash) {
    for(int i=3;i>0;i--)fc_fighter(c,f,(int)(g->x+8-g->camera)-f->facing*i*9,(int)g->y+32,1,0x456b9b);
  }
  if(g->y<HEIGHT)fc_fighter(c,f,(int)(g->x+8-g->camera),(int)g->y+32,1,0);
  fc_effects(c,f,g->camera,0);fc_hud(c,f);
  char status[48];snprintf(status,sizeof(status),"DOJO 01 / %02d LEFT",remaining(g));
  fc_text(c,WIDTH-124,12,status,0xf3e6c9,1);
  fc_text(c,WIDTH-112,25,"START PAUSE",0xb3cbcb,1);
  if(g->tick<420)fc_text(c,164,HEIGHT-16,"A JUMP X HIT Y MOVE RB SUPER",0xe0d9bb,1);
  if(g->finish) {
    fc_dim(c,110);fc_center(c,78,"YOU WIN",0xffd58a,3);
    fc_center(c,109,"DINOSAUR LAND IS NEXT",0xb4e8e8,1);
    fc_center(c,135,"START TO PLAY AGAIN",0xf0e8ce,1);
  }
  if(g->screen==2)fc_pause(c,g->pause_choice);
}
static SDL_GameController *controller;
static const SDL_GameControllerButton physical_buttons[]={
  SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
  SDL_CONTROLLER_BUTTON_DPAD_UP,SDL_CONTROLLER_BUTTON_DPAD_DOWN,
  SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_X,SDL_CONTROLLER_BUTTON_B,
  SDL_CONTROLLER_BUTTON_Y,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
  SDL_CONTROLLER_BUTTON_LEFTSHOULDER,SDL_CONTROLLER_BUTTON_START,SDL_CONTROLLER_BUTTON_BACK
};
static uint32_t key_button(SDL_Scancode key) {
  static const SDL_Scancode keys[]={SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT,
    SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_Z,SDL_SCANCODE_X,
    SDL_SCANCODE_C,SDL_SCANCODE_A,SDL_SCANCODE_S,SDL_SCANCODE_D,
    SDL_SCANCODE_RETURN,SDL_SCANCODE_ESCAPE};
  if(key==SDL_SCANCODE_SPACE)return FC_JUMP;
  for(int i=0;i<12;i++)if(keys[i]==key)return 1u<<i;
  return 0;
}
static void open_controller(void) {
  if(controller && !SDL_GameControllerGetAttached(controller)){SDL_GameControllerClose(controller);controller=NULL;}
  if(!controller)for(int i=0;i<SDL_NumJoysticks();i++)if(SDL_IsGameController(i)) {
    controller=SDL_GameControllerOpen(i);if(controller)break;
  }
}
static uint32_t buttons(void) {
  const Uint8 *k=SDL_GetKeyboardState(NULL);uint32_t b=0;
  if(k[SDL_SCANCODE_LEFT])b|=FC_LEFT;if(k[SDL_SCANCODE_RIGHT])b|=FC_RIGHT;
  if(k[SDL_SCANCODE_UP])b|=FC_UP;if(k[SDL_SCANCODE_DOWN])b|=FC_DOWN;
  if(k[SDL_SCANCODE_Z] || k[SDL_SCANCODE_SPACE])b|=FC_JUMP;
  if(k[SDL_SCANCODE_X])b|=FC_PUNCH;if(k[SDL_SCANCODE_C])b|=FC_KICK;
  if(k[SDL_SCANCODE_A])b|=FC_SPECIAL;if(k[SDL_SCANCODE_S])b|=FC_SUPER;
  if(k[SDL_SCANCODE_D])b|=FC_GUARD;
  if(k[SDL_SCANCODE_RETURN])b|=FC_START;if(k[SDL_SCANCODE_ESCAPE])b|=FC_BACK;
  if(controller) {
    for(int i=0;i<12;i++)if(SDL_GameControllerGetButton(controller,physical_buttons[i]))b|=1u<<i;
    int x=SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX),y=SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY);
    if(x>12000)b|=FC_RIGHT;if(x<-12000)b|=FC_LEFT;
    if(y>12000)b|=FC_DOWN;if(y<-12000)b|=FC_UP;
  }
  if((b&(FC_LEFT|FC_RIGHT))==(FC_LEFT|FC_RIGHT))b&=~(FC_LEFT|FC_RIGHT);
  if((b&(FC_UP|FC_DOWN))==(FC_UP|FC_DOWN))b&=~(FC_UP|FC_DOWN);
  return b;
}
static int capture(Dojo *g,FcCanvas c,const char *dir) {
  const char *names[]={"select","fight","super","ko"};
  for(int i=0;i<4;i++) {
    if(i==1){begin(g);g->x=174;g->tick=440;g->fight.tick=30;}
    if(i==2){g->fight.meter=300;g->fight.move=FC_SUPER_MOVE;g->fight.flash=12;}
    if(i==3){g->fight.flash=0;fc_ko(&g->fight);}
    render(g,c);char path[4096];snprintf(path,sizeof(path),"%s/%s.bmp",dir,names[i]);
    SDL_Surface *s=SDL_CreateRGBSurfaceWithFormatFrom(c.pixels,WIDTH,HEIGHT,32,WIDTH*4,SDL_PIXELFORMAT_ARGB8888);
    if(!s)return 1;int result=SDL_SaveBMP(s,path);SDL_FreeSurface(s);if(result)return 1;
  }
  return 0;
}
int main(int argc,char **argv) {
  int smoke=0;bool fullscreen=false;const char *capture_dir=NULL;
  for(int i=1;i<argc;i++) {
    if(!strcmp(argv[i],"--fullscreen"))fullscreen=true;
    else if(!strcmp(argv[i],"--smoke") && i+1<argc)smoke=atoi(argv[++i]);
    else if(!strcmp(argv[i],"--capture") && i+1<argc)capture_dir=argv[++i];
    else {printf("Usage: %s [--fullscreen] [--smoke FRAMES] [--capture DIRECTORY]\n",argv[0]);return strcmp(argv[i],"--help")!=0;}
  }
  if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER)!=0){fprintf(stderr,"%s\n",SDL_GetError());return 1;}
  if(smoke || capture_dir)SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"nearest");
  SDL_Window *window=SDL_CreateWindow("Super Mario World II - Dojo",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
    WIDTH*3,HEIGHT*3,SDL_WINDOW_RESIZABLE|(fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0)|((smoke||capture_dir)?SDL_WINDOW_HIDDEN:0));
  if(!window){fprintf(stderr,"Window: %s\n",SDL_GetError());SDL_Quit();return 1;}
  SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
  if(!renderer)renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
  if(!renderer){fprintf(stderr,"Renderer: %s\n",SDL_GetError());SDL_DestroyWindow(window);SDL_Quit();return 1;}
  SDL_RenderSetLogicalSize(renderer,WIDTH,HEIGHT);
  SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,WIDTH,HEIGHT);
  uint32_t pixels[WIDTH*HEIGHT];FcCanvas canvas={pixels,WIDTH,HEIGHT,WIDTH};Dojo game={0};
  if(!texture){SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 1;}
  const char *sprites=getenv("WW_SPRITES");
  fc_assets_load(sprites?sprites:"local/sf2/sprites.dat");
  const char *audio=getenv("WW_AUDIO_DIR");
  if(!capture_dir)fc_audio_init(audio?audio:fc_assets_ready()?"local/sf2/audio":NULL);
  open_controller();int result=0;
  if(capture_dir)result=capture(&game,canvas,capture_dir);
  else {
    double previous=(double)SDL_GetPerformanceCounter()/SDL_GetPerformanceFrequency(),accumulator=0;
    unsigned frames=0;uint32_t pending=0;
    while(!game.quit) {
      SDL_Event event;
      while(SDL_PollEvent(&event)) {
        if(event.type==SDL_QUIT)game.quit=true;
        if(event.type==SDL_CONTROLLERDEVICEADDED || event.type==SDL_CONTROLLERDEVICEREMOVED)open_controller();
        /* Preserve taps whose down and up events land between simulation ticks. */
        if(event.type==SDL_KEYDOWN && !event.key.repeat)pending|=key_button(event.key.keysym.scancode);
        if(event.type==SDL_CONTROLLERBUTTONDOWN && controller &&
           event.cbutton.which==SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
          for(int i=0;i<12;i++)if(physical_buttons[i]==event.cbutton.button)pending|=1u<<i;
        }
        if(event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_FOCUS_LOST && game.screen==1)game.screen=2;
        if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_F11) {
          fullscreen=!fullscreen;SDL_SetWindowFullscreen(window,fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);
        }
      }
      double now=(double)SDL_GetPerformanceCounter()/SDL_GetPerformanceFrequency();
      double dt=now-previous;previous=now;if(dt>0.1)dt=0.1;accumulator+=dt;
      uint32_t b=buttons()|pending;
      if(smoke) {
        b=frames==1?FC_START:frames>20?FC_RIGHT:0;
        if(frames%50==22)b|=FC_SPECIAL;if(frames==80)b|=FC_SUPER;
        if(frames%95>72)b|=FC_JUMP;
        accumulator=1.0/60.0;
      }
      while(accumulator>=1.0/60.0){tick(&game,b);pending=0;accumulator-=1.0/60.0;}
      render(&game,canvas);SDL_UpdateTexture(texture,NULL,pixels,WIDTH*4);
      SDL_SetRenderDrawColor(renderer,10,15,25,255);SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer,texture,NULL,NULL);SDL_RenderPresent(renderer);
      if(smoke && ++frames>=(unsigned)smoke)break;
      if(!smoke)SDL_Delay(1);
    }
    if(smoke)printf("Dojo smoke: %u frames, hp=%d, enemies=%d, meter=%d\n",frames,game.fight.hp,remaining(&game),game.fight.meter);
  }
  if(controller)SDL_GameControllerClose(controller);
  fc_assets_free();fc_audio_shutdown();SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return result;
}
