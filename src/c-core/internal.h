#ifndef SH_INTERNAL_H
#define SH_INTERNAL_H
#include "simple_hulk.h"
#define SH_MAX_ITEMS 8
#define SH_MAX_CONTACTS 16
#define SH_LEFT SH_OBJECTIVE_LEFT
#define SH_RIGHT SH_OBJECTIVE_RIGHT
#define SH_POWER SH_OBJECTIVE_POWER
#define SH_ANALYZE_LEFT SH_OBJECTIVE_ANALYZE_LEFT
#define SH_ANALYZE_RIGHT SH_OBJECTIVE_ANALYZE_RIGHT
#define SH_THIRD SH_OBJECTIVE_THIRD
#define SH_STARTED SH_OBJECTIVE_STARTED

typedef struct { int strength, quarry; SHBug bug; } SHContact;
typedef struct { SHItem view; int type; } SHStoredItem;
enum { ITEM_RECORDER, ITEM_CAPACITOR, ITEM_CAPSULE_L, ITEM_CAPSULE_R,
       ITEM_CHARGE, ITEM_SAMPLE_L, ITEM_SAMPLE_R, ITEM_LEDGER, ITEM_KEY };
struct SHGame {
    const SHScenario *scenario;
    SHOptions options;
    SHStatus status;
    SHEntity entity[SH_MAX_ENTITIES];
    SHStoredItem items[SH_MAX_ITEMS];
    int item_count;
    char board[SH_ROWS][SH_COLS];
    SHContact contacts[SH_MAX_CONTACTS];
    int contact_count, normal_contacts, scheduled, queue[SH_MAX_CONTACTS];
    int queue_first, queue_last, arrived, entry_used[6], alarm, arrivals_stopped;
    int supply_used, pending_attack, pending_victim;
    unsigned reaction_seen;
    uint32_t rng;
    SHFeedback feedback;
    void *feedback_user;
    SHRevealPolicy reveal_policy;
    void *reveal_user;
    SHDice dice;
    void *dice_user;
    char error[160];
};
#endif
