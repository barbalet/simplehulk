#include "simple_hulk.h"
#include <stdio.h>
#include <string.h>
#ifdef __wasm__
#define EXPORT(name) __attribute__((export_name(name)))
__attribute__((import_module("env"),import_name("feedback")))
#else
#define EXPORT(name)
#endif
extern void browser_feedback(int type,int actor,int target,int row,int col,const char *message,int round);
static SHGame *game;
static char json[32768];
static void feedback(const SHEvent *e,void *user) {(void)user;browser_feedback(e->type,e->actor,e->target,e->row,e->col,e->message,e->round);}
EXPORT("start") int start(int scenario,unsigned seed,int specialist,int flags) {
    SHOptions o;memset(&o,0,sizeof o);o.specialist=(SHBug)specialist;
    o.training=flags&1;o.hard_vacuum=flags&2;o.reliable_bursts=flags&4;o.ghost_contacts=flags&8;o.sealed_bulkheads=flags&16;o.supply_cache=flags&32;
    sh_destroy(game);game=sh_create(scenario,seed,&o,feedback,NULL);return game!=NULL;
}
EXPORT("command") int command(int op,int a,int b,int c,int d) {
    SHAction action;
    if(!game)return SH_INVALID;
    switch(op) {
    case 0:return sh_activate(game,a);
    case 1:action.type=(SHActionType)a;action.row=b;action.col=c;action.target=d;action.option=d;return sh_action(game,action);
    case 2:return sh_end_activation(game);
    case 3:return sh_end_phase(game);
    case 4:return sh_deploy(game,(char)a);
    case 5:return sh_finish_deployment(game);
    case 6:return sh_react(game,a,b);
    case 7:return sh_finish_reactions(game);
    default:return SH_INVALID;
    }
}
EXPORT("error") const char *error(void) {return sh_last_error(game);}
EXPORT("snapshot") const char *snapshot(int viewer) {
    SHStatus s=sh_status(game);SHEntity e;SHItem item;int i,r,c,n=0;
    n+=snprintf(json+n,sizeof json-(size_t)n,"{\"round\":%d,\"active\":%d,\"phase\":%d,\"outcome\":%d,\"pending\":%d,\"deadline\":%d,\"objectives\":%d,\"evacuated\":%d,\"signal\":%d,\"map\":[",s.round,s.active,s.phase,s.outcome,s.pending_target,s.deadline,(int)s.objectives,s.evacuated,s.signal_remaining);
    for(r=0;r<SH_ROWS;r++) {json[n++]='"';for(c=0;c<SH_COLS;c++)json[n++]=sh_tile(game,r,c);json[n++]='"';if(r<SH_ROWS-1)json[n++]=',';}
    n+=snprintf(json+n,sizeof json-(size_t)n,"],\"entities\":[");
    for(i=0;i<s.entities;i++) {sh_entity(game,i,(SHTeam)viewer,&e);
        n+=snprintf(json+n,sizeof json-(size_t)n,"%s{\"id\":%d,\"alive\":%d,\"extracted\":%d,\"r\":%d,\"c\":%d,\"ap\":%d,\"wounds\":%d,\"done\":%d,\"team\":%d,\"facing\":%d,\"weapon\":%d,\"bug\":%d,\"ammo\":%d,\"frag\":%d,\"stun\":%d,\"ow\":%d,\"jam\":%d,\"stunned\":%d,\"strength\":%d,\"item\":%d}",i?",":"",e.id,e.alive,e.extracted,e.row,e.col,e.ap,e.wounds,e.done,e.team,e.facing,e.weapon,e.bug,e.ammunition,e.frag,e.stun,e.overwatch,e.jammed,e.stunned,e.strength,e.item);
    }
    n+=snprintf(json+n,sizeof json-(size_t)n,"],\"items\":[");
    for(i=0;sh_item(game,i,&item);i++)n+=snprintf(json+n,sizeof json-(size_t)n,"%s{\"id\":%d,\"holder\":%d,\"r\":%d,\"c\":%d,\"dropped\":%d,\"name\":\"%s\"}",i?",":"",item.id,item.holder,item.row,item.col,item.dropped,item.name);
    snprintf(json+n,sizeof json-(size_t)n,"],\"clockRound\":%d,\"rescued\":%d,\"chargesLeft\":%d,\"bruteTagged\":%d,\"bruteDead\":%d,\"shield\":%d}",s.clock_round,s.rescued,s.charges_left,s.brute_tagged,s.brute_dead,s.shield);return json;
}
EXPORT("mission_name") const char *mission_name(int id) {const SHScenario *s=sh_scenario(id);return s?s->name:"";}
EXPORT("mission_objective") const char *mission_objective(int id) {const SHScenario *s=sh_scenario(id);return s?s->objective:"";}
