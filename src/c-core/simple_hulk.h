#ifndef SIMPLE_HULK_H
#define SIMPLE_HULK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SH_ROWS 25
#define SH_COLS 31
#define SH_SCENARIOS 9
#define SH_MAX_ENTITIES 96
#define SH_VOID ' '
#define SH_CLOSED_DOOR '='
#define SH_OPEN_DOOR '/'

typedef struct SHGame SHGame;
typedef enum { SH_MARINE, SH_ALIEN, SH_BLIP } SHTeam;
typedef enum { SH_BASIC, SH_SKITTER, SH_BRUTE, SH_SPITTER } SHBug;
typedef enum { SH_RIFLE, SH_SCATTERGUN, SH_CANNON, SH_FLAME } SHWeapon;
typedef enum { SH_NORTH, SH_EAST, SH_SOUTH, SH_WEST } SHFacing;
typedef enum { SH_MARINE_PHASE, SH_DEPLOYMENT, SH_ALIEN_PHASE, SH_FINISHED } SHPhase;
typedef enum { SH_ONGOING, SH_MARINES_WIN, SH_ALIENS_WIN } SHOutcome;
typedef enum {
    SH_OK, SH_INVALID, SH_WRONG_PHASE, SH_NO_AP, SH_BLOCKED,
    SH_NOT_VISIBLE, SH_NO_AMMO, SH_JAMMED, SH_REACTION_PENDING, SH_GAME_OVER
} SHResult;
typedef enum {
    SH_MOVE, SH_TURN, SH_DOOR, SH_SHOOT, SH_MELEE, SH_FRAG, SH_STUN,
    SH_OVERWATCH, SH_CLEAR_JAM, SH_INTERACT, SH_PICKUP, SH_TRANSFER,
    SH_REVEAL, SH_BREACH, SH_SUPPLY
} SHActionType;
typedef enum {
    SH_EVENT_PHASE, SH_EVENT_MOVEMENT, SH_EVENT_WEAPON, SH_EVENT_OVERWATCH,
    SH_EVENT_JAM, SH_EVENT_DAMAGE, SH_EVENT_REVEAL, SH_EVENT_DEPLOYMENT,
    SH_EVENT_OBJECTIVE, SH_EVENT_RESULT, SH_EVENT_ACTIVATION, SH_EVENT_DOOR
} SHEventType;

typedef struct {
    SHActionType type;
    int row, col; /* Zero-based destination/door/blast center. */
    int target;   /* Entity ID for attacks/transfers/scanner inspection. */
    int option;   /* Facing for TURN; item ID for PICKUP; 0 ammo / 1 frag for SUPPLY. */
} SHAction;

typedef struct {
    SHEventType type;
    int round, actor, target, row, col, ap;
    char message[240];
} SHEvent;
typedef void (*SHFeedback)(const SHEvent *event, void *user);
/* Optional deterministic dice source: must return 1..6. Called only for game dice. */
typedef int (*SHDice)(void *user);
/* Override automatic reveal placement. Positions are zero-based. Slot 0 must
   retain the blip square. Other survivors use empty orthogonal neighbors.
   Use (-1,-1) only for excess aliens with no space. acting selects the survivor
   continuing an active blip. Invalid plans fall back to the legal default. */
typedef void (*SHRevealPolicy)(int contact, int strength, int row, int col,
                               int positions[3][2], int *acting, void *user);

typedef struct {
    int training, hard_vacuum, reliable_bursts, ghost_contacts;
    int sealed_bulkheads, supply_cache;
    SHBug specialist; /* BASIC means no optional replacement. */
} SHOptions;

typedef struct {
    int id, alive, extracted, row, col, ap, wounds, done, stunned;
    SHTeam team;
    SHFacing facing;
    SHWeapon weapon;
    SHBug bug;
    int ammunition, frag, stun, overwatch, jammed; /* ammunition=-1 means unlimited. */
    int inspected, quarry;
    int strength; /* -1 for an uninspected blip in a Marine view. */
    int item;     /* -1 means empty mission-item slot. */
} SHEntity;

typedef struct {
    int id;
    const char *name, *briefing, *objective;
    const char *map[SH_ROWS];
    int pool[3], starting, schedule_first, schedule_last, arrivals_per_phase;
    int arrival_cutoff, deadline;
} SHScenario;

typedef enum {
    SH_OBJECTIVE_LEFT = 1u, SH_OBJECTIVE_RIGHT = 2u, SH_OBJECTIVE_POWER = 4u,
    SH_OBJECTIVE_ANALYZE_LEFT = 32u, SH_OBJECTIVE_ANALYZE_RIGHT = 64u,
    SH_OBJECTIVE_THIRD = 128u, SH_OBJECTIVE_STARTED = 256u
} SHObjective;

typedef struct {
    int round, active, entities, pending_target, deadline;
    SHPhase phase;
    SHOutcome outcome;
    unsigned objectives;
    int signal_remaining, clock_round, evacuated, rescued, charges_left;
    int brute_tagged, brute_dead, shield;
} SHStatus;

typedef struct { int id, holder, row, col, dropped; const char *name; } SHItem;

const SHScenario *sh_scenario(int id);
SHGame *sh_create(int scenario, uint32_t seed, const SHOptions *options,
                  SHFeedback feedback, void *user);
void sh_destroy(SHGame *game);
void sh_set_dice(SHGame *game, SHDice dice, void *user);
void sh_set_reveal_policy(SHGame *game, SHRevealPolicy policy, void *user);
SHStatus sh_status(const SHGame *game);
int sh_entity(const SHGame *game, int id, SHTeam viewer, SHEntity *out);
int sh_item(const SHGame *game, int id, SHItem *out);
char sh_tile(const SHGame *game, int row, int col);
/* + | - are walls; blank is inaccessible exterior. # remains a solid-wall
   compatibility glyph. Closed doors are =, never +. Out-of-bounds tiles are blank. */
int sh_tile_is_wall(char tile);
int sh_tile_is_floor(char tile);
const char *sh_last_error(const SHGame *game);
const char *sh_result_name(SHResult result);
const char *sh_weapon_name(SHWeapon weapon);
const char *sh_bug_name(SHBug bug);
int sh_can_see(const SHGame *game, int from, int row, int col);
/* Current team sight: front half-plane, shared by living, onboard Marines.
   Walls and vacuum are always charted; unseen interiors are '?'. Observed
   entities project unseen living hostiles as anonymous blips without changing
   their actual rules identity. Alien view remains complete. */
int sh_marine_visible(const SHGame *game, int row, int col);
char sh_view_tile(const SHGame *game, int row, int col, SHTeam viewer);
int sh_observed_entity(const SHGame *game, int id, SHTeam viewer, SHEntity *out);
int sh_can_shoot(const SHGame *game, int marine, int target);
SHResult sh_activate(SHGame *game, int entity);
SHResult sh_action(SHGame *game, SHAction action);
SHResult sh_end_activation(SHGame *game);
SHResult sh_end_phase(SHGame *game);
/* Deployment is a distinct choice phase. Entries are 'A'..'F'. */
SHResult sh_deploy(SHGame *game, char entry);
SHResult sh_finish_deployment(SHGame *game);
/* One response per Marine per reaction window; fire=0 declines that response.
   finish_reactions declines all remaining responses and resumes a paused attack. */
SHResult sh_react(SHGame *game, int marine, int fire);
SHResult sh_finish_reactions(SHGame *game);
/* Render the public board; blip strengths never appear. Returns required bytes. */
size_t sh_board(const SHGame *game, char *buffer, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
