#include "test.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {FILE *file;int verbose;unsigned count[SH_EVENT_DOOR+1];char ending[240];} Report;
static void feedback(const SHEvent *e,void *user) {
    Report *r=user;r->count[e->type]++;
    if(e->type==SH_EVENT_RESULT)snprintf(r->ending,sizeof r->ending,"%s",e->message);
    if(r->file) fprintf(r->file,"[round %02d] %s\n",e->round,e->message);
    if(r->verbose) printf("  [round %02d] %s\n",e->round,e->message);
}
static SHAction action(SHActionType type,int r,int c,int target,int option) {
    SHAction a;a.type=type;a.row=r;a.col=c;a.target=target;a.option=option;return a;
}
static int alive(const SHEntity *e) {return e->alive&&!e->extracted;}
static int dist(int r,int c,int y,int x) {return abs(r-y)+abs(c-x);}
static int at(const SHGame *g,int r,int c) {
    int i;SHEntity e;SHStatus s=sh_status(g);
    for(i=0;i<s.entities;i++) if(sh_entity(g,i,SH_MARINE,&e)&&alive(&e)&&e.row==r&&e.col==c) return i;
    return -1;
}
static int find(const SHGame *g,char label,int *r,int *c) {
    int y,x;for(y=0;y<SH_ROWS;y++) for(x=0;x<SH_COLS;x++) if(sh_tile(g,y,x)==label) {*r=y;*c=x;return 1;}return 0;
}
/* A public-state breadth-first planner. No board edits or hidden-strength reads. */
static int step(const SHGame *g,int actor,int goal_r,int goal_c,int *next_r,int *next_c) {
    int queue[SH_ROWS*SH_COLS],parent[SH_ROWS*SH_COLS],head=0,tail=0,i,start,end,v,n,r,c,nr,nc;
    static const int dy[]={-1,0,1,0},dx[]={0,1,0,-1};SHEntity e;
    sh_entity(g,actor,SH_MARINE,&e);start=e.row*SH_COLS+e.col;end=goal_r*SH_COLS+goal_c;
    for(i=0;i<SH_ROWS*SH_COLS;i++) parent[i]=-1;
    queue[tail++]=start;parent[start]=start;
    while(head<tail && parent[end]<0) {
        v=queue[head++];r=v/SH_COLS;c=v%SH_COLS;
        for(i=0;i<4;i++) {
            nr=r+dy[i];nc=c+dx[i];if(nr<0||nc<0||nr>=SH_ROWS||nc>=SH_COLS||(!sh_tile_is_floor(sh_tile(g,nr,nc))&&sh_tile(g,nr,nc)!=SH_CLOSED_DOOR)) continue;
            n=nr*SH_COLS+nc;if(parent[n]>=0||at(g,nr,nc)>=0) continue;
            parent[n]=v;queue[tail++]=n;
        }
    }
    if(parent[end]<0||start==end) return 0;
    v=end;while(parent[v]!=start) v=parent[v];*next_r=v/SH_COLS;*next_c=v%SH_COLS;return 1;
}
static int facing(int r,int c,int y,int x) {int dy=y-r,dx=x-c;return abs(dy)>=abs(dx)?(dy<0?SH_NORTH:SH_SOUTH):(dx>0?SH_EAST:SH_WEST);}
static void reactions(SHGame *g) {
    SHStatus s=sh_status(g);int i;SHEntity e;
    if(s.pending_target<0||s.outcome!=SH_ONGOING)return;
    for(i=0;i<5 && sh_status(g).outcome==SH_ONGOING;i++) {
        sh_entity(g,i,SH_MARINE,&e);
        if(e.overwatch&&sh_can_shoot(g,i,s.pending_target)) (void)sh_react(g,i,1);
    }
    if(sh_status(g).pending_target>=0) (void)sh_finish_reactions(g);
}
static int safe_blast(const SHGame *g,int r,int c) {
    int i;SHEntity e;for(i=0;i<5;i++) {sh_entity(g,i,SH_MARINE,&e);if(alive(&e)&&dist(r,c,e.row,e.col)<=1)return 0;}return 1;
}
static int defend(SHGame *g,int id) {
    int i,best=-1,range=999,d;SHEntity e,t;SHStatus s=sh_status(g);sh_entity(g,id,SH_MARINE,&e);
    for(i=5;i<s.entities;i++) {
        sh_entity(g,i,SH_MARINE,&t);if(!alive(&t)||t.team!=SH_ALIEN||!sh_can_see(g,id,t.row,t.col))continue;
        d=dist(e.row,e.col,t.row,t.col);if(d<range){range=d;best=i;}
    }
    if(best<0)return 0;sh_entity(g,best,SH_MARINE,&t);
    if(e.jammed && e.ap>=1) return sh_action(g,action(SH_CLEAR_JAM,0,0,-1,0))==SH_OK;
    if(e.weapon!=SH_FLAME&&sh_can_shoot(g,id,best)&&e.ap>=1) return sh_action(g,action(SH_SHOOT,0,0,best,0))==SH_OK;
    if(range<=4 && e.ap>=2 && safe_blast(g,t.row,t.col)) {
        SHActionType type=e.weapon==SH_FLAME&&e.ammunition>0?SH_SHOOT:e.frag?SH_FRAG:e.stun?SH_STUN:SH_MOVE;
        if(type!=SH_MOVE && sh_action(g,action(type,t.row,t.col,best,0))==SH_OK)return 1;
    }
    if(range==1 && e.ap>=1 && (e.ammunition==0||e.weapon==SH_FLAME)) return sh_action(g,action(SH_MELEE,0,0,best,0))==SH_OK;
    if(range<=10&&e.ap>=2&&e.facing!=(SHFacing)facing(e.row,e.col,t.row,t.col)) return sh_action(g,action(SH_TURN,0,0,-1,facing(e.row,e.col,t.row,t.col)))==SH_OK;
    return 0;
}
static char marine_goal(const SHGame *g,int mission,int id) {
    SHStatus s=sh_status(g);SHEntity e;SHItem item;unsigned f=s.objectives;sh_entity(g,id,SH_MARINE,&e);
    if(e.item>=0) {
        sh_item(g,e.item,&item);
        if(mission==5) return !(f&1)?'1':!(f&2)?'2':!(f&128)?'3':'Z';
        if(mission==6) return strstr(item.name,"sample")?'H':'Z';
        if(mission==8) return 'X';
        if(mission==2) return 'X';
        return 'Z';
    }
    switch(mission) {
    case 0:case 2:case 8:
        if(!(f&1) && (id==0 || (f&2)))return 'U';
        if(!(f&2) && (id==4 || (f&1)))return 'V';
        if(mission==8) {if(s.clock_round)return 'Z';if(id==3)return 'Q';}
        if(mission==2 && id==3 && !s.clock_round)return 'Q';
        if((f&3)==3)return 'X';break;
    case 1:if(!(f&4))return id==0?'P':'.';return id==0||id==4?'R':'.';
    case 3:if(!(f&4))return id==2?'H':'.';return id==0?'L':id==4?'R':'.';
    case 4:
        if(s.clock_round)return 'Z';
        if(id==0)return !(f&1)?'1':'Z';if(id==4)return !(f&128)?'3':'Z';if(id==1)return !(f&2)?'2':'Z';
        if(id==2&&(f&131)==131)return 'X';break;
    case 5:
        if(s.clock_round)return 'Z';if(s.charges_left && (id==0||id==1||id==4))return 'Q';break;
    case 6:
        if((f&96)==96)return id==2||id==0?'X':'.';
        return id==0&&!(f&32)?'L':id==4&&!(f&64)?'R':'.';
    case 7:if(id==0&&!(f&1))return 'L';if(id==4&&!(f&2))return 'R';if((f&3)==3&&s.brute_dead&&s.brute_tagged)return 'X';break;
    default:break;
    }
    return '.';
}
static int inspect_target(const SHGame *g) {
    SHStatus s=sh_status(g);SHEntity e;int i;for(i=5;i<s.entities;i++){sh_entity(g,i,SH_MARINE,&e);if(alive(&e)&&e.team==SH_BLIP)return i;}return -1;
}
static void marine_turn(SHGame *g,int mission,int id) {
    int attempts=0,r,c,y,x;char goal;SHEntity e;SHItem item;
    if(sh_activate(g,id)!=SH_OK)return;
    while(sh_status(g).active==id && sh_status(g).outcome==SH_ONGOING && attempts++<20) {
        sh_entity(g,id,SH_MARINE,&e);if(e.ap==0) {
            if(mission==8&&sh_tile(g,e.row,e.col)=='Z') (void)sh_action(g,action(SH_INTERACT,0,0,-1,0));break;
        }
        if(defend(g,id))continue;
        /* Recover dropped mission items rather than abandoning the objective. */
        if(e.item<0) {
            int k;for(k=0;sh_item(g,k,&item);k++) if(item.holder==-1&&item.dropped&&item.row==e.row&&item.col==e.col) {
                if(sh_action(g,action(SH_PICKUP,0,0,-1,k))==SH_OK)break;
            }
            sh_entity(g,id,SH_MARINE,&e);
        }
        goal=marine_goal(g,mission,id);
        if(goal!='.'&&find(g,goal,&r,&c)) {
            if(e.row==r&&e.col==c) {
                if(sh_action(g,action(SH_INTERACT,0,0,inspect_target(g),1))==SH_OK)continue;
            } else if(step(g,id,r,c,&y,&x)) {
                if(sh_tile(g,y,x)==SH_CLOSED_DOOR) {
                    if(sh_action(g,action(SH_DOOR,y,x,-1,0))==SH_OK)continue;
                } else {
                    int want=facing(e.row,e.col,y,x);
                    if(want!=(int)e.facing&&e.ap>=3 && want==((int)e.facing+2)%4) {
                        if(sh_action(g,action(SH_TURN,0,0,-1,want))==SH_OK)continue;
                    }
                    if(sh_action(g,action(SH_MOVE,y,x,-1,0))==SH_OK)continue;
                }
            }
        } else {
            /* Separate guard stations keep objectives and the extraction hatch free. */
            r=15;c=id==1?9:id==3?21:15;
            if(e.row!=r||e.col!=c) if(step(g,id,r,c,&y,&x)) {
                if(sh_action(g,action(sh_tile(g,y,x)==SH_CLOSED_DOOR?SH_DOOR:SH_MOVE,y,x,-1,0))==SH_OK)continue;
            }
        }
        if(e.jammed&&e.ap>=1) {if(sh_action(g,action(SH_CLEAR_JAM,0,0,-1,0))==SH_OK)continue;}
        if(e.ap>=2) (void)sh_action(g,action(SH_OVERWATCH,0,0,-1,0));
        break;
    }
    if(sh_status(g).active==id) (void)sh_end_activation(g);
}
static void alien_turn(SHGame *g,int id) {
    int i,best,r,c,y,x,attempts=0,d,minimum;SHEntity e,m;
    if(sh_activate(g,id)!=SH_OK)return;
    while(sh_status(g).active==id&&sh_status(g).outcome==SH_ONGOING&&attempts++<30) {
        sh_entity(g,id,SH_ALIEN,&e);if(e.ap<=0)break;best=-1;minimum=999;
        for(i=0;i<5;i++){sh_entity(g,i,SH_ALIEN,&m);if(!alive(&m))continue;d=dist(e.row,e.col,m.row,m.col);if(d<minimum){minimum=d;best=i;}}
        if(best<0)break;sh_entity(g,best,SH_ALIEN,&m);
        if(e.team==SH_ALIEN&&minimum==1) {
            if(sh_action(g,action(SH_MELEE,0,0,best,0))==SH_OK){reactions(g);continue;}
        }
        if(e.team==SH_ALIEN&&e.bug==SH_SPITTER&&minimum<=4&&e.ap>=2&&sh_can_see(g,id,m.row,m.col)) {
            if(sh_action(g,action(SH_SHOOT,0,0,best,0))==SH_OK){reactions(g);continue;}
        }
        /* Goal is an empty square adjacent to the prey, never its occupied square. */
        r=c=-1;
        for(i=0;i<4;i++) {
            static const int dy[]={-1,0,1,0},dx[]={0,1,0,-1};
            int gr=m.row+dy[i],gc=m.col+dx[i];
            if(gr<0||gc<0||gr>=SH_ROWS||gc>=SH_COLS||(!sh_tile_is_floor(sh_tile(g,gr,gc))&&sh_tile(g,gr,gc)!=SH_CLOSED_DOOR)||at(g,gr,gc)>=0)continue;
            if(step(g,id,gr,gc,&y,&x)){r=y;c=x;break;}
        }
        if(r<0)break;
        if(sh_action(g,action(sh_tile(g,r,c)==SH_CLOSED_DOOR?SH_DOOR:SH_MOVE,r,c,-1,0))!=SH_OK)break;
        reactions(g);
    }
    if(sh_status(g).active==id)(void)sh_end_activation(g);
}
static int valid_state(const SHGame *g) {
    SHStatus s=sh_status(g);
    int i,j,owners[8]={0};
    SHEntity e,other;
    SHItem item;
    if(s.entities<5 || s.entities>SH_MAX_ENTITIES || s.round<1 || s.round>s.deadline) return 0;
    for(i=0;i<s.entities;i++) {
        if(!sh_entity(g,i,SH_MARINE,&e) || e.id!=i || e.ap<0) return 0;
        if(!alive(&e)) continue;
        if(e.wounds<=0 || e.row<0 || e.row>=SH_ROWS || e.col<0 || e.col>=SH_COLS || !sh_tile_is_floor(sh_tile(g,e.row,e.col))) return 0;
        if(e.team==SH_MARINE && (e.ammunition < -1 || e.frag<0 || e.stun<0)) return 0;
        for(j=0;j<i;j++) {
            sh_entity(g,j,SH_MARINE,&other);
            if(alive(&other)&&e.row==other.row&&e.col==other.col)return 0;
        }
        if(e.item>=0) {
            if(e.item>=8 || ++owners[e.item]>1 || !sh_item(g,e.item,&item) || item.holder!=i) return 0;
        }
    }
    for(i=0;sh_item(g,i,&item);i++) {
        if(item.holder>=0 && (item.holder>=5 || !sh_entity(g,item.holder,SH_MARINE,&e) || !alive(&e) || e.item!=i)) return 0;
    }
    return 1;
}
int run_scenario(int mission,unsigned seed,int verbose,const char *path) {
    Report report;SHGame *g;SHStatus s;int i,iteration=0,failed=0;SHEntity e;char board[900];
    memset(&report,0,sizeof report);report.verbose=verbose;report.file=fopen(path,"w");
    if(!report.file){fprintf(stderr,"Cannot write report %s; run from the repository root after make.\n",path);return 1;}
    g=sh_create(mission,seed,NULL,feedback,&report);if(!g){fclose(report.file);return 1;}
    printf("\n[%d] %s (seed %u)\n    %s\n",mission,sh_scenario(mission)->name,seed,sh_scenario(mission)->objective);
    while((s=sh_status(g)).outcome==SH_ONGOING && iteration++<100) {
        if(!valid_state(g)){fprintf(stderr,"Scenario %d violates a game-state invariant.\n",mission);failed=1;break;}
        if(s.phase==SH_MARINE_PHASE) {
            for(i=0;i<5 && sh_status(g).outcome==SH_ONGOING;i++) marine_turn(g,mission,(i+s.round)%5);
            if(sh_status(g).outcome==SH_ONGOING && sh_end_phase(g)!=SH_OK){failed=1;break;}
        } else if(s.phase==SH_DEPLOYMENT) {
            int cycle;for(cycle=0;cycle<2;cycle++) for(i=0;i<6;i++) (void)sh_deploy(g,(char)('A'+(i+s.round)%6));
            if(sh_finish_deployment(g)!=SH_OK){failed=1;break;}
        } else if(s.phase==SH_ALIEN_PHASE) {
            /* Re-read the entity count: a reveal may create more activations. */
            for(i=5;i<sh_status(g).entities&&sh_status(g).outcome==SH_ONGOING;i++) {
                sh_entity(g,i,SH_ALIEN,&e);if(alive(&e)&&!e.done)alien_turn(g,i);
            }
            if(sh_status(g).outcome==SH_ONGOING&&sh_end_phase(g)!=SH_OK){failed=1;break;}
        } else {failed=1;break;}
    }
    s=sh_status(g);if(s.outcome==SH_ONGOING || !valid_state(g))failed=1;
    printf("    %s in round %d. Movement %u; weapon events %u; overwatch events %u; jams %u; objectives %u.\n    Full feedback: %s\n",failed?"RUN FAILED":s.outcome==SH_MARINES_WIN?"Marines win":"Aliens win",s.round,report.count[SH_EVENT_MOVEMENT],report.count[SH_EVENT_WEAPON],report.count[SH_EVENT_OVERWATCH],report.count[SH_EVENT_JAM],report.count[SH_EVENT_OBJECTIVE],path);
    printf("    Ending: %s\n",report.ending[0]?report.ending:"no legal ending reached");
    for(i=0;i<5;i++) {
        sh_entity(g,i,SH_MARINE,&e);
        fprintf(report.file,"Marine %d: alive=%d evacuated=%d wounds=%d ammo=%d frag=%d stun=%d item=%d\n",i,e.alive,e.extracted,e.wounds,e.ammunition,e.frag,e.stun,e.item);
    }
    sh_board(g,board,sizeof board);fprintf(report.file,"\nFinal board:\n%s\nOutcome=%d Round=%d Objectives=%u Rescued=%d Evacuated=%d\n",board,s.outcome,s.round,s.objectives,s.rescued,s.evacuated);
    if(failed)fprintf(stderr,"Scenario %d stalled: %s\n",mission,sh_last_error(g));
    sh_destroy(g);if(fclose(report.file)!=0)failed=1;return failed;
}
