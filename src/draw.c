#include "draw.h"
#include "assets.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Small original 5x7 bitmap alphabet. Each integer is one row. */
static const uint8_t letters[36][7]={
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{31,4,4,4,4,4,31},
 {7,2,2,2,2,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14}
};
void fc_rect(FcCanvas c,int x,int y,int w,int h,uint32_t color) {
  int x2=x+w,y2=y+h;if(x<0)x=0;if(y<0)y=0;
  if(x2>c.w)x2=c.w;if(y2>c.h)y2=c.h;
  for(int j=y;j<y2;j++)for(int i=x;i<x2;i++)c.pixels[j*c.pitch+i]=0xff000000|color;
}
static void frame(FcCanvas c,int x,int y,int w,int h,uint32_t color) {
  fc_rect(c,x,y,w,1,color);fc_rect(c,x,y+h-1,w,1,color);
  fc_rect(c,x,y,1,h,color);fc_rect(c,x+w-1,y,1,h,color);
}
void fc_text(FcCanvas c,int x,int y,const char *s,uint32_t color,int scale) {
  for(;*s;s++,x+=6*scale) {
    int ch=toupper((unsigned char)*s),idx=ch>='A'&&ch<='Z'?ch-'A':ch>='0'&&ch<='9'?ch-'0'+26:-1;
    if(idx>=0) {
      for(int row=0;row<7;row++)for(int col=0;col<5;col++)
        if(letters[idx][row]&(1<<(4-col)))fc_rect(c,x+col*scale,y+row*scale,scale,scale,color);
    } else if(ch=='-' || ch=='+' || ch=='=') {
      fc_rect(c,x,y+3*scale,5*scale,scale,color);
      if(ch=='+')fc_rect(c,x+2*scale,y+scale,scale,5*scale,color);
      if(ch=='=')fc_rect(c,x,y+5*scale,5*scale,scale,color);
    } else if(ch=='.' || ch==':') {
      fc_rect(c,x+2*scale,y+6*scale,scale,scale,color);
      if(ch==':')fc_rect(c,x+2*scale,y+2*scale,scale,scale,color);
    } else if(ch=='/' || ch=='>') {
      for(int j=0;j<5;j++)fc_rect(c,x+(ch=='/'?4-j:(j<3?j:4-j))*scale,y+(j+1)*scale,scale,scale,color);
    } else if(ch=='!') {fc_rect(c,x+2*scale,y,scale,4*scale,color);fc_rect(c,x+2*scale,y+6*scale,scale,scale,color);}
    else if(ch=='?') {
      static const uint8_t rows[]={14,17,1,2,4,0,4};
      for(int row=0;row<7;row++)for(int col=0;col<5;col++)
        if(rows[row]&(1<<(4-col)))fc_rect(c,x+col*scale,y+row*scale,scale,scale,color);
    }
  }
}
void fc_center(FcCanvas c,int y,const char *s,uint32_t color,int scale) {
  fc_text(c,(c.w-(int)strlen(s)*6*scale+scale)/2,y,s,color,scale);
}
void fc_dim(FcCanvas c,int amount) {
  for(int y=0;y<c.h;y++)for(int x=0;x<c.w;x++) {
    uint32_t p=c.pixels[y*c.pitch+x];
    c.pixels[y*c.pitch+x]=0xff000000|((((p>>16)&255)*(unsigned)amount/256)<<16)|
      ((((p>>8)&255)*(unsigned)amount/256)<<8)|((p&255)*(unsigned)amount/256);
  }
}
void fc_fighter(FcCanvas c,const FcFight *f,int x,int feet,int scale,uint32_t tint) {
  int pose=FC_POSE_IDLE;unsigned tick=f->tick;
  if(f->hp<=0){pose=FC_POSE_KO;tick=(unsigned)f->ko_timer;}
  else if(f->stun){pose=FC_POSE_HURT;tick=(unsigned)(22-f->stun);}
  else if(f->victory){pose=FC_POSE_WIN;tick=f->move_tick;}
  else if(f->guarding)pose=FC_POSE_GUARD;
  else if(f->move!=FC_IDLE) {
    static const int poses[]={FC_POSE_IDLE,FC_POSE_PUNCH,FC_POSE_KICK,FC_POSE_FIRE,
      FC_POSE_RISE,FC_POSE_SPIN,FC_POSE_RUSH,FC_POSE_FLURRY,FC_POSE_FIRE,
      FC_POSE_THROW,FC_POSE_TELEPORT,FC_POSE_FLURRY,FC_POSE_FLURRY,FC_POSE_DIVE};
    pose=poses[f->move];tick=(unsigned)f->move_tick;
    if(!f->body.grounded && f->move==FC_JAB)pose=FC_POSE_AIR_PUNCH;
    if(!f->body.grounded && f->move==FC_ROUNDHOUSE)pose=FC_POSE_AIR_KICK;
    if(f->body.grounded && f->crouching_attack) {
      if(f->move==FC_JAB)pose=FC_POSE_CROUCH_PUNCH;
      if(f->move==FC_ROUNDHOUSE)pose=FC_POSE_CROUCH_KICK;
    }
    if(f->move==FC_SUPER_MOVE) {
      static const int super_pose[]={FC_POSE_FIRE,FC_POSE_RUSH,FC_POSE_FLURRY,FC_POSE_FIRE,
        FC_POSE_RISE,FC_POSE_RUSH,FC_POSE_SPIN,FC_POSE_FLURRY,FC_POSE_RUSH,FC_POSE_RUSH,FC_POSE_RISE,FC_POSE_RUSH};
      pose=super_pose[f->character];
      if(f->character==FC_BISON && f->move_tick>8)tick=12;
    }
  } else if(!f->body.grounded){pose=FC_POSE_JUMP;tick=f->air_tick;}
  else if(f->previous&FC_DOWN)pose=FC_POSE_DUCK;
  else if(f->moving){pose=FC_POSE_WALK;tick=(unsigned)(f->walk_distance*0.65f);}
  if(f->invuln && !f->stun && f->move==FC_IDLE && f->hp>0 && ((f->tick/4)%2))return;
  if(fc_psycho_flight(f) && !f->flash) {
    /* The level-three joke from Psycho Crusher, FTW: a blue-white comet
     * whose wake stretches offscreen, with the original Alpha spin at its tip. */
    int length=f->move_tick*12;if(length>c.w)length=c.w;
    int cy=feet-26,pulse=(f->tick/4)%3;
    for(int dy=-30;dy<=30;dy++) {
      int a=dy<0?-dy:dy;if(a>28+pulse)continue;
      uint32_t color=a>23?0x6757d9:a>18?0x439bff:a>15?0x8af6ff:0xf0ffff;
      int nose=20-(a*a)/40;
      int left=f->facing>0?x-length:x-nose;
      fc_rect(c,left,cy+dy,length+nose,1,color);
    }
    fc_sprite(c,0,FC_POSE_PSYCHO,f->tick,x,cy,f->facing,scale,2,tint);
    return;
  }
  if(f->character==FC_BISON && f->move==FC_RUSH && tick>8)tick=12;
  if(f->move==FC_TELEPORT && f->move_tick>5 && f->move_tick<19)
    tint=(f->tick/3)%2?0x9b6ed3:0x413063;
  fc_sprite(c,f->character,pose,tick,x,feet,f->facing,scale,2,tint);
}
void fc_effects(FcCanvas c,const FcFight *f,float camx,float camy) {
  if(f->flash) {
    fc_dim(c,70);
    int x=(int)(f->body.box.x+f->body.box.w/2-camx),y=(int)(f->body.box.y+f->body.box.h-camy);
    fc_fighter(c,f,x,y,1,0);
    fc_sprite(c,0,FC_POSE_SUPER_CHARGE,(18-f->flash)*2,x,y-24,f->facing,1,2,0);
  }
  for(int i=0;i<FC_MAX_SHOTS;i++)if(f->shots[i].ttl) {
    const FcShot *p=&f->shots[i];int x=(int)(p->box.x-camx),y=(int)(p->box.y-camy);
    int w=(int)p->box.w,h=(int)p->box.h;
    fc_sprite(c,f->character,FC_POSE_SHOT,f->tick,x+w/2,y+h/2,p->vx>0?1:-1,p->super?2:1,2,0);
  }
  for(int i=0;i<FC_MAX_SPARKS;i++)if(f->sparks[i].ttl) {
    const FcSpark *s=&f->sparks[i];
    fc_rect(c,(int)(s->x-camx),(int)(s->y-camy),s->ttl>12?3:2,2,s->color);
  }
  for(int i=0;i<FC_MAX_BLASTS;i++)if(f->blasts[i].ttl) {
    const FcBlast *b=&f->blasts[i];int age=24-b->ttl;
    int r=(b->large?10:5)+age/2,x=(int)(b->x-camx),y=(int)(b->y-camy);
    uint32_t color=age<6?0xffffdd:age<13?0xffd22b:0xff6720;
    if(age<7) {
      fc_rect(c,x-r,y-r/2,r*2,r,color);fc_rect(c,x-r/2,y-r,r,r*2,color);
      fc_rect(c,x-r/3,y-r/3,r*2/3,r*2/3,0xffffff);
    } else {
      fc_rect(c,x-r/2,y-r,r,2,color);fc_rect(c,x-r/2,y+r,r,2,color);
      fc_rect(c,x-r,y-r/2,2,r,color);fc_rect(c,x+r,y-r/2,2,r,color);
      for(int dx=-1;dx<=1;dx+=2)for(int dy=-1;dy<=1;dy+=2)
        fc_rect(c,x+dx*(r*3/4),y+dy*(r*3/4),3,3,color);
    }
  }
}
/* Original SF2 letter art, scaled to the SNES 256-pixel playfield. */
static void arcade_text(FcCanvas c,int x,int y,const char *s,bool big) {
  for(;*s;s++) {
    int ch=toupper((unsigned char)*s),i=ch>='A'&&ch<='Z'?ch-'A':ch>='0'&&ch<='9'?ch-'0'+26:-1;
    if(i>=0 && fc_assets_ready()) {
      for(int dx=-1;dx<=1;dx++)for(int dy=-1;dy<=1;dy++)if(dx || dy)
        fc_sprite(c,0,big?FC_POSE_BIG_FONT:FC_POSE_FONT,(unsigned)i,x+dx,y+dy,1,big?3:2,big?4:3,0x010101);
      fc_sprite(c,0,big?FC_POSE_BIG_FONT:FC_POSE_FONT,(unsigned)i,x,y,1,big?3:2,big?4:3,0);
    }
    else {char glyph[2]={(char)ch,0};fc_text(c,x,y+1,glyph,0xffdd65,1);}
    x+=big?(ch=='W'?19:13):8;
  }
}
static void arcade_center(FcCanvas c,int y,const char *s,bool big) {
  int width=0;for(const char *p=s;*p;p++)width+=big?(*p=='W'?19:13):8;
  arcade_text(c,(c.w-width)/2,y,s,big);
}
void fc_title(FcCanvas c,unsigned tick,unsigned high_score) {
  /* Exact SF2 title art beside the original SMW lettering. */
  fc_sprite(c,0,FC_POSE_TITLE_II,0,192,36,1,2,3,0);
  fc_sprite(c,0,FC_POSE_TITLE_SUBTITLE,0,32,111,1,4,5,0);
  if(tick%90<65)arcade_center(c,139,"PRESS START",false);
  char text[32];snprintf(text,sizeof(text),"HI SCORE %07u",high_score);
  arcade_center(c,19,text,false);
}
static void hud_text(FcCanvas c,int x,int y,const char *s) {
  for(;*s;s++,x+=8) {
    int ch=toupper((unsigned char)*s),i=ch>='A'&&ch<='Z'?ch-'A':ch>='0'&&ch<='9'?ch-'0'+26:-1;
    if(i>=0 && fc_assets_ready()) {
      for(int dx=-1;dx<=1;dx++)for(int dy=-1;dy<=1;dy++)if(dx || dy)
        fc_sprite(c,0,FC_POSE_HUD_FONT,(unsigned)i,x+dx,y+dy,1,1,1,0x010101);
      fc_sprite(c,0,FC_POSE_HUD_FONT,(unsigned)i,x,y,1,1,1,0);
    } else {char glyph[2]={(char)ch,0};fc_text(c,x,y,glyph,0xffeead,1);}
  }
}
static void hud_icon(FcCanvas c,int x,int y,FcCanvas icon) {
  if(!icon.pixels)return;
  for(int row=0;row<icon.h;row++)for(int col=0;col<icon.w;col++) {
    uint32_t p=icon.pixels[row*icon.pitch+col];
    if(p>>24)fc_rect(c,x+col,y+row,1,1,p);
  }
}
static void life_bar(FcCanvas c,int x,int y,int w,int hp,int maximum,bool reverse,bool loss) {
  if(maximum<1)maximum=1;if(hp<0)hp=0;if(hp>maximum)hp=maximum;
  fc_rect(c,x-2,y-2,w+4,11,0x080808);frame(c,x-1,y-1,w+2,9,0xfffbe1);
  for(int row=0;row<7;row++)for(int col=0;col<w;col++)
    fc_rect(c,x+col,y+row,1,1,loss?(((row+col)&1)?0xec2828:0x40587e):0x15182b);
  int filled=w*hp/maximum,left=reverse?x+w-filled:x;
  fc_rect(c,left,y,filled,7,0xffed00);fc_rect(c,left,y,filled,1,0xffffa3);
  fc_rect(c,left,y+6,filled,1,0xe6b400);
}
void fc_hud(FcCanvas c,const FcFight *f) { fc_battle_hud(c,f,NULL,NULL); }
void fc_battle_hud(FcCanvas c,const FcFight *f,const FcOpponent *o,const FcStageHud *s) {
  char text[64];const FcCharacter *r=&fc_roster[f->character];
  int mid=c.w/2,w=mid-23;bool boss=o && o->active;
  if(s) {
    snprintf(text,sizeof(text),"1P %07u",s->score);hud_text(c,7,3,text);
    snprintf(text,sizeof(text),"HI %07u",s->high_score);hud_text(c,c.w-87,3,text);
  }
  life_bar(c,8,17,w,f->hp,r->vitality,true,true);
  if(boss) {
    life_bar(c,c.w-8-w,17,w,o->hp,o->max_hp,false,true);
    hud_text(c,c.w-35-(int)strlen(o->name)*8,29,o->name);
  } else if(s) {
    /* The opponent's side becomes the native 100-coin extra-life gauge. */
    life_bar(c,c.w-8-w,17,w,s->coins,100,false,false);
    hud_text(c,c.w-99,29,"NEXT 1UP");
  }
  fc_sprite(c,0,FC_POSE_KO_MARK,0,mid-14,13,1,3,4,0);
  frame(c,6,28,22,28,0x080808);frame(c,5,27,24,30,0xefedd5);
  fc_sprite(c,f->character,FC_POSE_HUD_PORTRAIT,0,7,29,1,1,1,0);
  hud_text(c,33,29,r->name);
  if(s) {
    snprintf(text,sizeof(text),"X%02d",s->lives);hud_text(c,33,42,text);
    for(int i=0;i<5;i++) {
      int x=62+i*8;
      fc_sprite(c,0,FC_POSE_ROUND_MARK,0,x,42,1,1,3,0x080808);
      fc_sprite(c,0,FC_POSE_ROUND_MARK,0,x,41,1,1,3,i<s->dragon_coins?0:0x777b82);
    }
    if(s->timed) {
      snprintf(text,sizeof(text),"%03d",s->time);arcade_center(c,29,text,true);
    } else {
      /* Bowser/Akuma freezes SMW's clock. Draw infinity, not a fake timer. */
      static const uint8_t infinity[]={0x66,0xff,0x99,0xff,0x66};
      for(int y=0;y<5;y++)for(int x=0;x<8;x++)if(infinity[y]&(1<<(7-x)))
        fc_rect(c,mid-9+x*2,30+y*2,4,4,0x080808);
      for(int y=0;y<5;y++)for(int x=0;x<8;x++)if(infinity[y]&(1<<(7-x)))
        fc_rect(c,mid-8+x*2,31+y*2,2,2,0xffe76a);
    }
    hud_text(c,mid-16,44,"TIME");
    hud_icon(c,c.w-103,42,s->star_icon);
    snprintf(text,sizeof(text),"%02d",s->bonus_stars);hud_text(c,c.w-93,42,text);
    hud_icon(c,c.w-65,42,s->coin_icon);
    snprintf(text,sizeof(text),"%02d",s->coins);hud_text(c,c.w-55,42,text);
    frame(c,c.w-30,28,22,22,0x080808);frame(c,c.w-29,29,20,20,0xefedd5);
    hud_icon(c,c.w-27,31,s->reserve_icon);
  }
  /* A slim arcade gauge with slanted ends, one continuous fill and SUPER
   * lettering over the bezel. Small ticks mark the three Alpha-style levels. */
  int gy=c.h-14,fill=120*f->meter/300;
  for(int row=0;row<11;row++) {
    int slant=(10-row)/2,left=8+slant;
    fc_rect(c,left,gy+row,126,1,0x080810);
    if(row>0 && row<10)fc_rect(c,left+1,gy+row,124,1,row<3?0xfff6be:0x8c8967);
    if(row>2 && row<8) {
      fc_rect(c,left+3,gy+row,120,1,0x15182b);
      uint32_t blue=row==3?0xc4ffff:row<6?0x219eff:0x2254c8;
      if(f->meter==300 && (f->tick/12)%2)blue=row==3?0xffffff:0xffd522;
      fc_rect(c,left+3,gy+row,fill,1,blue);
      for(int i=1;i<3;i++)fc_rect(c,left+3+i*40,gy+row,1,1,0x777c92);
    }
  }
  arcade_text(c,14,gy-6,"SUPER",false);
  snprintf(text,sizeof(text),"%d",f->meter/100);
  arcade_text(c,139,gy-3,text,true);
  if(f->meter==300)fc_text(c,158,gy+2,"MAX",0xffe366,1);
  if(f->combo>1 && !(s && s->clearing)) {
    snprintf(text,sizeof(text),"%d HIT COMBO",f->combo);hud_text(c,9,65,text);
    if(f->combo_award<0)snprintf(text,sizeof(text),"%dUP",-f->combo_award);
    else snprintf(text,sizeof(text),"%d",f->combo_award);
    hud_text(c,9,76,text);
  }
  if(f->hp<=0 || (boss && o->ko_timer>0)) {
    fc_dim(c,170);
    fc_sprite(c,0,FC_POSE_KO_MARK,0,c.w/2-65,c.h/2-32,1,3,1,0);
    fc_text(c,c.w/2+45,c.h/2-25,"!!",0xffe450,3);
    arcade_center(c,c.h/2+30,f->hp<=0?"YOU LOSE":"YOU WIN",true);
  }
}
static void menu_background(FcCanvas c) {
  fc_rect(c,0,0,c.w,c.h,0x080c51);
  for(int y=0;y<c.h;y+=4)fc_rect(c,0,y,c.w,2,0x101563);
  frame(c,4,4,c.w-8,c.h-8,0x3854a1);
}
void fc_selection(FcCanvas c,int choice,unsigned tick,bool compact) {
  (void)compact;menu_background(c);
  arcade_center(c,8,"PLAYER SELECT",true);
  fc_sprite(c,0,FC_POSE_MAP,0,(c.w-192)/2,29,1,1,1,0);
  int start=(c.w-240)/2;
  for(int i=0;i<FC_ROSTER_COUNT;i++) {
    int x=start+(i%6)*40,y=114+(i/6)*44;bool selected=i==choice;
    fc_rect(c,x,y,38,41,selected?0x2979c8:0x131d59);
    frame(c,x-1,y-1,40,43,selected?0xffff8a:0x617ccb);
    fc_sprite(c,i,FC_POSE_PORTRAIT,0,x+19,y+39,1,1,4,0);
    if(selected && (tick/20)%2==0)fc_text(c,x+2,y+2,"1P",0xffee22,1);
  }
  arcade_center(c,94,fc_roster[choice].name,false);
  fc_center(c,c.h-12,"DPAD SELECT  A / START CONFIRM",0xffffd5,1);
}
void fc_pause(FcCanvas c,int choice,bool boss_shortcut) {
  fc_dim(c,70);fc_rect(c,c.w/2-99,c.h/2-65,198,139,0x0c155c);
  frame(c,c.w/2-99,c.h/2-65,198,139,0xffd52b);
  arcade_center(c,c.h/2-52,"PAUSE",true);
  const char *items[]={"RESUME","PLAYER SELECT","AKUMA BATTLE","QUIT GAME"};
  for(int i=0;i<(boss_shortcut?4:3);i++) {
    if(choice==i)fc_rect(c,c.w/2-90,c.h/2-12+i*21,180,16,0x274ba1);
    arcade_center(c,c.h/2-9+i*21,items[!boss_shortcut && i==2?3:i],false);
    if(choice==i)fc_text(c,c.w/2-87,c.h/2-8+i*21,">",0xffff55,1);
  }
}
void fc_continue(FcCanvas c,int character,int ticks) {
  if(ticks<30)return;
  int seconds=9-(ticks-30)/60;if(seconds<0)seconds=0;
  fc_rect(c,0,0,c.w,c.h,0x000000);
  char label[24];snprintf(label,sizeof(label),"CONTINUE? %d",seconds);
  arcade_text(c,8,8,label,false);
  fc_sprite(c,character,FC_POSE_CONTINUE,0,76,154,1,3,4,0);
  /* Original glowing SF2 lamps arranged into the six-by-eight countdown. */
  static const uint8_t digits[10][8]={
    {30,51,51,51,51,51,51,30},{12,28,12,12,12,12,12,63},
    {30,51,3,6,12,24,48,63},{30,51,3,14,3,3,51,30},
    {6,14,30,54,54,63,6,6},{63,48,48,62,3,3,51,30},
    {14,24,48,62,51,51,51,30},{63,3,6,12,24,24,24,24},
    {30,51,51,30,51,51,51,30},{30,51,51,51,31,3,6,28}
  };
  for(int row=0;row<8;row++)for(int col=0;col<6;col++)if(digits[seconds][row]&(1<<(5-col)))
    fc_sprite(c,0,FC_POSE_COUNTDOWN_LAMP,0,164+col*10,53+row*10,1,1,6,0);
  arcade_center(c,181,"START TO CONTINUE",false);
  fc_center(c,204,"BACK TO WORLD MAP",0xaaaac4,1);
}
void fc_level_clear(FcCanvas c,int character,bool perfect) {
  /* The native heading is suppressed at its tile upload, leaving the
   * original scenery and time bonus behind the fighter's victory art. */
  fc_sprite(c,character,FC_POSE_WIN,40,54,99,1,1,3,0);
  arcade_center(c,54,perfect?"PERFECT":fc_roster[character].name,true);
  arcade_center(c,78,"COURSE CLEAR",false);
}
