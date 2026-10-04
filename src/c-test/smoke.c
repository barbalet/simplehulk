#include "test.h"
/* White-box fixtures construct small positions. The full-game runner uses only
   the public API and never edits state or bypasses objective prerequisites. */
#include "../c-core/internal.h"
#include <stdio.h>
#include <string.h>

static int checks,failures;
#define CHECK(test) do {checks++;if(!(test)){failures++;fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#test);}} while(0)
typedef struct {int values[24],length,pos;} Dice;
static int fixed_die(void *user) {Dice *d=user;return d->values[d->pos++%d->length];}
static void dice(SHGame *g,Dice *d,int a,int b) {memset(d,0,sizeof *d);d->values[0]=a;d->values[1]=b;d->length=2;sh_set_dice(g,fixed_die,d);}
static SHAction act(SHActionType type,int r,int c,int target,int option) {
    SHAction a;a.type=type;a.row=r;a.col=c;a.target=target;a.option=option;return a;
}
static SHGame *fixture(int mission) {
    SHGame *g=sh_create(mission,123,NULL,NULL,NULL);int r,c,i;
    if(!g) return NULL;
    for(r=0;r<SH_ROWS;r++) for(c=0;c<SH_COLS;c++) g->board[r][c]=r==0||c==0||r==SH_ROWS-1||c==SH_COLS-1?'#':'.';
    g->status.entities=5;g->status.active=-1;g->status.pending_target=-1;
    for(i=0;i<5;i++){g->entity[i].row=i?20:10;g->entity[i].col=i?2+i*3:10;g->entity[i].ap=4;g->entity[i].done=0;g->entity[i].alive=1;}
    return g;
}
static int enemy(SHGame *g,int r,int c,SHTeam team,SHBug bug,int strength) {
    int id=g->status.entities++;SHEntity *e=&g->entity[id];
    memset(e,0,sizeof *e);e->id=id;e->alive=1;e->row=r;e->col=c;e->team=team;e->bug=bug;e->strength=strength;
    e->quarry=g->scenario->id==7&&bug==SH_BRUTE;e->wounds=team==SH_ALIEN&&bug==SH_BRUTE?3:1;e->ap=6;e->ammunition=-1;e->item=-1;return id;
}
static void alien_phase(SHGame *g) {CHECK(sh_end_phase(g)==SH_OK);CHECK(sh_finish_deployment(g)==SH_OK);}
static void station(SHGame *g,int id,char label) {
    /* Each station call prepares a fresh legal activation fixture. */
    g->status.active=-1;g->entity[id].done=0;g->entity[id].ap=4;g->entity[id].row=10;g->entity[id].col=10;g->board[10][10]=label;
    CHECK(sh_activate(g,id)==SH_OK);
}
static void objective(SHGame *g,int id,char label) {
    station(g,id,label);CHECK(sh_action(g,act(SH_INTERACT,0,0,-1,1))==SH_OK);
}
static void test_setup(void) {
    int i,j;SHEntity e;char board[900],small[3];SHGame *a,*b;
    CHECK(sh_create(-1,1,NULL,NULL,NULL)==NULL);CHECK(sh_scenario(9)==NULL);
    for(i=0;i<9;i++) {
        a=sh_create(i,44,NULL,NULL,NULL);b=sh_create(i,44,NULL,NULL,NULL);CHECK(a!=NULL&&b!=NULL);
        CHECK(sh_status(a).round==1&&sh_status(a).phase==SH_MARINE_PHASE);
        CHECK(sh_board(a,board,sizeof board)==801);CHECK(strlen(board)==800);
        CHECK(sh_board(a,small,sizeof small)==801&&small[2]=='\0');
        for(j=0;j<5;j++){CHECK(sh_entity(a,j,SH_MARINE,&e));CHECK(e.ap==4&&e.wounds==2&&e.frag==1&&e.stun==1);}
        CHECK(a->entity[2].ammunition==8&&a->entity[3].ammunition==4);
        CHECK(a->normal_contacts==b->normal_contacts);
        CHECK(memcmp(a->contacts,b->contacts,sizeof a->contacts)==0);
        for(j=5;j<a->status.entities;j++) {
            CHECK(sh_entity(a,j,SH_MARINE,&e));CHECK(e.team==SH_BLIP&&e.strength==-1);
            CHECK(sh_entity(a,j,SH_ALIEN,&e));CHECK(e.strength>=1&&e.strength<=3);
        }
        if(i==7){int quarry=0;for(j=5;j<9;j++)if(a->entity[j].quarry){quarry++;CHECK(a->entity[j].bug==SH_BRUTE&&a->entity[j].strength==1);}CHECK(quarry==1);}
        sh_destroy(a);sh_destroy(b);
    }
}
static void test_movement_sight(void) {
    SHGame *g=fixture(0);int a=enemy(g,9,11,SH_ALIEN,SH_BASIC,0);SHEntity before;
    CHECK(sh_activate(g,0)==SH_OK);before=g->entity[0];
    CHECK(sh_action(g,act(SH_MOVE,11,11,-1,0))==SH_INVALID);CHECK(memcmp(&before,&g->entity[0],sizeof before)==0);
    CHECK(sh_action(g,act(SH_MOVE,11,10,-1,0))==SH_OK);CHECK(g->entity[0].ap==2);
    CHECK(sh_activate(g,1)==SH_INVALID);
    CHECK(sh_action(g,act(SH_TURN,0,0,-1,SH_SOUTH))==SH_OK);CHECK(g->entity[0].ap==0);
    CHECK(sh_action(g,act(SH_MOVE,12,10,-1,0))==SH_NO_AP);
    CHECK(sh_end_activation(g)==SH_OK);CHECK(sh_activate(g,0)==SH_INVALID);CHECK(sh_activate(g,a)==SH_WRONG_PHASE);
    g->entity[0].row=10;g->entity[0].col=10;g->entity[0].facing=SH_NORTH;
    CHECK(sh_can_see(g,0,9,11));g->board[10][11]='#';CHECK(!sh_can_see(g,0,9,11));g->board[10][11]='.';
    g->board[9][10]='+';CHECK(!sh_can_see(g,0,8,10));g->board[9][10]='/';CHECK(sh_can_see(g,0,8,10));
    g->entity[a].row=11;g->entity[a].col=10;CHECK(sh_can_see(g,0,11,10));CHECK(!sh_can_shoot(g,0,a));
    g->entity[a].row=10;g->entity[a].col=18;CHECK(sh_can_shoot(g,0,a));g->entity[a].col=19;CHECK(!sh_can_shoot(g,0,a));
    sh_destroy(g);
}
static void test_overwatch(void) {
    Dice d;SHGame *g=fixture(0);int a=enemy(g,9,10,SH_ALIEN,SH_BASIC,0);
    dice(g,&d,6,6);CHECK(sh_activate(g,0)==SH_OK);CHECK(sh_action(g,act(SH_OVERWATCH,0,0,-1,0))==SH_OK);
    CHECK(g->status.active==-1&&g->entity[0].ap==0&&g->entity[0].overwatch);
    alien_phase(g);CHECK(sh_activate(g,a)==SH_OK);CHECK(sh_action(g,act(SH_MELEE,0,0,0,0))==SH_OK);
    CHECK(g->status.pending_target==a&&d.pos==0);CHECK(sh_action(g,act(SH_MOVE,8,10,-1,0))==SH_REACTION_PENDING);
    CHECK(sh_react(g,0,1)==SH_OK);CHECK(!g->entity[a].alive&&g->entity[0].wounds==2);
    CHECK(sh_finish_reactions(g)==SH_OK);CHECK(d.pos==2);CHECK(sh_end_phase(g)==SH_OK);CHECK(!g->entity[0].overwatch);
    sh_destroy(g);
    g=fixture(0);a=enemy(g,9,10,SH_ALIEN,SH_BASIC,0);g->entity[0].weapon=SH_CANNON;g->entity[0].ammunition=1;
    dice(g,&d,1,6);CHECK(sh_activate(g,0)==SH_OK);CHECK(sh_action(g,act(SH_OVERWATCH,0,0,-1,0))==SH_OK);alien_phase(g);
    CHECK(sh_activate(g,a)==SH_OK);CHECK(sh_action(g,act(SH_MOVE,8,10,-1,0))==SH_OK);
    CHECK(sh_react(g,0,1)==SH_OK);CHECK(g->entity[a].wounds==1&&g->entity[0].jammed&&g->entity[0].ammunition==0&&!g->entity[0].overwatch);
    CHECK(sh_finish_reactions(g)==SH_OK);CHECK(sh_end_activation(g)==SH_OK);CHECK(sh_end_phase(g)==SH_OK);
    CHECK(sh_activate(g,0)==SH_OK);CHECK(sh_action(g,act(SH_CLEAR_JAM,0,0,-1,0))==SH_OK);CHECK(g->entity[0].ap==3);
    CHECK(sh_action(g,act(SH_OVERWATCH,0,0,-1,0))==SH_NO_AMMO);sh_destroy(g);
    g=fixture(0);a=enemy(g,9,10,SH_ALIEN,SH_BASIC,0);dice(g,&d,1,6);
    CHECK(sh_activate(g,0)==SH_OK);CHECK(sh_action(g,act(SH_SHOOT,0,0,a,0))==SH_OK);CHECK(!g->entity[0].jammed&&!g->entity[a].alive);sh_destroy(g);
    g=fixture(0);a=enemy(g,9,10,SH_ALIEN,SH_BASIC,0);dice(g,&d,6,1);
    CHECK(sh_activate(g,0)==SH_OK);CHECK(sh_action(g,act(SH_OVERWATCH,0,0,-1,0))==SH_OK);alien_phase(g);
    CHECK(sh_activate(g,a)==SH_OK);CHECK(sh_action(g,act(SH_MELEE,0,0,0,0))==SH_OK);CHECK(sh_react(g,0,0)==SH_OK);
    CHECK(sh_react(g,0,1)==SH_INVALID);CHECK(sh_finish_reactions(g)==SH_OK);CHECK(g->entity[0].wounds==1);sh_destroy(g);
}
static void test_weapons_blips(void) {
    SHGame *g;Dice d;int a,b,i;
    g=fixture(0);a=enemy(g,9,10,SH_ALIEN,SH_BRUTE,0);g->entity[0].weapon=SH_FLAME;g->entity[0].ammunition=4;dice(g,&d,3,3);
    CHECK(sh_activate(g,0)==SH_OK);CHECK(sh_action(g,act(SH_OVERWATCH,0,0,-1,0))==SH_INVALID);
    CHECK(sh_action(g,act(SH_SHOOT,9,10,a,0))==SH_OK);CHECK(g->entity[a].wounds==1);CHECK(!g->entity[0].alive);sh_destroy(g);
    g=fixture(0);a=enemy(g,8,10,SH_ALIEN,SH_BASIC,0);b=enemy(g,8,11,SH_ALIEN,SH_BASIC,0);dice(g,&d,4,4);
    CHECK(sh_activate(g,0)==SH_OK);CHECK(sh_action(g,act(SH_FRAG,8,10,-1,0))==SH_OK);
    CHECK(!g->entity[a].alive&&!g->entity[b].alive&&g->entity[0].frag==0&&g->entity[0].ap==2);sh_destroy(g);
    g=fixture(0);a=enemy(g,8,10,SH_ALIEN,SH_BASIC,0);
    CHECK(sh_activate(g,0)==SH_OK);CHECK(sh_action(g,act(SH_STUN,8,10,-1,0))==SH_OK);
    CHECK(sh_action(g,act(SH_STUN,8,10,-1,0))==SH_NO_AMMO);CHECK(sh_end_activation(g)==SH_OK);alien_phase(g);
    CHECK(g->entity[a].ap==4);CHECK(sh_end_phase(g)==SH_OK);CHECK(!g->entity[a].stunned);sh_destroy(g);
    g=fixture(0);g->board[9][10]='+';a=enemy(g,8,10,SH_BLIP,SH_BASIC,3);alien_phase(g);
    CHECK(sh_activate(g,a)==SH_OK);CHECK(sh_action(g,act(SH_DOOR,9,10,-1,0))==SH_OK);
    CHECK(g->entity[a].team==SH_ALIEN&&g->entity[a].ap==5);CHECK(g->status.entities==8);
    for(i=5;i<8;i++)CHECK(g->entity[i].ap==5);
    CHECK(sh_end_activation(g)==SH_OK);CHECK(sh_activate(g,6)==SH_OK);CHECK(g->entity[6].ap==5);sh_destroy(g);
    g=fixture(0);a=enemy(g,9,10,SH_BLIP,SH_BASIC,3);g->board[8][10]=g->board[9][9]=g->board[9][11]='#';
    CHECK(sh_activate(g,0)==SH_OK);CHECK(sh_action(g,act(SH_TURN,0,0,-1,SH_EAST))==SH_OK);
    CHECK(g->entity[a].team==SH_ALIEN&&g->status.entities==6);sh_destroy(g);
}
static void test_items_deployment(void) {
    SHGame *g=fixture(1);Dice d;SHItem item;int a;
    objective(g,0,'P');objective(g,0,'R');g->entity[1].row=10;g->entity[1].col=11;g->entity[1].wounds=1;
    CHECK(sh_action(g,act(SH_TRANSFER,0,0,1,0))==SH_OK);CHECK(g->entity[0].item==-1&&g->entity[1].item==0);
    CHECK(sh_end_activation(g)==SH_OK);a=enemy(g,9,11,SH_ALIEN,SH_BASIC,0);alien_phase(g);dice(g,&d,6,1);
    CHECK(sh_activate(g,a)==SH_OK);CHECK(sh_action(g,act(SH_MELEE,0,0,1,0))==SH_OK);CHECK(!g->entity[1].alive);
    CHECK(sh_item(g,0,&item)&&item.dropped&&item.holder==-1&&item.row==10&&item.col==11);
    CHECK(sh_end_activation(g)==SH_OK);CHECK(sh_end_phase(g)==SH_OK);CHECK(sh_activate(g,0)==SH_OK);
    CHECK(sh_action(g,act(SH_MOVE,10,11,-1,0))==SH_OK);CHECK(sh_action(g,act(SH_PICKUP,0,0,-1,0))==SH_OK);CHECK(g->entity[0].item==0);sh_destroy(g);
    g=sh_create(0,4,NULL,NULL,NULL);CHECK(sh_end_phase(g)==SH_OK);CHECK(sh_finish_deployment(g)==SH_OK);CHECK(sh_end_phase(g)==SH_OK);
    CHECK(sh_end_phase(g)==SH_OK);CHECK(sh_finish_deployment(g)==SH_INVALID);
    CHECK(sh_deploy(g,'E')==SH_OK);CHECK(sh_deploy(g,'E')==SH_BLOCKED);CHECK(sh_deploy(g,'F')==SH_OK);CHECK(sh_finish_deployment(g)==SH_OK);sh_destroy(g);
    g=fixture(6);g->board[4][4]='F';g->board[4][5]='A';g->status.round=2;g->alarm=1;enemy(g,4,4,SH_ALIEN,SH_BASIC,0);
    CHECK(sh_end_phase(g)==SH_OK);CHECK(sh_deploy(g,'A')==SH_INVALID);CHECK(sh_finish_deployment(g)==SH_OK);CHECK(g->queue_first<g->queue_last);
    CHECK(sh_end_phase(g)==SH_OK);g->entity[5].alive=0;CHECK(sh_end_phase(g)==SH_OK);CHECK(sh_deploy(g,'F')==SH_OK);CHECK(!g->alarm);sh_destroy(g);
}
static void test_objectives(void) {
    SHGame *g;int id;Dice d;
    g=fixture(0);station(g,0,'X');CHECK(sh_action(g,act(SH_INTERACT,0,0,-1,0))==SH_INVALID);CHECK(g->entity[0].ap==4);
    objective(g,0,'U');objective(g,0,'V');objective(g,0,'X');CHECK(sh_status(g).outcome==SH_MARINES_WIN);CHECK(sh_activate(g,1)==SH_GAME_OVER);sh_destroy(g);
    g=fixture(1);objective(g,0,'P');objective(g,0,'R');objective(g,0,'Z');CHECK(g->status.outcome==SH_MARINES_WIN);sh_destroy(g);
    g=fixture(2);objective(g,0,'U');objective(g,0,'V');objective(g,0,'X');objective(g,0,'Q');objective(g,0,'X');
    CHECK(g->status.signal_remaining==2);CHECK(sh_end_activation(g)==SH_OK);alien_phase(g);CHECK(g->status.signal_remaining==2);
    CHECK(sh_end_phase(g)==SH_OK);alien_phase(g);CHECK(g->status.signal_remaining==1);CHECK(sh_end_phase(g)==SH_OK);
    CHECK(sh_end_phase(g)==SH_OK);CHECK(g->status.outcome==SH_MARINES_WIN);sh_destroy(g);
    g=fixture(3);objective(g,0,'H');objective(g,0,'L');
    CHECK(sh_action(g,act(SH_MOVE,9,10,-1,0))==SH_OK);CHECK(g->entity[0].ap==0);
    objective(g,0,'Z');objective(g,0,'R');objective(g,0,'Z');CHECK(g->status.rescued==2&&g->status.outcome==SH_MARINES_WIN);sh_destroy(g);
    g=fixture(4);objective(g,0,'1');objective(g,0,'2');objective(g,0,'3');objective(g,0,'X');CHECK(g->arrivals_stopped&&g->status.clock_round==1);
    objective(g,0,'Z');objective(g,1,'Z');objective(g,2,'Z');CHECK(g->status.evacuated==3&&g->status.outcome==SH_MARINES_WIN);sh_destroy(g);
    g=fixture(5);objective(g,0,'Q');objective(g,0,'1');objective(g,0,'Q');objective(g,0,'2');objective(g,0,'Q');objective(g,0,'3');
    CHECK(g->status.charges_left==0&&g->arrivals_stopped);objective(g,0,'Z');objective(g,1,'Z');CHECK(g->status.outcome==SH_MARINES_WIN);sh_destroy(g);
    g=fixture(6);objective(g,0,'L');objective(g,0,'H');CHECK(!g->alarm);objective(g,0,'R');objective(g,0,'H');CHECK(g->alarm);
    objective(g,0,'X');objective(g,0,'Z');CHECK(g->status.outcome==SH_MARINES_WIN);sh_destroy(g);
    g=fixture(7);id=enemy(g,9,10,SH_ALIEN,SH_BRUTE,0);objective(g,0,'L');objective(g,0,'R');CHECK(g->status.brute_tagged);
    dice(g,&d,6,6);CHECK(sh_action(g,act(SH_SHOOT,0,0,id,0))==SH_OK);CHECK(g->entity[id].wounds==1);
    CHECK(sh_action(g,act(SH_SHOOT,0,0,id,0))==SH_OK);CHECK(g->status.brute_dead);
    objective(g,0,'X');CHECK(g->status.outcome==SH_MARINES_WIN);sh_destroy(g);
    g=fixture(8);objective(g,0,'U');objective(g,0,'V');objective(g,0,'Q');objective(g,0,'X');
    station(g,0,'.');g->board[10][11]='Z';CHECK(sh_action(g,act(SH_INTERACT,0,0,-1,0))==SH_OK);CHECK(g->status.shield);
    station(g,0,'Z');g->entity[0].ap=0;CHECK(sh_action(g,act(SH_INTERACT,0,0,-1,0))==SH_OK);
    objective(g,1,'Z');objective(g,2,'Z');CHECK(g->status.outcome==SH_MARINES_WIN);sh_destroy(g);
    g=fixture(0);g->status.round=12;CHECK(sh_end_phase(g)==SH_OK);CHECK(g->status.outcome==SH_ALIENS_WIN&&g->status.phase==SH_FINISHED);sh_destroy(g);
    g=fixture(4);g->status.clock_round=1;g->status.round=4;CHECK(sh_end_phase(g)==SH_OK);CHECK(g->status.outcome==SH_ALIENS_WIN);sh_destroy(g);
}
static void test_optional(void) {
    SHOptions options;SHGame *g;Dice d;int a;
    memset(&options,0,sizeof options);options.hard_vacuum=options.training=1;options.specialist=SH_SKITTER;
    g=sh_create(0,7,&options,NULL,NULL);CHECK(g->entity[0].wounds==1&&g->status.deadline==14);sh_destroy(g);
    g=fixture(0);g->options.reliable_bursts=1;a=enemy(g,9,10,SH_ALIEN,SH_BASIC,0);dice(g,&d,1,6);
    CHECK(sh_activate(g,0)==SH_OK);CHECK(sh_action(g,act(SH_OVERWATCH,0,0,-1,0))==SH_OK);alien_phase(g);
    CHECK(sh_activate(g,a)==SH_OK);CHECK(sh_action(g,act(SH_MOVE,8,10,-1,0))==SH_OK);CHECK(sh_react(g,0,1)==SH_OK);CHECK(!g->entity[0].jammed&&!g->entity[a].alive);sh_destroy(g);
    g=fixture(0);g->options.sealed_bulkheads=1;g->board[9][10]='+';dice(g,&d,5,5);
    CHECK(sh_activate(g,0)==SH_OK);CHECK(sh_action(g,act(SH_BREACH,9,10,-1,0))==SH_OK);CHECK(g->board[9][10]=='.');sh_destroy(g);
    g=fixture(0);a=enemy(g,7,10,SH_ALIEN,SH_SPITTER,0);dice(g,&d,4,4);alien_phase(g);
    CHECK(sh_activate(g,a)==SH_OK);CHECK(g->entity[a].ap==5);CHECK(sh_action(g,act(SH_SHOOT,0,0,0,0))==SH_OK);CHECK(g->entity[0].wounds==1);sh_destroy(g);
}
static void choose_east(int contact,int strength,int row,int col,int positions[3][2],int *acting,void *user) {
    (void)contact;(void)strength;(void)user;
    positions[0][0]=row;positions[0][1]=col;
    positions[1][0]=row;positions[1][1]=col+1;
    *acting=1;
}
static void test_reveal_choices(void) {
    SHGame *g=fixture(0);Dice d;int a,b;
    g->board[9][10]='+';a=enemy(g,8,10,SH_BLIP,SH_BASIC,2);
    sh_set_reveal_policy(g,choose_east,NULL);
    CHECK(sh_activate(g,0)==SH_OK);CHECK(sh_action(g,act(SH_OVERWATCH,0,0,-1,0))==SH_OK);alien_phase(g);
    CHECK(sh_activate(g,a)==SH_OK);CHECK(sh_action(g,act(SH_DOOR,9,10,-1,0))==SH_OK);
    b=g->status.active;CHECK(b!=a&&g->entity[b].row==8&&g->entity[b].col==11);
    CHECK(g->entity[b].ap==5&&g->entity[a].ap==5);
    CHECK(g->status.pending_target==b);dice(g,&d,6,6);
    CHECK(sh_react(g,0,1)==SH_OK);CHECK(!g->entity[b].alive&&g->entity[a].alive);
    CHECK(sh_finish_reactions(g)==SH_OK);CHECK(sh_activate(g,a)==SH_OK);
    CHECK(sh_action(g,act(SH_MOVE,9,10,-1,0))==SH_OK);CHECK(g->status.pending_target==a);
    CHECK(sh_react(g,0,1)==SH_OK);CHECK(sh_finish_reactions(g)==SH_OK);sh_destroy(g);
    g=fixture(0);g->board[9][10]='+';a=enemy(g,8,10,SH_BLIP,SH_BASIC,1);b=enemy(g,7,10,SH_BLIP,SH_SKITTER,1);
    {int i;for(i=1;i<5;i++){g->entity[i].row=10+i;g->entity[i].col=10;}}
    alien_phase(g);CHECK(sh_activate(g,a)==SH_OK);CHECK(sh_action(g,act(SH_DOOR,9,10,-1,0))==SH_OK);
    CHECK(g->entity[a].team==SH_ALIEN&&g->entity[b].team==SH_BLIP);
    /* Kill the front blocker through a reaction, exposing an unactivated skitter. */
    g->entity[0].overwatch=1;g->status.pending_target=a;g->reaction_seen=0;dice(g,&d,6,6);
    CHECK(sh_react(g,0,1)==SH_OK);CHECK(sh_finish_reactions(g)==SH_OK);
    CHECK(g->entity[b].team==SH_ALIEN&&g->entity[b].ap==8);sh_destroy(g);
}
static void test_ghost_and_quarry(void) {
    SHOptions options;SHGame *g;Dice d;int a,b;
    memset(&options,0,sizeof options);options.ghost_contacts=1;
    g=sh_create(0,5,&options,NULL,NULL);g->status.round=3;
    CHECK(sh_end_phase(g)==SH_OK);CHECK(g->queue_last==3);CHECK(g->queue[2]==g->normal_contacts);
    CHECK(g->contacts[g->normal_contacts].strength==0);sh_destroy(g);
    g=fixture(0);a=enemy(g,8,10,SH_BLIP,SH_BASIC,0);CHECK(sh_activate(g,0)==SH_OK);
    CHECK(sh_action(g,act(SH_TURN,0,0,-1,SH_EAST))==SH_OK);CHECK(!g->entity[a].alive);sh_destroy(g);
    g=fixture(7);a=enemy(g,8,11,SH_ALIEN,SH_BRUTE,0);b=enemy(g,9,10,SH_ALIEN,SH_BRUTE,0);g->entity[b].quarry=0;
    dice(g,&d,6,6);CHECK(sh_activate(g,0)==SH_OK);
    CHECK(sh_action(g,act(SH_SHOOT,0,0,b,0))==SH_OK);CHECK(sh_action(g,act(SH_SHOOT,0,0,b,0))==SH_OK);
    CHECK(!g->entity[b].alive&&g->entity[a].alive&&!g->status.brute_dead);
    objective(g,0,'L');objective(g,0,'R');station(g,0,'X');
    CHECK(sh_action(g,act(SH_INTERACT,0,0,-1,0))==SH_INVALID);CHECK(g->entity[0].ap==4);sh_destroy(g);
}
int smoke_tests(void) {
    checks=failures=0;test_setup();test_movement_sight();test_overwatch();test_weapons_blips();test_items_deployment();test_objectives();test_optional();test_reveal_choices();test_ghost_and_quarry();
    printf("Smoke tests: %d checks, %d failures (actions, sight, overwatch, items, arrivals, all mission objectives).\n",checks,failures);
    return failures;
}
