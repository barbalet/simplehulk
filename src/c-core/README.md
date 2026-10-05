# Simple Hulk C core

A C99 library implementing the core rules and all nine current missions. The
engine has no third-party dependencies, file loading, global game state, or UI
requirements. `simple_hulk.h` is the public API. `rules.c` owns action validation
and game transitions; `scenarios.c` contains the maps and mission setup data.
`api_cli.c` is an interactive front end that uses the same API as the tests.

Build from the repository root with `make`. Start a game with:

```sh
./build/simple-hulk 0 2026
```

The arguments are mission ID and random seed. ID 0 is Wake the beacon; IDs 1–8
correspond to the eight numbered scenarios in the FLUFF book. `help` lists the
commands. The CLI uses one-based map coordinates; C API coordinates are zero-based.

## Hull map cells

All nine `SHScenario.map` arrays match the outlined maps in RULES and FLUFF,
including their blank exterior cells. Rows remain exactly 31 characters wide;
the books omit trailing blank cells when printing. Objectives and paths retain
their original coordinates.

`+`, `|`, `-` are hull walls, blank is inaccessible exterior, `=` is a closed
door, `/` is an open door, and `.` or a mission letter/number is floor. In
particular, entry `D` is floor and wall corner `+` is never a door. `#` is still
recognized as a solid wall for caller-created boards, but published maps use
outlines. `sh_tile` returns blank for out-of-bounds coordinates.

Use `sh_tile_is_wall` and `sh_tile_is_floor` when inspecting cells. For a route
planner, allow floor or `SH_CLOSED_DOOR`, then open the door before entering.
Closed doors, walls and exterior block both movement and sight, are excluded
from blast/reveal placement, and cannot host arriving contacts. Door actions
operate only `SH_CLOSED_DOOR` / `SH_OPEN_DOOR`; hull corners cannot be opened or
breached. The same classifiers are used by the core and native test bots.

## Calling the API

```c
#include "simple_hulk.h"
#include <stdio.h>

static void feedback(const SHEvent *event, void *user) {
    (void)user;
    printf("Round %d: %s\n", event->round, event->message);
}

int main(void) {
    SHGame *game = sh_create(0, 2026, NULL, feedback, NULL);
    SHAction move = {SH_MOVE, 21, 13, -1, 0};
    if (!game) return 1;
    if (sh_activate(game, 0) == SH_OK && sh_action(game, move) == SH_OK) {
        SHAction hold = {SH_OVERWATCH, 0, 0, -1, 0};
        sh_action(game, hold);
    }
    sh_destroy(game);
    return 0;
}
```

Compile a caller against `build/libsimplehulk.a` with `-Isrc/c-core`. Keep one
thread per game, or serialize access yourself. The engine copies `SHOptions`;
callback contexts must live until callbacks are replaced or the game is destroyed.
Event messages are valid during the callback; copy them if keeping a history.
Callbacks must not reenter or destroy their game. `sh_set_dice` allows deterministic
dice for tests. An invalid custom die value is treated as 1.

## Turns and reactions

1. `sh_activate` selects one live, unactivated entity of the current team.
2. `sh_action` spends AP only after the requested action has passed validation.
   `sh_end_activation` voluntarily discards unused AP. Overwatch and evacuation
   end activation automatically. Re-read `sh_status` after every successful action:
   death, a reveal choice, or mission completion can change the active entity.
3. After an alien action, inspect `pending_target`. If it is nonnegative, call
   `sh_react` for eligible Marines in the desired order. Each gets one response;
   `fire=0` declines it. Reactions spend ammunition but not AP.
4. `sh_finish_reactions` declines any remaining responses and resolves a paused
   alien attack if its attacker survived. No unrelated action can occur while a
   reaction is pending. There is no reaction merely for arrival or reveal.
5. `sh_end_phase` ends the Marine phase, applies phase-end objectives, and checks
   the deadline before an alien phase can occur. The next state is deployment.
6. Place scheduled/queued contacts with `sh_deploy('A'..'F')`, then call
   `sh_finish_deployment`. The engine refuses to finish while a legal arrival is
   still available. It enforces distinct entries, caps, FIFO queues, cutoff phases,
   the quarantine alarm, and missions that stop arrivals.
7. Activate aliens/blips, including siblings created by reveals. Ending the alien
   phase clears stun and begins the next Marine phase.

Ending a phase also passes on any models that have not activated; that is a legal
choice, not a source of extra AP. A game ends in a Marine win or an alien win.
After completion, mutation calls return `SH_GAME_OVER`.

## State and decisions

`sh_status`, `sh_entity`, `sh_item`, `sh_tile`, and `sh_board` supply the front end
with snapshots. Entity IDs are stable for the lifetime of a game; removed entities
remain queryable. A blip becomes an alien using its original ID. New siblings have
new IDs. Public maps show `b` for a blip, never its strength. A Marine entity view
hides uninspected blip strength, specialist type, and quarry identity. This is a
local game interface, not an authentication boundary: an alien view contains the
alien player's information, so the front end decides which player can see it.

By default a reveal fills empty neighbors north, east, south, west, and continues
with the alien on the former blip square. `sh_set_reveal_policy` lets a front end
choose legal neighboring placements and the continuing alien. Invalid plans fall
back to the default. Revealed aliens share remaining AP during an active reveal;
unactivated specialists receive their own profile budget. Excess aliens are lost
when there is no space.

Weapon and grenade actions enforce range, front arc, blocking models, walls,
door corners, ammunition, friendly area damage, and stun. Marines see in all
facings. The rifle/shotgun/cannon can enter overwatch; the flame projector cannot.
One die of 1 cancels an entire normal overwatch burst and jams the gun. Ordinary
shots never jam. Rear attacks add one to the attacking alien's melee total.

`SH_INTERACT` implements the mission action on the active Marine's square; it
checks costs and prerequisites. Its `target` chooses a hidden contact for a scanner.
At the recorder locker, `option=1` selects a fragmentation grenade and `option=0`
selects cannon ammunition. `SH_PICKUP` uses `option` as a dropped item ID;
`SH_TRANSFER` uses `target` as an adjacent recipient. Native initial pickup actions
must use `SH_INTERACT`, so generic pickup cannot bypass locks or collection costs.

Mission-item holder `-1` means on the map, `-2` means consumed/extracted. `dropped`
distinguishes a recoverable dropped item from a still-locked initial objective.
Ground coordinates are meaningful only for ground items. Marines have one mission
item slot. Capsule carriers pay 2 AP for movement. Free shuttle boarding still
requires an explicit action during an activation and ends that activation.

The optional settings implement training time, hard vacuum, reliable bursts,
ghost contacts, breachable bulkheads, a cannon supply cache, and one specialist
replacement. The hunt also includes its mandatory brute. A separate quarry flag
prevents another optional brute from satisfying the hunt's target condition.

## Mission objective flags

The public `objectives` bitset preserves completed actions. Flags are independent
of item collection, which is represented by `sh_item` and each Marine's item slot.

| Bit | Meaning |
|---|---|
| 1 | Left relay/scanner or anchor/valve 1 |
| 2 | Right relay/scanner or anchor/valve 2 |
| 4 | Power isolation / capsule life support |
| 32 | Left quarantine sample analyzed |
| 64 | Right quarantine sample analyzed |
| 128 | Anchor/valve 3 |
| 256 | Transmitter, shutdown, or launch authorized |

Signal markers, rescue totals, evacuation totals, clocks, and quarry progress are
also present in `SHStatus`. The core faithfully uses the books' mission deadlines;
it does not increase them to make a bot win. Maps and setup data currently match
the Markdown books. Update `scenarios.c` and the objective handlers together when
changing a mission, then run the tests.
