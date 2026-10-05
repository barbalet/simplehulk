#include "simple_hulk.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void feedback(const SHEvent *event, void *user) {
    (void)user;
    printf("[round %02d / AP %d] %s\n", event->round, event->ap, event->message);
}
static void help(void) {
    puts("Map: + | - hull walls, blank exterior, = closed door, / open door, . floor.\n"
         "Commands (map coordinates are one-based):\n"
         "  map | status | units | items | view marine|alien\n"
         "  activate ID | move ROW COL | turn north|east|south|west\n"
         "  door ROW COL | shoot TARGET | flame ROW COL | melee TARGET\n"
         "  frag ROW COL | stun ROW COL | overwatch | clear\n"
         "  interact [BLIP_TO_INSPECT] | pickup ITEM_ID | transfer MARINE_ID\n"
         "  reveal | end | phase | deploy A..F | ready\n"
         "  react MARINE_ID fire|hold | resolve | help | quit\n"
         "Marine and alien activations must finish before another starts.\n"
         "After an alien action, resolve any pending overwatch before continuing.\n"
         "A deployment phase ends with 'ready' once arrivals are placed or blocked.\n"
         "'interact' at the recorder supply locker selects a fragmentation grenade.\n"
         "Use 'interact-ammo' there to select four cannon ammunition instead.");
}
static void units(const SHGame *game, SHTeam viewer) {
    SHStatus status = sh_status(game);
    SHEntity e;
    int i;
    for (i = 0; i < status.entities; i++) {
        sh_entity(game, i, viewer, &e);
        if (!e.alive || e.extracted) continue;
        printf("%2d %-6s (%2d,%2d) AP=%d wounds=%d", i,
               e.team == SH_MARINE ? "Marine" : e.team == SH_BLIP ? "blip" : "alien",
               e.row + 1, e.col + 1, e.ap, e.wounds);
        if (e.team == SH_MARINE)
            printf(" %s facing=%d ammo=%d overwatch=%d jam=%d item=%d",
                   sh_weapon_name(e.weapon), e.facing, e.ammunition,
                   e.overwatch, e.jammed, e.item);
        if (e.team == SH_BLIP)
            printf(" strength=%d (-1 means hidden)", e.strength);
        putchar('\n');
    }
}
int main(int argc, char **argv) {
    SHGame *game;
    SHStatus status;
    SHAction action;
    SHResult result;
    SHTeam viewer = SH_MARINE;
    unsigned long mission = 0, seed = 2026;
    char line[256], command[32], word[32], *end;
    int row, col, target;
    if (argc > 3) return 2;
    if (argc > 1) {
        errno = 0; mission = strtoul(argv[1], &end, 10);
        if (errno || !*argv[1] || *end || mission >= SH_SCENARIOS) return 2;
    }
    if (argc > 2) {
        errno = 0; seed = strtoul(argv[2], &end, 10);
        if (errno || !*argv[2] || *end || seed > 4294967295UL) return 2;
    }
    game = sh_create((int)mission, (uint32_t)seed, NULL, feedback, NULL);
    if (!game) return 1;
    help();
    while (fputs("hulk> ", stdout), fflush(stdout), fgets(line, sizeof line, stdin)) {
        if (sscanf(line, "%31s", command) != 1) continue;
        if (!strcmp(command, "quit")) break;
        if (!strcmp(command, "help")) { help(); continue; }
        if (!strcmp(command, "map")) {
            char board[900]; sh_board(game, board, sizeof board); puts(board); continue;
        }
        if (!strcmp(command, "status")) {
            status = sh_status(game);
            printf("%s: round=%d phase=%d active=%d reaction=%d outcome=%d deadline=%d objectives=%u\n",
                   sh_scenario((int)mission)->name, status.round, status.phase, status.active,
                   status.pending_target, status.outcome, status.deadline, status.objectives);
            continue;
        }
        if (!strcmp(command, "units")) { units(game, viewer); continue; }
        if (!strcmp(command, "items")) {
            SHItem item; int i;
            for (i = 0; sh_item(game, i, &item); i++)
                printf("%d %s holder=%d ground=(%d,%d) dropped=%d\n", i, item.name,
                       item.holder, item.row + 1, item.col + 1, item.dropped);
            continue;
        }
        if (!strcmp(command, "view") && sscanf(line, "%*s %31s", word) == 1) {
            if (!strcmp(word, "marine")) viewer = SH_MARINE;
            else if (!strcmp(word, "alien")) viewer = SH_ALIEN;
            else { puts("Choose marine or alien."); continue; }
            puts("View changed; pass the console to that player before inspecting units.");
            continue;
        }
        result = SH_INVALID;
        memset(&action, 0, sizeof action); action.target = -1;
        if (!strcmp(command, "activate") && sscanf(line, "%*s %d", &target) == 1)
            result = sh_activate(game, target);
        else if (!strcmp(command, "end")) result = sh_end_activation(game);
        else if (!strcmp(command, "phase")) result = sh_end_phase(game);
        else if (!strcmp(command, "ready")) result = sh_finish_deployment(game);
        else if (!strcmp(command, "deploy") && sscanf(line, "%*s %31s", word) == 1)
            result = sh_deploy(game, word[0]);
        else if (!strcmp(command, "resolve")) result = sh_finish_reactions(game);
        else if (!strcmp(command, "react") && sscanf(line, "%*s %d %31s", &target, word) == 2) {
            if (!strcmp(word, "fire") || !strcmp(word, "hold"))
                result = sh_react(game, target, !strcmp(word, "fire"));
        } else {
            int recognized = 1;
            if (!strcmp(command, "move")) action.type = SH_MOVE;
            else if (!strcmp(command, "door")) action.type = SH_DOOR;
            else if (!strcmp(command, "flame")) action.type = SH_SHOOT;
            else if (!strcmp(command, "frag")) action.type = SH_FRAG;
            else if (!strcmp(command, "stun")) action.type = SH_STUN;
            else recognized = 0;
            if (recognized) {
                if (sscanf(line, "%*s %d %d", &row, &col) != 2 || row < 1 || col < 1 || row > SH_ROWS || col > SH_COLS) {
                    puts("Provide a row 1..25 and column 1..31."); continue;
                }
                action.row = row - 1; action.col = col - 1;
            } else if (!strcmp(command, "turn") && sscanf(line, "%*s %31s", word) == 1) {
                action.type = SH_TURN;
                action.option = !strcmp(word, "north") ? SH_NORTH : !strcmp(word, "east") ? SH_EAST :
                                !strcmp(word, "south") ? SH_SOUTH : !strcmp(word, "west") ? SH_WEST : -1;
            } else if ((!strcmp(command, "shoot") || !strcmp(command, "melee") || !strcmp(command, "transfer") || !strcmp(command, "pickup")) && sscanf(line, "%*s %d", &target) == 1) {
                action.type = !strcmp(command, "shoot") ? SH_SHOOT : !strcmp(command, "melee") ? SH_MELEE : !strcmp(command, "transfer") ? SH_TRANSFER : SH_PICKUP;
                action.target = target; action.option = target;
            } else if (!strcmp(command, "overwatch")) action.type = SH_OVERWATCH;
            else if (!strcmp(command, "clear")) action.type = SH_CLEAR_JAM;
            else if (!strcmp(command, "reveal")) action.type = SH_REVEAL;
            else if (!strcmp(command, "interact") || !strcmp(command, "interact-ammo")) {
                action.type = SH_INTERACT; action.option = strcmp(command, "interact-ammo") != 0;
                if (sscanf(line, "%*s %d", &target) == 1) action.target = target;
            } else { puts("Unknown or incomplete command. Type help."); continue; }
            result = sh_action(game, action);
        }
        if (result != SH_OK) printf("Action rejected: %s (%s).\n", sh_result_name(result), sh_last_error(game));
    }
    sh_destroy(game);
    return 0;
}
