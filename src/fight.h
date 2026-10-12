#ifndef WORLD_WARRIOR_FIGHT_H
#define WORLD_WARRIOR_FIGHT_H
#include <stdbool.h>
#include <stdint.h>

/* World coordinates are pixels; simulation always runs at 60 Hz. No SDL or
 * emulator state here: the dojo and the SMW adapter run the same combat. */
enum { FC_LEFT=1, FC_RIGHT=2, FC_UP=4, FC_DOWN=8, FC_JUMP=16,
  FC_PUNCH=32, FC_KICK=64, FC_SPECIAL=128, FC_SUPER=256,
  FC_GUARD=512, FC_START=1024, FC_BACK=2048 };
enum { FC_RYU, FC_HONDA, FC_CHUNLI, FC_GUILE, FC_KEN, FC_BLANKA,
  FC_ZANGIEF, FC_DHALSIM, FC_BALROG, FC_VEGA, FC_SAGAT, FC_BISON, FC_ROSTER_COUNT };
typedef enum { FC_IDLE, FC_JAB, FC_ROUNDHOUSE, FC_PROJECTILE,
  FC_RISING, FC_SPIN, FC_RUSH, FC_FLURRY, FC_SUPER_MOVE,
  FC_THROW, FC_TELEPORT, FC_ELECTRIC, FC_FLAME, FC_DIVE } FcMove;
enum { FC_SND_SWING=1, FC_SND_HIT=2, FC_SND_HURT=4, FC_SND_KO=8,
  FC_SND_SPECIAL=16, FC_SND_SUPER=32, FC_SND_BLOCK=64, FC_SND_SELECT=128,
  FC_SND_BOSS_KO=256, FC_SND_PERFECT=512, FC_SND_AKUMA_SUPER=1024, FC_SND_AKUMA_WARP=2048 };
#define FC_MAX_TARGETS 64
#define FC_MAX_SHOTS 12
#define FC_MAX_SPARKS 64
#define FC_MAX_BLASTS 24
typedef struct { float x,y,w,h; } FcBox;
typedef struct { FcBox box; int facing; bool grounded; } FcBody;
typedef struct {
  const char *name;
  int hp, max_hp, ko_timer;
  bool active;
} FcOpponent;
typedef struct {
  FcBox box;
  int hp;
  bool active, hit;
  float knock_x, knock_y;
} FcTarget;
typedef struct {
  const char *name, *style, *special, *secondary, *super_name;
  uint32_t color, cloth, skin;
  int vitality;
  float speed;
} FcCharacter;
extern const FcCharacter fc_roster[FC_ROSTER_COUNT];
typedef struct {
  FcBox box;
  float vx;
  int ttl, damage, pierce;
  bool super;
  uint64_t hit_mask;
  bool scenery_hit;
} FcShot;
typedef struct { float x,y,vx,vy; int ttl; uint32_t color; } FcSpark;
typedef struct { float x,y; int ttl; bool large; } FcBlast;
/* Keep the last six popups, but total every hit even if a super fills all
 * native score-sprite slots in one frame. Negative awards mean extra lives. */
typedef struct { float x,y; int award; } FcReward;
typedef struct { uint32_t tick; uint8_t direction; int8_t facing; } FcDirection;
typedef struct {
  int character, hp, meter, facing;
  FcMove move, buffered;
  int move_tick, super_level, hitstop, flash, stun, invuln;
  int combo, combo_timer, ko_timer, buffer_timer;
  int charge_left, charge_right, charge_down, left_grace, right_grace, down_grace;
  int mash_count, mash_timer, motion_count;
  bool guarding, move_hit, frozen;
  bool override_x, override_y;
  float velocity_x, velocity_y;
  uint64_t melee_mask;
  uint32_t tick, previous, events;
  FcDirection motion[32];
  FcShot shots[FC_MAX_SHOTS];
  FcSpark sparks[FC_MAX_SPARKS];
  FcBlast blasts[FC_MAX_BLASTS];
  FcReward rewards[6];
  int reward_count,score_gain,lives_gain,combo_award;
  FcBody body;
  float walk_distance, vertical_motion;
  int air_tick,fall_tick,walk_pause;
  uint32_t animation_tick;
  bool moving, body_valid, victory;
  bool crouching_attack, buffered_crouch,air_attack,jump_forward;
} FcFight;

void fc_init(FcFight *f, int character);
/* Clear transient actions on a room change, keeping vitality and meter. */
void fc_clear_actions(FcFight *f);
void fc_tick(FcFight *f, uint32_t buttons, FcBody body, FcTarget *targets, int count);
bool fc_hurt(FcFight *f, int damage, int from_direction);
void fc_ko(FcFight *f);
void fc_meter(FcFight *f, int amount);
void fc_explode(FcFight *f, float x, float y, bool large);
bool fc_overlap(FcBox a, FcBox b);
bool fc_motion(const FcFight *f, const uint8_t *sequence, int count, int window);
bool fc_busy(const FcFight *f);
bool fc_psycho_flight(const FcFight *f);
const char *fc_move_name(const FcFight *f);
#endif
