#include "internal.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int dr[4] = {-1,0,1,0}, dc[4] = {0,1,0,-1};
static const char *names[5] = {"Vale","Iona","Kes","Rook","Sen"};
static void visibility(SHGame *g);
static void check_result(SHGame *g, int deadline);
static void melee(SHGame *g, int attacker, int defender);
static int reveal(SHGame *g, int id);
static int base_ap(const SHEntity *e) {
    if (e->team == SH_MARINE) return 4;
    if (e->team == SH_BLIP) return 6;
    return e->bug == SH_SKITTER ? 8 : e->bug == SH_BRUTE ? 4 : e->bug == SH_SPITTER ? 5 : 6;
}
static int valid(int r,int c) { return r>=0 && r<SH_ROWS && c>=0 && c<SH_COLS; }
static int distance(int r,int c,int y,int x) { return abs(r-y)+abs(c-x); }
static int occupied(const SHGame *g,int r,int c) {
    int i;
    for(i=0;i<g->status.entities;i++) if(g->entity[i].alive && !g->entity[i].extracted && g->entity[i].row==r && g->entity[i].col==c) return i;
    return -1;
}
static int entity_ok(const SHGame *g,int i) { return g && i>=0 && i<g->status.entities; }
static const char *name_of(int id) { return id>=0 && id<5 ? names[id] : "contact"; }
static void emit(SHGame *g,SHEventType type,int actor,int target,const char *fmt,...) {
    SHEvent event; va_list args;
    memset(&event,0,sizeof event); event.type=type; event.round=g->status.round;
    event.actor=actor; event.target=target; event.row=event.col=-1;
    if(entity_ok(g,actor)) { event.row=g->entity[actor].row;event.col=g->entity[actor].col;event.ap=g->entity[actor].ap; }
    va_start(args,fmt);vsnprintf(event.message,sizeof event.message,fmt,args);va_end(args);
    if(g->feedback) g->feedback(&event,g->feedback_user);
}
static SHResult fail(SHGame *g,SHResult result,const char *message) {
    if(g) snprintf(g->error,sizeof g->error,"%s",message);
    return result;
}
static uint32_t random_number(SHGame *g) {
    uint32_t x=g->rng; x^=x<<13; x^=x>>17; x^=x<<5; g->rng=x;return x;
}
static int roll(SHGame *g) {
    int value=g->dice ? g->dice(g->dice_user) : (int)(random_number(g)%6)+1;
    return value>=1 && value<=6 ? value : 1;
}
const char *sh_weapon_name(SHWeapon w) {
    static const char *labels[]={"boarding rifle","scattergun","rotary cannon","flame projector"};
    return w>=SH_RIFLE && w<=SH_FLAME ? labels[w] : "unknown weapon";
}
const char *sh_bug_name(SHBug bug) {
    static const char *labels[]={"basic alien","skitter","brute","spitter"};
    return bug>=SH_BASIC && bug<=SH_SPITTER ? labels[bug] : "unknown bug";
}
const char *sh_result_name(SHResult r) {
    static const char *labels[]={"OK","invalid action","wrong phase","insufficient AP","blocked","not visible","empty weapon","jammed weapon","reaction pending","game over"};
    return r>=SH_OK && r<=SH_GAME_OVER ? labels[r] : "unknown result";
}
const char *sh_last_error(const SHGame *g) { return g?g->error:"null game"; }
char sh_tile(const SHGame *g,int r,int c) { return g && valid(r,c)?g->board[r][c]:SH_VOID; }
int sh_tile_is_wall(char tile) {return tile=='+'||tile=='|'||tile=='-'||tile=='#';}
int sh_tile_is_floor(char tile) {return tile=='.'||tile==SH_OPEN_DOOR||(tile>='A'&&tile<='Z')||(tile>='1'&&tile<='3');}
SHStatus sh_status(const SHGame *g) { SHStatus empty;memset(&empty,0,sizeof empty);return g?g->status:empty; }
int sh_entity(const SHGame *g,int id,SHTeam viewer,SHEntity *out) {
    if(!entity_ok(g,id)||!out) return 0;
    *out=g->entity[id];
    if(out->team==SH_BLIP && viewer==SH_MARINE) {
        /* Inspection reveals private contact information without placing aliens. */
        if(!out->inspected) { out->strength=-1;out->bug=SH_BASIC;out->quarry=0; }
    }
    return 1;
}
int sh_item(const SHGame *g,int id,SHItem *out) {
    if(!g||!out||id<0||id>=g->item_count) return 0;
    *out=g->items[id].view;return 1;
}
void sh_destroy(SHGame *g) { free(g); }
void sh_set_dice(SHGame *g,SHDice dice,void *user) { if(g) {g->dice=dice;g->dice_user=user;} }
void sh_set_reveal_policy(SHGame *g,SHRevealPolicy policy,void *user) { if(g) {g->reveal_policy=policy;g->reveal_user=user;} }
static int blocker(const SHGame *g,int r,int c,int from,int target) {
    int id;
    if(!valid(r,c)||!sh_tile_is_floor(g->board[r][c])) return 1;
    id=occupied(g,r,c);return id>=0 && id!=from && id!=target;
}
int sh_can_see(const SHGame *g,int from,int row,int col) {
    const SHEntity *e; int r,c,nr,nc,ir=0,ic=0,sr,sc,target;
    if(!entity_ok(g,from)||!valid(row,col)||!g->entity[from].alive||g->entity[from].extracted) return 0;
    if(!sh_tile_is_floor(g->board[row][col])) return 0;
    e=&g->entity[from];r=e->row;c=e->col;nr=abs(row-r);nc=abs(col-c);
    sr=row>r?1:-1;sc=col>c?1:-1;target=occupied(g,row,col);
    while(ir<nr || ic<nc) {
        long a=(long)(1+2*ic)*nr,b=(long)(1+2*ir)*nc;
        if(a==b) {
            if(blocker(g,r+sr,c,from,target)||blocker(g,r,c+sc,from,target)) return 0;
            r+=sr;c+=sc;ir++;ic++;
        } else if(a<b) { c+=sc;ic++; } else { r+=sr;ir++; }
        if(blocker(g,r,c,from,target)) return 0;
    }
    return 1;
}
static int arc(const SHEntity *e,int r,int c) { return (r-e->row)*dr[e->facing]+(c-e->col)*dc[e->facing]>=0; }
int sh_marine_visible(const SHGame *g,int row,int col) {
    int i;
    if(!g||!valid(row,col)) return 0;
    for(i=0;i<g->status.entities;i++) {
        const SHEntity *e=&g->entity[i];
        if(e->team==SH_MARINE&&e->alive&&!e->extracted&&arc(e,row,col)&&sh_can_see(g,i,row,col)) return 1;
    }
    return 0;
}
char sh_view_tile(const SHGame *g,int row,int col,SHTeam viewer) {
    char tile=sh_tile(g,row,col);
    if(viewer!=SH_MARINE||tile==SH_VOID||sh_tile_is_wall(tile)||sh_marine_visible(g,row,col)) return tile;
    /* A blocking door cannot be a sight destination: show its face when a
       Marine can see the adjacent approach square in the same front arc. */
    if(tile==SH_CLOSED_DOOR) {
        int i,d;
        for(i=0;i<g->status.entities;i++) {
            const SHEntity *e=&g->entity[i];
            if(e->team!=SH_MARINE||!e->alive||e->extracted||!arc(e,row,col))continue;
            for(d=0;d<4;d++)if(sh_can_see(g,i,row+dr[d],col+dc[d]))return tile;
        }
    }
    return '?';
}
int sh_observed_entity(const SHGame *g,int id,SHTeam viewer,SHEntity *out) {
    if(!sh_entity(g,id,viewer,out))return 0;
    if(viewer==SH_MARINE&&out->team!=SH_MARINE&&!sh_marine_visible(g,out->row,out->col)) {
        out->team=SH_BLIP;out->strength=-1;out->bug=SH_BASIC;out->quarry=0;
        out->weapon=SH_RIFLE;out->wounds=0;out->ap=0;out->facing=SH_NORTH;
        out->ammunition=-1;out->frag=0;out->stun=0;out->overwatch=0;
        out->jammed=0;out->stunned=0;out->item=-1;out->inspected=0;
    }
    return 1;
}
static int weapon_range(SHWeapon w) { return w==SH_RIFLE?8:w==SH_SCATTERGUN?4:w==SH_CANNON?10:4; }
int sh_can_shoot(const SHGame *g,int from,int target) {
    const SHEntity *e,*t;
    if(!entity_ok(g,from)||!entity_ok(g,target)) return 0;
    e=&g->entity[from];t=&g->entity[target];
    if(!e->alive||e->extracted||!t->alive||t->extracted||e->team!=SH_MARINE||t->team!=SH_ALIEN||e->jammed||e->ammunition==0) return 0;
    return arc(e,t->row,t->col) && distance(e->row,e->col,t->row,t->col)<=weapon_range(e->weapon) && sh_can_see(g,from,t->row,t->col);
}
static int add_entity(SHGame *g,SHTeam team,int r,int c,SHBug bug,int strength) {
    SHEntity *e;int id=g->status.entities;
    if(id>=SH_MAX_ENTITIES) return -1;
    e=&g->entity[id];memset(e,0,sizeof *e);e->id=id;e->team=team;e->row=r;e->col=c;e->alive=1;
    e->bug=bug;e->strength=strength;e->wounds=team==SH_ALIEN && bug==SH_BRUTE?3:1;
    e->item=-1;e->ammunition=-1;e->ap=base_ap(e);g->status.entities++;return id;
}
static void add_item(SHGame *g,int type,char label,const char *name) {
    SHStoredItem *item;int r,c,id=g->item_count++;
    item=&g->items[id];memset(item,0,sizeof *item);item->type=type;item->view.id=id;item->view.holder=-1;item->view.name=name;
    for(r=0;r<SH_ROWS;r++) for(c=0;c<SH_COLS;c++) if(g->board[r][c]==label) {item->view.row=r;item->view.col=c;return;}
}
SHGame *sh_create(int scenario,uint32_t seed,const SHOptions *options,SHFeedback callback,void *user) {
    SHGame *g;int r,c,i,j,n=0,k=0;SHContact temp;const SHScenario *s=sh_scenario(scenario);
    if(!s) return NULL;
    g=calloc(1,sizeof *g);if(!g) return NULL;
    g->scenario=s;if(options) g->options=*options;g->rng=seed?seed:0x6d2b79f5u;
    g->feedback=callback;g->feedback_user=user;g->status.round=1;g->status.active=-1;
    g->status.pending_target=-1;g->status.deadline=s->deadline+(g->options.training?2:0);
    g->status.signal_remaining=3;g->status.charges_left=3;
    for(r=0;r<SH_ROWS;r++) for(c=0;c<SH_COLS;c++) g->board[r][c]=s->map[r][c];
    for(r=0;r<SH_ROWS;r++) for(c=0;c<SH_COLS;c++) if(g->board[r][c]=='M') {
        int id=add_entity(g,SH_MARINE,r,c,SH_BASIC,0);SHEntity *e=&g->entity[id];
        e->wounds=g->options.hard_vacuum?1:2;e->weapon=(SHWeapon)(k==4?0:k);e->ammunition=e->weapon==SH_CANNON?8:e->weapon==SH_FLAME?4:-1;e->ap=4;e->frag=e->stun=1;k++;
    }
    for(i=0;i<3;i++) for(j=0;j<s->pool[i];j++) {g->contacts[n].strength=i+1;g->contacts[n++].bug=SH_BASIC;}
    g->normal_contacts=g->contact_count=n;
    if(scenario==7) { /* Brute is guaranteed to be among the starting contacts. */
        for(i=0;i<n;i++) if(g->contacts[i].strength==1) {g->contacts[i].bug=SH_BRUTE;g->contacts[i].quarry=1;break;}
    }
    if(g->options.specialist>SH_BASIC && g->options.specialist<=SH_SPITTER) {
        for(i=0;i<n;i++) if(g->contacts[i].strength==1 && g->contacts[i].bug==SH_BASIC) {g->contacts[i].bug=g->options.specialist;break;}
    }
    for(i=n-1;i>0;i--) {j=(int)(random_number(g)%(uint32_t)(i+1));temp=g->contacts[i];g->contacts[i]=g->contacts[j];g->contacts[j]=temp;}
    if(scenario==7) for(i=0;i<n;i++) if(g->contacts[i].quarry) {j=(int)(random_number(g)%(uint32_t)s->starting);temp=g->contacts[j];g->contacts[j]=g->contacts[i];g->contacts[i]=temp;break;}
    if(g->options.ghost_contacts) {g->contacts[n].strength=0;g->contacts[n].bug=SH_BASIC;g->contact_count++;}
    for(i=0;i<s->starting;i++) {
        char label=(char)('A'+i);
        for(r=0;r<SH_ROWS;r++) for(c=0;c<SH_COLS;c++) if(g->board[r][c]==label) {int contact=add_entity(g,SH_BLIP,r,c,g->contacts[i].bug,g->contacts[i].strength);g->entity[contact].quarry=g->contacts[i].quarry;}
    }
    g->scheduled=s->starting;
    switch(scenario) {
    case 1:add_item(g,ITEM_RECORDER,'R',"flight recorder");break;
    case 2:add_item(g,ITEM_CAPACITOR,'Q',"reserve capacitor");break;
    case 3:add_item(g,ITEM_CAPSULE_L,'L',"left rescue capsule");add_item(g,ITEM_CAPSULE_R,'R',"right rescue capsule");break;
    case 5:for(i=0;i<3;i++) add_item(g,ITEM_CHARGE,'Q',"demolition charge");break;
    case 6:add_item(g,ITEM_SAMPLE_L,'L',"left sample");add_item(g,ITEM_SAMPLE_R,'R',"right sample");add_item(g,ITEM_LEDGER,'X',"crew ledger");break;
    case 8:add_item(g,ITEM_KEY,'Q',"authorization key");break;
    default:break;
    }
    emit(g,SH_EVENT_PHASE,-1,-1,"Mission %d: %s. Marine phase 1; deadline phase %d.",scenario,s->name,g->status.deadline);
    visibility(g);return g;
}
static void outcome(SHGame *g,SHOutcome result,const char *reason) {
    if(g->status.outcome!=SH_ONGOING) return;
    g->status.outcome=result;g->status.phase=SH_FINISHED;g->status.active=-1;g->status.pending_target=-1;
    emit(g,SH_EVENT_RESULT,-1,-1,"%s: %s",result==SH_MARINES_WIN?"Marines win":"Aliens win",reason);
}
static void check_result(SHGame *g,int deadline) {
    int i,live=0,minimum=g->scenario->id==4||g->scenario->id==8?3:g->scenario->id==5?2:0;
    for(i=0;i<5;i++) if(g->entity[i].alive && !g->entity[i].extracted) live++;
    if(g->status.outcome!=SH_ONGOING) return;
    if(minimum && live+g->status.evacuated<minimum) {outcome(g,SH_ALIENS_WIN,"too few Marines remain to meet the evacuation requirement");return;}
    if(!live) {outcome(g,SH_ALIENS_WIN,"no Marine remains aboard");return;}
    if(deadline) {
        int last=g->status.deadline,span=g->scenario->id==4?3:g->scenario->id==5?2:g->scenario->id==8?4:0;
        if(span && g->status.clock_round && g->status.clock_round+span<last) last=g->status.clock_round+span;
        if(g->status.round>=last) outcome(g,SH_ALIENS_WIN,"the objective or evacuation deadline expired");
    }
}
static void damage(SHGame *g,int id,int wounds,int attacker) {
    SHEntity *e;if(!entity_ok(g,id)||!g->entity[id].alive||g->entity[id].extracted) return;
    e=&g->entity[id];e->wounds-=wounds;
    emit(g,SH_EVENT_DAMAGE,attacker,id,"%s %d takes %d wound(s); %d remain.",e->team==SH_MARINE?"Marine":"Alien",id,wounds,e->wounds>0?e->wounds:0);
    if(e->wounds<=0) {
        e->wounds=0;e->alive=0;e->overwatch=0;e->done=1;
        if(e->item>=0) {SHItem *item=&g->items[e->item].view;item->holder=-1;item->row=e->row;item->col=e->col;item->dropped=1;e->item=-1;}
        if(e->team==SH_ALIEN && e->quarry && g->scenario->id==7) g->status.brute_dead=1;
        if(g->status.active==id) g->status.active=-1;
        emit(g,SH_EVENT_DAMAGE,attacker,id,"%s %d removed from the deck.",e->team==SH_MARINE?"Marine":"Alien",id);
    }
    check_result(g,0);
}
static int reveal(SHGame *g,int id) {
    SHEntity saved;
    int i,j,d,next,remaining,active,chosen=0,positions[3][2],ids[3]={-1,-1,-1},invalid=0;
    if(!entity_ok(g,id)||g->entity[id].team!=SH_BLIP||!g->entity[id].alive) return id;
    saved=g->entity[id];active=g->status.active==id;
    if(saved.strength==0) {
        g->entity[id].alive=0;g->entity[id].done=1;
        if(active) g->status.active=-1;
        emit(g,SH_EVENT_REVEAL,id,-1,"Blip %d was a ghost contact; signal cleared.",id);return -1;
    }
    for(i=0;i<3;i++) positions[i][0]=positions[i][1]=-1;
    positions[0][0]=saved.row;positions[0][1]=saved.col;j=1;
    for(d=0;d<4 && j<saved.strength;d++) {
        int r=saved.row+dr[d],c=saved.col+dc[d];
        if(valid(r,c)&&sh_tile_is_floor(sh_tile(g,r,c))&&occupied(g,r,c)<0) {positions[j][0]=r;positions[j++][1]=c;}
    }
    if(g->reveal_policy) {
        int defaults[3][2],survivors=j;memcpy(defaults,positions,sizeof defaults);
        g->reveal_policy(id,saved.strength,saved.row,saved.col,positions,&chosen,g->reveal_user);
        if(positions[0][0]!=saved.row||positions[0][1]!=saved.col||chosen<0||chosen>=saved.strength) invalid=1;
        j=1;
        for(i=1;i<saved.strength;i++) {
            int r=positions[i][0],c=positions[i][1],k;
            if(r==-1&&c==-1) continue;
            if(!valid(r,c)||distance(saved.row,saved.col,r,c)!=1||!sh_tile_is_floor(sh_tile(g,r,c))||occupied(g,r,c)>=0) invalid=1;
            for(k=0;k<i;k++) if(positions[k][0]==r&&positions[k][1]==c) invalid=1;
            j++;
        }
        if(j!=survivors || (chosen>=0&&chosen<3&&positions[chosen][0]<0)) invalid=1;
        if(invalid) {memcpy(positions,defaults,sizeof positions);chosen=0;emit(g,SH_EVENT_REVEAL,id,-1,"Invalid reveal plan; using legal default placement.");}
    }
    g->entity[id].team=SH_ALIEN;g->entity[id].wounds=saved.bug==SH_BRUTE?3:1;g->entity[id].ammunition=-1;
    remaining=saved.done?0:active?saved.ap:base_ap(&g->entity[id])-(saved.stunned?2:0);
    if(remaining>base_ap(&g->entity[id]))remaining=base_ap(&g->entity[id]);
    if(remaining<0)remaining=0;
    g->entity[id].ap=remaining;ids[0]=id;
    for(i=1;i<saved.strength;i++) {
        if(positions[i][0]<0) {emit(g,SH_EVENT_REVEAL,id,-1,"Cramped reveal loses an excess alien.");continue;}
        next=add_entity(g,SH_ALIEN,positions[i][0],positions[i][1],saved.bug,0);
        if(next<0) {emit(g,SH_EVENT_REVEAL,id,-1,"Capacity exhausted during reveal.");continue;}
        ids[i]=next;g->entity[next].ap=remaining;g->entity[next].done=saved.done;g->entity[next].stunned=saved.stunned;
    }
    if(active)g->status.active=ids[chosen]>=0?ids[chosen]:id;
    emit(g,SH_EVENT_REVEAL,id,-1,"Blip %d reveals %d alien(s), profile %s, with %d AP remaining; continuing entity %d.",id,saved.strength,sh_bug_name(saved.bug),remaining,active?g->status.active:id);
    return active?g->status.active:id;
}
static void visibility(SHGame *g) {
    int i,m,changed;
    do {
        changed=0;
        for(i=5;i<g->status.entities;i++) if(g->entity[i].alive && g->entity[i].team==SH_BLIP) {
            for(m=0;m<5;m++) if(g->entity[m].alive && !g->entity[m].extracted && arc(&g->entity[m],g->entity[i].row,g->entity[i].col)&&sh_can_see(g,m,g->entity[i].row,g->entity[i].col)) {reveal(g,i);changed=1;break;}
        }
    } while(changed);
}
static void shoot(SHGame *g,int from,int target,int reaction) {
    char dice_text[32],ammo_text[32];
    SHEntity *e=&g->entity[from],*t=&g->entity[target];int dice[3],n=e->weapon==SH_RIFLE?2:3,i,hits=0,ones=0,hit=5;
    if(e->ammunition>0) e->ammunition--;
    if(e->weapon==SH_SCATTERGUN && distance(e->row,e->col,t->row,t->col)<=2) hit=4;
    for(i=0;i<n;i++) {dice[i]=roll(g);if(dice[i]==1) ones++;if(dice[i]>=hit) hits++;}
    if(n==2)snprintf(dice_text,sizeof dice_text,"%d,%d",dice[0],dice[1]);
    else snprintf(dice_text,sizeof dice_text,"%d,%d,%d",dice[0],dice[1],dice[2]);
    if(e->ammunition<0)snprintf(ammo_text,sizeof ammo_text,"unlimited");
    else snprintf(ammo_text,sizeof ammo_text,"%d",e->ammunition);
    emit(g,reaction?SH_EVENT_OVERWATCH:SH_EVENT_WEAPON,from,target,"Marine weapons use: %s fires %s at alien %d; dice %s; %d raw hit(s), ammunition %s.",name_of(from),sh_weapon_name(e->weapon),target,dice_text,hits,ammo_text);
    if(reaction && ones>=(g->options.reliable_bursts?2:1)) {
        e->jammed=1;e->overwatch=0;emit(g,SH_EVENT_JAM,from,target,"Overwatch jam: %s's entire burst fails; clear for 1 AP next activation.",name_of(from));return;
    }
    if(hits) damage(g,target,hits,from);
}
static void open_reactions(SHGame *g,int actor,int attack,int victim) {
    int m,eligible=0;
    if(g->status.outcome!=SH_ONGOING) return;
    if(!entity_ok(g,actor)||!g->entity[actor].alive||g->entity[actor].team!=SH_ALIEN) return;
    for(m=0;m<5;m++) if(g->entity[m].overwatch && sh_can_shoot(g,m,actor)) eligible=1;
    if(eligible) {
        g->status.pending_target=actor;g->pending_attack=attack;g->pending_victim=victim;g->reaction_seen=0;
        emit(g,SH_EVENT_OVERWATCH,actor,-1,"Overwatch window: alien %d acted; Marines may respond before its attack resolves.",actor);
    } else if(attack==1) melee(g,actor,victim);
    else if(attack==2) {
        int d=roll(g);emit(g,SH_EVENT_WEAPON,actor,victim,"Spitter %d fires; die %d.",actor,d);if(d>=4) damage(g,victim,1,actor);
    }
}
SHResult sh_react(SHGame *g,int marine,int fire) {
    int target;
    if(!g) return SH_INVALID;
    if(g->status.outcome!=SH_ONGOING) return fail(g,SH_GAME_OVER,"mission finished");
    target=g->status.pending_target;
    if(target<0) return fail(g,SH_WRONG_PHASE,"no reaction window");
    if(marine<0||marine>=5||(g->reaction_seen&(1u<<marine))||!g->entity[marine].overwatch||!sh_can_shoot(g,marine,target)) return fail(g,SH_INVALID,"Marine has no eligible unused reaction");
    g->reaction_seen|=1u<<marine;
    if(fire) shoot(g,marine,target,1);else emit(g,SH_EVENT_OVERWATCH,marine,target,"%s declines overwatch.",name_of(marine));
    return SH_OK;
}
SHResult sh_finish_reactions(SHGame *g) {
    int actor,attack,victim;
    if(!g) return SH_INVALID;
    if(g->status.outcome!=SH_ONGOING) return SH_GAME_OVER;
    actor=g->status.pending_target;if(actor<0) return fail(g,SH_WRONG_PHASE,"no reaction window");
    attack=g->pending_attack;victim=g->pending_victim;g->status.pending_target=-1;
    if(g->entity[actor].alive && entity_ok(g,victim) && g->entity[victim].alive) {
        if(attack==1) melee(g,actor,victim);
        else if(attack==2) {int d=roll(g);emit(g,SH_EVENT_WEAPON,actor,victim,"Spitter %d fires after reactions; die %d.",actor,d);if(d>=4) damage(g,victim,1,actor);}
    }
    visibility(g);return SH_OK;
}
static int melee_bonus(const SHEntity *e) {return e->team==SH_MARINE?0:e->bug==SH_SKITTER?1:e->bug==SH_BRUTE?3:e->bug==SH_SPITTER?0:2;}
static void melee(SHGame *g,int a,int b) {
    SHEntity *attacker=&g->entity[a],*defender=&g->entity[b];int ar=roll(g),br=roll(g),av=ar+melee_bonus(attacker),bv=br+melee_bonus(defender);
    if(attacker->team==SH_ALIEN && defender->team==SH_MARINE && attacker->row==defender->row-dr[defender->facing] && attacker->col==defender->col-dc[defender->facing]) av++;
    emit(g,SH_EVENT_WEAPON,a,b,"Melee: entity %d rolls %d (total %d); entity %d rolls %d (total %d).",a,ar,av,b,br,bv);
    if(av>bv) damage(g,b,1,a);else if(bv>av) damage(g,a,1,b);
}
static SHResult pay(SHGame *g,int cost) {
    SHEntity *e=&g->entity[g->status.active];
    if(e->ap<cost) return fail(g,SH_NO_AP,"not enough action points");
    e->ap-=cost;return SH_OK;
}
static int carried_type(const SHGame *g,int id) {int item=g->entity[id].item;return item<0?-1:g->items[item].type;}
static int available_item(const SHGame *g,int type) {
    int i;for(i=0;i<g->item_count;i++) if(g->items[i].type==type && g->items[i].view.holder==-1 && !g->items[i].view.dropped) return i;return -1;
}
static void consume_item(SHGame *g,int marine) {
    int item=g->entity[marine].item;
    if(item>=0) {g->items[item].view.holder=-2;g->entity[marine].item=-1;}
}
static SHResult collect(SHGame *g,int type,int cost) {
    int id=g->status.active,item=available_item(g,type);
    if(g->entity[id].item>=0||item<0) return fail(g,SH_INVALID,"item slot occupied or objective item unavailable");
    if(pay(g,cost)!=SH_OK) return SH_NO_AP;
    g->entity[id].item=item;g->items[item].view.holder=id;
    emit(g,SH_EVENT_OBJECTIVE,id,-1,"%s collects %s for %d AP.",name_of(id),g->items[item].view.name,cost);return SH_OK;
}
static SHResult flag_objective(SHGame *g,unsigned flag,const char *message) {
    if(g->status.objectives&flag) return fail(g,SH_INVALID,"objective already completed");
    if(pay(g,2)!=SH_OK) return SH_NO_AP;
    g->status.objectives|=flag;emit(g,SH_EVENT_OBJECTIVE,g->status.active,-1,"%s: %s (2 AP).",name_of(g->status.active),message);return SH_OK;
}
static SHResult evacuate(SHGame *g,int needed) {
    int id=g->status.active,cost=g->scenario->id==8 && g->status.shield?0:1;
    if(!g->status.clock_round) return fail(g,SH_INVALID,"evacuation is not authorized");
    if(pay(g,cost)!=SH_OK) return SH_NO_AP;
    g->entity[id].extracted=1;g->entity[id].done=1;g->entity[id].overwatch=0;g->entity[id].ap=0;
    if(g->entity[id].item>=0) consume_item(g,id);
    g->status.evacuated++;g->status.active=-1;
    emit(g,SH_EVENT_OBJECTIVE,id,-1,"%s evacuates; %d/%d Marines safe.",name_of(id),g->status.evacuated,needed);
    if(g->status.evacuated>=needed) outcome(g,SH_MARINES_WIN,"required Marines evacuated");
    return SH_OK;
}
static SHResult interact(SHGame *g,SHAction a) {
    int id=g->status.active,scenario=g->scenario->id,item=carried_type(g,id),i;
    SHEntity *e=&g->entity[id];char label=g->board[e->row][e->col];unsigned flags=g->status.objectives;
    if(scenario==8 && label!='Z' && g->status.clock_round && !g->status.shield) {
        for(i=0;i<4;i++) if(sh_tile(g,e->row+dr[i],e->col+dc[i])=='Z') {
            if(pay(g,2)!=SH_OK) return SH_NO_AP;
            g->status.shield=1;emit(g,SH_EVENT_OBJECTIVE,id,-1,"%s seals the ramp shield; boarding now costs 0 AP during activation.",name_of(id));return SH_OK;
        }
    }
    switch(scenario) {
    case 0:case 2:case 8:
        if(label=='U'||label=='V') return flag_objective(g,label=='U'?SH_LEFT:SH_RIGHT,"relay restored");
        if(scenario==8 && label=='Q') return collect(g,ITEM_KEY,1);
        if(scenario==8 && label=='Z') return evacuate(g,3);
        if(scenario==2 && label=='Q') return collect(g,ITEM_CAPACITOR,2);
        if(label=='X' && (flags&(SH_LEFT|SH_RIGHT))==(SH_LEFT|SH_RIGHT)) {
            if(scenario==0) {
                if(pay(g,2)!=SH_OK) return SH_NO_AP;
                emit(g,SH_EVENT_OBJECTIVE,id,-1,"%s transmits the beacon coordinates.",name_of(id));outcome(g,SH_MARINES_WIN,"both relays restored and beacon transmitted");return SH_OK;
            }
            if(!(flags&SH_STARTED)) {
                if(scenario==8 && item!=ITEM_KEY) return fail(g,SH_INVALID,"authorization requires the carried key");
                if(pay(g,2)!=SH_OK) return SH_NO_AP;
                g->status.objectives|=SH_STARTED;g->status.clock_round=g->status.round;
                if(scenario==8) consume_item(g,id);
                emit(g,SH_EVENT_OBJECTIVE,id,-1,"%s starts %s in round %d.",name_of(id),scenario==2?"transmission":"launch",g->status.round);return SH_OK;
            }
            if(scenario==2 && item==ITEM_CAPACITOR) {
                if(pay(g,2)!=SH_OK) return SH_NO_AP;
                consume_item(g,id);g->status.signal_remaining--;
                emit(g,SH_EVENT_OBJECTIVE,id,-1,"Capacitor removes a signal marker; %d remain.",g->status.signal_remaining);
                if(!g->status.signal_remaining) outcome(g,SH_MARINES_WIN,"transmission completed");return SH_OK;
            }
        }
        break;
    case 1:
        if(label=='P') return flag_objective(g,SH_POWER,"recorder power isolated");
        if(label=='R' && (flags&SH_POWER)) return collect(g,ITEM_RECORDER,2);
        if(label=='Q' && !g->supply_used && (a.option==1 || e->weapon==SH_CANNON)) {
            if(pay(g,2)!=SH_OK) return SH_NO_AP;
            g->supply_used=1;if(a.option==1) e->frag++;else e->ammunition+=4;
            emit(g,SH_EVENT_OBJECTIVE,id,-1,"%s opens the supply locker for %s.",name_of(id),a.option==1?"one fragmentation grenade":"four cannon ammunition");return SH_OK;
        }
        if(label=='Z' && item==ITEM_RECORDER) {
            if(pay(g,2)!=SH_OK) return SH_NO_AP;
            consume_item(g,id);outcome(g,SH_MARINES_WIN,"flight recorder extracted");return SH_OK;
        }
        break;
    case 3:
        if(label=='H') return flag_objective(g,SH_POWER,"crew life support restored");
        if((flags&SH_POWER) && (label=='L'||label=='R')) return collect(g,label=='L'?ITEM_CAPSULE_L:ITEM_CAPSULE_R,2);
        if(label=='Z' && (item==ITEM_CAPSULE_L||item==ITEM_CAPSULE_R)) {
            if(pay(g,2)!=SH_OK) return SH_NO_AP;
            consume_item(g,id);g->status.rescued++;
            emit(g,SH_EVENT_OBJECTIVE,id,-1,"%s extracts a rescue capsule; %d/2 safe.",name_of(id),g->status.rescued);
            if(g->status.rescued==2) outcome(g,SH_MARINES_WIN,"both crew capsules rescued");return SH_OK;
        }
        break;
    case 4:
        if(label>='1' && label<='3') return flag_objective(g,label=='1'?SH_LEFT:label=='2'?SH_RIGHT:SH_THIRD,"coolant valve opened");
        if(label=='X' && (flags&(SH_LEFT|SH_RIGHT|SH_THIRD))==(SH_LEFT|SH_RIGHT|SH_THIRD) && !(flags&SH_STARTED)) {
            if(pay(g,2)!=SH_OK) return SH_NO_AP;
            g->status.objectives|=SH_STARTED;g->status.clock_round=g->status.round;g->arrivals_stopped=1;
            emit(g,SH_EVENT_OBJECTIVE,id,-1,"Reactor shutdown: evacuate three by phase %d or overall deadline, whichever is earlier.",g->status.round+3);return SH_OK;
        }
        if(label=='Z') return evacuate(g,3);
        break;
    case 5:
        if(label=='Q') {SHResult result=collect(g,ITEM_CHARGE,1);if(result==SH_OK) g->status.charges_left--;return result;}
        if(label>='1' && label<='3' && item==ITEM_CHARGE) {
            unsigned flag=label=='1'?SH_LEFT:label=='2'?SH_RIGHT:SH_THIRD;
            SHResult result=flag_objective(g,flag,"anchor charge armed");
            if(result!=SH_OK) return result;
            consume_item(g,id);
            if((g->status.objectives&(SH_LEFT|SH_RIGHT|SH_THIRD))==(SH_LEFT|SH_RIGHT|SH_THIRD)) {
                g->status.clock_round=g->status.round;g->arrivals_stopped=1;
                emit(g,SH_EVENT_OBJECTIVE,id,-1,"Collapse clock starts: evacuate two by phase %d or overall deadline.",g->status.round+2);
            }
            return SH_OK;
        }
        if(label=='Z') return evacuate(g,2);
        break;
    case 6:
        if(label=='L'||label=='R') return collect(g,label=='L'?ITEM_SAMPLE_L:ITEM_SAMPLE_R,2);
        if(label=='H' && (item==ITEM_SAMPLE_L||item==ITEM_SAMPLE_R)) {
            if(pay(g,2)!=SH_OK) return SH_NO_AP;
            g->status.objectives|=item==ITEM_SAMPLE_L?SH_ANALYZE_LEFT:SH_ANALYZE_RIGHT;consume_item(g,id);
            emit(g,SH_EVENT_OBJECTIVE,id,-1,"%s delivers a sample for analysis.",name_of(id));
            if((g->status.objectives&(SH_ANALYZE_LEFT|SH_ANALYZE_RIGHT))==(SH_ANALYZE_LEFT|SH_ANALYZE_RIGHT)) {g->alarm=1;emit(g,SH_EVENT_OBJECTIVE,id,-1,"Protocol alarm: next queued contact must use entry F.");}
            return SH_OK;
        }
        if(label=='X' && (flags&(SH_ANALYZE_LEFT|SH_ANALYZE_RIGHT))==(SH_ANALYZE_LEFT|SH_ANALYZE_RIGHT)) return collect(g,ITEM_LEDGER,2);
        if(label=='Z' && item==ITEM_LEDGER) {
            if(pay(g,2)!=SH_OK) return SH_NO_AP;
            consume_item(g,id);outcome(g,SH_MARINES_WIN,"crew ledger extracted after both samples analyzed");return SH_OK;
        }
        break;
    case 7:
        if(label=='L'||label=='R') {
            unsigned flag=label=='L'?SH_LEFT:SH_RIGHT;int brute=-1,hidden=0;
            for(i=5;i<g->status.entities;i++) {
                if(g->entity[i].team==SH_ALIEN && g->entity[i].quarry) brute=i;
                if(g->entity[i].alive && g->entity[i].team==SH_BLIP) hidden++;
            }
            if(flags&flag) return fail(g,SH_INVALID,"scanner already activated");
            if(brute<0 && hidden && (!entity_ok(g,a.target)||!g->entity[a.target].alive||g->entity[a.target].team!=SH_BLIP)) return fail(g,SH_INVALID,"choose a hidden contact to inspect");
            if(pay(g,2)!=SH_OK) return SH_NO_AP;
            g->status.objectives|=flag;
            if(brute>=0||g->status.brute_dead) g->status.brute_tagged=1;
            else if(hidden) {
                g->entity[a.target].inspected=1;
                if(g->entity[a.target].quarry) g->status.brute_tagged=1;
                emit(g,SH_EVENT_OBJECTIVE,id,a.target,"Scanner inspects blip %d: strength %d, profile %s; no reveal or reaction.",a.target,g->entity[a.target].strength,sh_bug_name(g->entity[a.target].bug));
            }
            emit(g,SH_EVENT_OBJECTIVE,id,-1,"%s activates scanner %c; brute tagged=%d.",name_of(id),label,g->status.brute_tagged);return SH_OK;
        }
        if(label=='Q' && !g->supply_used && e->wounds<(g->options.hard_vacuum?1:2)) {
            if(pay(g,2)!=SH_OK) return SH_NO_AP;
            e->wounds++;g->supply_used=1;emit(g,SH_EVENT_OBJECTIVE,id,-1,"%s heals one wound at the medical cabinet.",name_of(id));return SH_OK;
        }
        if(label=='X' && (flags&(SH_LEFT|SH_RIGHT))==(SH_LEFT|SH_RIGHT) && g->status.brute_dead && g->status.brute_tagged) {
            if(pay(g,2)!=SH_OK) return SH_NO_AP;
            outcome(g,SH_MARINES_WIN,"both scanners active and tagged brute eliminated; report sent");return SH_OK;
        }
        break;
    default:break;
    }
    return fail(g,SH_INVALID,"objective unavailable here or prerequisites unfinished");
}
static int area(const SHGame *g,int r,int c,int y,int x) {return valid(y,x)&&distance(r,c,y,x)<=1&&sh_tile_is_floor(sh_tile(g,y,x));}
static void blast(SHGame *g,int actor,int r,int c,int stun,int flame) {
    int i,d;
    for(i=5;i<g->status.entities;i++) if(g->entity[i].alive && g->entity[i].team==SH_BLIP && area(g,r,c,g->entity[i].row,g->entity[i].col)) reveal(g,i);
    for(i=0;i<g->status.entities;i++) if(g->entity[i].alive && !g->entity[i].extracted && area(g,r,c,g->entity[i].row,g->entity[i].col)) {
        if(stun) {g->entity[i].stunned=1;if(g->entity[i].team==SH_MARINE) g->entity[i].overwatch=0;emit(g,SH_EVENT_WEAPON,actor,i,"Stun affects entity %d; no stacking, overwatch cancelled for Marines.",i);}
        else {d=roll(g);emit(g,SH_EVENT_WEAPON,actor,i,"%s blast at (%d,%d): entity %d rolls %d.",flame?"Flame":"Fragmentation",r+1,c+1,i,d);if(d>=(flame?3:4)) damage(g,i,flame?2:1,actor);}
    }
}
SHResult sh_activate(SHGame *g,int id) {
    SHEntity *e;
    if(!g||!entity_ok(g,id)) return SH_INVALID;
    if(g->status.outcome!=SH_ONGOING) return fail(g,SH_GAME_OVER,"mission finished");
    if(g->status.pending_target>=0) return fail(g,SH_REACTION_PENDING,"resolve overwatch first");
    if(g->status.phase!=SH_MARINE_PHASE && g->status.phase!=SH_ALIEN_PHASE) return fail(g,SH_WRONG_PHASE,"deployment is unfinished");
    if(g->status.active>=0) return fail(g,SH_INVALID,"finish the current activation first");
    e=&g->entity[id];
    if(!e->alive||e->extracted||e->done) return fail(g,SH_INVALID,"entity removed, extracted, or already activated");
    if((g->status.phase==SH_MARINE_PHASE)!=(e->team==SH_MARINE)) return fail(g,SH_WRONG_PHASE,"entity belongs to the other team");
    g->status.active=id;emit(g,SH_EVENT_ACTIVATION,id,-1,"%s %d activates with %d AP.",e->team==SH_MARINE?name_of(id):"Contact",id,e->ap);return SH_OK;
}
SHResult sh_end_activation(SHGame *g) {
    int id;
    if(!g) return SH_INVALID;
    if(g->status.outcome!=SH_ONGOING) return SH_GAME_OVER;
    if(g->status.pending_target>=0) return fail(g,SH_REACTION_PENDING,"resolve overwatch first");
    id=g->status.active;if(id<0) return fail(g,SH_INVALID,"no active model");
    g->entity[id].ap=0;g->entity[id].done=1;g->status.active=-1;return SH_OK;
}
SHResult sh_action(SHGame *g,SHAction a) {
    SHEntity *e;int id,cost=0,i,victim=-1,attack=0;SHResult result;
    if(!g) return SH_INVALID;
    if(g->status.outcome!=SH_ONGOING) return fail(g,SH_GAME_OVER,"mission finished");
    if(g->status.pending_target>=0) return fail(g,SH_REACTION_PENDING,"resolve overwatch first");
    id=g->status.active;if(id<0) return fail(g,SH_INVALID,"activate a model first");e=&g->entity[id];g->error[0]='\0';
    if(e->team==SH_BLIP && a.type==SH_REVEAL) {
        if(e->ap!=6-(e->stunned?2:0)) return fail(g,SH_INVALID,"voluntary reveal is only allowed at activation start");
        reveal(g,id);visibility(g);return SH_OK;
    }
    if(e->team!=SH_MARINE && a.type!=SH_MOVE && a.type!=SH_DOOR && a.type!=SH_MELEE && a.type!=SH_SHOOT && a.type!=SH_BREACH) return fail(g,SH_INVALID,"alien cannot perform this action");
    if(e->overwatch) return fail(g,SH_INVALID,"overwatch ended this Marine's activation");
    switch(a.type) {
    case SH_MOVE:
        if(!valid(a.row,a.col)||distance(e->row,e->col,a.row,a.col)!=1) return fail(g,SH_INVALID,"movement is one orthogonal square");
        if(!sh_tile_is_floor(sh_tile(g,a.row,a.col))||occupied(g,a.row,a.col)>=0) return fail(g,SH_BLOCKED,"destination occupied, hull wall, exterior, or closed door");
        cost=e->team==SH_MARINE && (a.row-e->row)*dr[e->facing]+(a.col-e->col)*dc[e->facing]<0?2:1;
        if(g->scenario->id==3 && (carried_type(g,id)==ITEM_CAPSULE_L||carried_type(g,id)==ITEM_CAPSULE_R)) cost=2;
        if(pay(g,cost)!=SH_OK) return SH_NO_AP;e->row=a.row;e->col=a.col;
        emit(g,SH_EVENT_MOVEMENT,id,-1,"%s movement: %s %d moves to (%d,%d), costs %d AP; %d remain.",e->team==SH_MARINE?"Marine":"Alien",name_of(id),id,a.row+1,a.col+1,cost,e->ap);break;
    case SH_TURN:
        if(a.option<0||a.option>3||a.option==(int)e->facing) return fail(g,SH_INVALID,"choose a different valid facing");
        cost=abs(a.option-(int)e->facing)==2?2:1;if(pay(g,cost)!=SH_OK) return SH_NO_AP;
        e->facing=(SHFacing)a.option;emit(g,SH_EVENT_MOVEMENT,id,-1,"Marine movement: %s turns to facing %d for %d AP.",name_of(id),a.option,cost);break;
    case SH_DOOR:case SH_BREACH:
        if(!valid(a.row,a.col)||distance(e->row,e->col,a.row,a.col)!=1) return fail(g,SH_INVALID,"door must be orthogonally adjacent");
        if(a.type==SH_BREACH) {
            if(!g->options.sealed_bulkheads||sh_tile(g,a.row,a.col)!=SH_CLOSED_DOOR) return fail(g,SH_INVALID,"breach option disabled or door already open");
            cost=2;if(pay(g,cost)!=SH_OK) return SH_NO_AP;i=roll(g);
            if(i>=5) g->board[a.row][a.col]='.';
            emit(g,SH_EVENT_DOOR,id,-1,"Bulkhead breach die %d at (%d,%d).",i,a.row+1,a.col+1);
        } else {
            char tile=sh_tile(g,a.row,a.col);
            if(tile!=SH_CLOSED_DOOR&&tile!=SH_OPEN_DOOR) return fail(g,SH_INVALID,"square is not an operable door");
            if(occupied(g,a.row,a.col)>=0) return fail(g,SH_BLOCKED,"cannot close an occupied doorway");
            cost=e->team==SH_ALIEN && e->bug==SH_BRUTE?2:1;if(pay(g,cost)!=SH_OK) return SH_NO_AP;
            g->board[a.row][a.col]=tile==SH_CLOSED_DOOR?SH_OPEN_DOOR:SH_CLOSED_DOOR;
            emit(g,SH_EVENT_DOOR,id,-1,"Entity %d %s door (%d,%d) for %d AP.",id,tile==SH_CLOSED_DOOR?"opens":"closes",a.row+1,a.col+1,cost);
        }
        break;
    case SH_SHOOT:
        if(e->team==SH_BLIP) return fail(g,SH_INVALID,"blips must reveal before attacking");
        if(e->team==SH_ALIEN) {
            if(e->bug!=SH_SPITTER||!entity_ok(g,a.target)||!g->entity[a.target].alive||g->entity[a.target].team!=SH_MARINE||g->entity[a.target].extracted) return fail(g,SH_INVALID,"spitter requires a living Marine target");
            victim=a.target;if(distance(e->row,e->col,g->entity[victim].row,g->entity[victim].col)>4||!sh_can_see(g,id,g->entity[victim].row,g->entity[victim].col)) return fail(g,SH_NOT_VISIBLE,"spitter target outside sight or range");
            if(pay(g,2)!=SH_OK) return SH_NO_AP;attack=2;break;
        }
        if(e->jammed) return fail(g,SH_JAMMED,"clear the jam first");
        if(e->ammunition==0) return fail(g,SH_NO_AMMO,"weapon has no ammunition");
        if(e->weapon==SH_FLAME) {
            if(!valid(a.row,a.col)||!arc(e,a.row,a.col)||distance(e->row,e->col,a.row,a.col)>4||!sh_can_see(g,id,a.row,a.col)) return fail(g,SH_NOT_VISIBLE,"flame target must be visible floor in front arc and range");
            if(pay(g,2)!=SH_OK) return SH_NO_AP;e->ammunition--;
            emit(g,SH_EVENT_WEAPON,id,-1,"Marine weapons use: %s fires flame projector; %d fuel remain.",name_of(id),e->ammunition);blast(g,id,a.row,a.col,0,1);
        } else {
            if(!sh_can_shoot(g,id,a.target)) return fail(g,SH_NOT_VISIBLE,"target outside sight, arc, range, or enemy profile");
            if(pay(g,1)!=SH_OK) return SH_NO_AP;shoot(g,id,a.target,0);
        }
        break;
    case SH_MELEE:
        if(e->team==SH_BLIP||!entity_ok(g,a.target)||!g->entity[a.target].alive||g->entity[a.target].extracted||g->entity[a.target].team==SH_BLIP||(g->entity[a.target].team==SH_MARINE)==(e->team==SH_MARINE)||distance(e->row,e->col,g->entity[a.target].row,g->entity[a.target].col)!=1) return fail(g,SH_INVALID,"melee requires an adjacent enemy model");
        if(pay(g,1)!=SH_OK) return SH_NO_AP;victim=a.target;
        if(e->team==SH_MARINE) melee(g,id,victim);else attack=1;break;
    case SH_FRAG:case SH_STUN:
        if((a.type==SH_FRAG?e->frag:e->stun)<=0) return fail(g,SH_NO_AMMO,"no grenade of this type remains");
        if(!valid(a.row,a.col)||!arc(e,a.row,a.col)||distance(e->row,e->col,a.row,a.col)<1||distance(e->row,e->col,a.row,a.col)>4||!sh_can_see(g,id,a.row,a.col)) return fail(g,SH_NOT_VISIBLE,"grenade landing square outside sight, arc, or range");
        if(pay(g,2)!=SH_OK) return SH_NO_AP;if(a.type==SH_FRAG) e->frag--;else e->stun--;
        emit(g,SH_EVENT_WEAPON,id,-1,"Marine weapons use: %s throws a %s grenade to (%d,%d).",name_of(id),a.type==SH_FRAG?"fragmentation":"stun",a.row+1,a.col+1);blast(g,id,a.row,a.col,a.type==SH_STUN,0);break;
    case SH_OVERWATCH:
        if(e->weapon==SH_FLAME||e->stunned) return fail(g,SH_INVALID,"weapon or stun prevents overwatch");
        if(e->jammed) return fail(g,SH_JAMMED,"clear the jam first");if(e->ammunition==0) return fail(g,SH_NO_AMMO,"empty weapon cannot enter overwatch");
        if(pay(g,2)!=SH_OK) return SH_NO_AP;e->overwatch=1;e->done=1;e->ap=0;g->status.active=-1;
        emit(g,SH_EVENT_OVERWATCH,id,-1,"Overwatch: %s covers facing %d with %s; activation ends.",name_of(id),e->facing,sh_weapon_name(e->weapon));break;
    case SH_CLEAR_JAM:
        if(!e->jammed) return fail(g,SH_INVALID,"weapon is not jammed");if(pay(g,1)!=SH_OK) return SH_NO_AP;
        e->jammed=0;emit(g,SH_EVENT_JAM,id,-1,"%s clears the jam for 1 AP.",name_of(id));break;
    case SH_INTERACT:
        result=interact(g,a);if(result!=SH_OK) return result;break;
    case SH_PICKUP:
        if(a.option<0||a.option>=g->item_count||e->item>=0||g->items[a.option].view.holder!=-1||!g->items[a.option].view.dropped||g->items[a.option].view.row!=e->row||g->items[a.option].view.col!=e->col) return fail(g,SH_INVALID,"no dropped item here or slot occupied");
        if(pay(g,1)!=SH_OK) return SH_NO_AP;e->item=a.option;g->items[a.option].view.holder=id;
        emit(g,SH_EVENT_OBJECTIVE,id,-1,"%s picks up dropped %s.",name_of(id),g->items[a.option].view.name);break;
    case SH_TRANSFER:
        if(e->item<0||a.target<0||a.target>=5||!g->entity[a.target].alive||g->entity[a.target].extracted||g->entity[a.target].item>=0||distance(e->row,e->col,g->entity[a.target].row,g->entity[a.target].col)!=1) return fail(g,SH_INVALID,"transfer needs an adjacent Marine with an empty slot");
        if(pay(g,1)!=SH_OK) return SH_NO_AP;g->entity[a.target].item=e->item;g->items[e->item].view.holder=a.target;e->item=-1;
        emit(g,SH_EVENT_OBJECTIVE,id,a.target,"%s hands a mission item to %s.",name_of(id),name_of(a.target));break;
    case SH_SUPPLY:
        if(!g->options.supply_cache||e->weapon!=SH_CANNON||g->supply_used||!strchr("UVXPHLR123",g->board[e->row][e->col])) return fail(g,SH_INVALID,"supply option unavailable here");
        if(pay(g,2)!=SH_OK) return SH_NO_AP;e->ammunition+=4;g->supply_used=1;
        emit(g,SH_EVENT_OBJECTIVE,id,-1,"%s uses house-rule supply cache: four ammunition added.",name_of(id));break;
    default:return fail(g,SH_INVALID,"unsupported action");
    }
    if(g->status.outcome!=SH_ONGOING) return SH_OK;
    visibility(g);
    /* A moving blip reveals automatically if it ends next to a Marine. */
    if(e->team==SH_BLIP) for(i=0;i<5;i++) if(g->entity[i].alive&&!g->entity[i].extracted&&distance(e->row,e->col,g->entity[i].row,g->entity[i].col)==1) {reveal(g,id);break;}
    if(g->status.phase==SH_ALIEN_PHASE) open_reactions(g,g->status.active>=0?g->status.active:id,attack,victim);
    return SH_OK;
}
static void reset_ap(SHGame *g,SHTeam team) {
    int i;
    for(i=0;i<g->status.entities;i++) {
        SHEntity *e=&g->entity[i];
        if(e->alive&&!e->extracted && ((e->team==SH_MARINE)==(team==SH_MARINE))) {
            e->ap=base_ap(e)-(team!=SH_MARINE && e->stunned?2:0);if(e->ap<0)e->ap=0;e->done=0;
        }
    }
}
SHResult sh_end_phase(SHGame *g) {
    int i;
    if(!g) return SH_INVALID;
    if(g->status.outcome!=SH_ONGOING) return fail(g,SH_GAME_OVER,"mission finished");
    if(g->status.pending_target>=0) return fail(g,SH_REACTION_PENDING,"resolve overwatch first");
    if(g->status.active>=0) return fail(g,SH_INVALID,"finish active entity first");
    if(g->status.phase==SH_DEPLOYMENT) return fail(g,SH_WRONG_PHASE,"use finish_deployment");
    if(g->status.phase==SH_MARINE_PHASE) {
        if(g->scenario->id==2 && (g->status.objectives&SH_STARTED) && g->status.round>g->status.clock_round) {
            for(i=0;i<5;i++) if(g->entity[i].alive&&!g->entity[i].extracted&&sh_tile(g,g->entity[i].row,g->entity[i].col)=='X') {
                g->status.signal_remaining--;emit(g,SH_EVENT_OBJECTIVE,i,-1,"Operator holds transmitter: %d signal marker(s) remain.",g->status.signal_remaining);
                if(g->status.signal_remaining==0) outcome(g,SH_MARINES_WIN,"transmission completed");break;
            }
        }
        check_result(g,1);if(g->status.outcome!=SH_ONGOING) return SH_OK;
        for(i=0;i<5;i++) {g->entity[i].done=1;g->entity[i].ap=0;}
        g->status.phase=SH_DEPLOYMENT;g->arrived=0;memset(g->entry_used,0,sizeof g->entry_used);reset_ap(g,SH_ALIEN);
        if(!g->arrivals_stopped && g->status.round<=g->scenario->arrival_cutoff) {
            if(g->status.round>=g->scenario->schedule_first && g->status.round<=g->scenario->schedule_last) {
                for(i=0;i<g->scenario->arrivals_per_phase && g->scheduled<g->normal_contacts;i++) g->queue[g->queue_last++]=g->scheduled++;
            }
            if(g->options.ghost_contacts && g->status.round==3) g->queue[g->queue_last++]=g->normal_contacts;
        } else g->queue_first=g->queue_last;
        emit(g,SH_EVENT_PHASE,-1,-1,"Alien deployment phase %d: %d queued contact(s), arrival cap %d.",g->status.round,g->queue_last-g->queue_first,g->scenario->arrivals_per_phase);
    } else {
        for(i=0;i<g->status.entities;i++) {g->entity[i].stunned=0;g->entity[i].overwatch=0;}
        g->status.round++;g->status.phase=SH_MARINE_PHASE;reset_ap(g,SH_MARINE);
        emit(g,SH_EVENT_PHASE,-1,-1,"Marine phase %d: movement, weapons, and overwatch available.",g->status.round);
    }
    return SH_OK;
}
static int entry_position(const SHGame *g,char label,int *row,int *col) {
    int r,c;if(label<'A'||label>'F') return 0;
    for(r=0;r<SH_ROWS;r++) for(c=0;c<SH_COLS;c++) if(g->board[r][c]==label) {*row=r;*col=c;return 1;}return 0;
}
SHResult sh_deploy(SHGame *g,char entry) {
    int r,c,index,id;SHContact contact;
    if(!g) return SH_INVALID;
    if(g->status.outcome!=SH_ONGOING) return SH_GAME_OVER;
    if(g->status.phase!=SH_DEPLOYMENT) return fail(g,SH_WRONG_PHASE,"not deployment phase");
    if(g->queue_first>=g->queue_last||g->arrived>=g->scenario->arrivals_per_phase) return fail(g,SH_INVALID,"no arrival available or phase cap reached");
    if(!entry_position(g,entry,&r,&c)) return fail(g,SH_INVALID,"unknown entry");
    if(g->alarm && entry!='F') return fail(g,SH_INVALID,"alarm contact must enter at F");
    if(g->entry_used[entry-'A']||occupied(g,r,c)>=0) return fail(g,SH_BLOCKED,"entry occupied or used this phase");
    if(g->status.entities>=SH_MAX_ENTITIES) return fail(g,SH_INVALID,"entity capacity exhausted");
    index=g->queue[g->queue_first++];contact=g->contacts[index];id=add_entity(g,SH_BLIP,r,c,contact.bug,contact.strength);
    if(id<0) return fail(g,SH_INVALID,"entity capacity exhausted");
    g->entity[id].quarry=contact.quarry;g->arrived++;g->entry_used[entry-'A']=1;g->alarm=0;
    emit(g,SH_EVENT_DEPLOYMENT,id,-1,"Contact %d arrives at %c; hidden strength retained, no overwatch on arrival.",id,entry);visibility(g);return SH_OK;
}
SHResult sh_finish_deployment(SHGame *g) {
    char entry;int r,c;
    if(!g) return SH_INVALID;
    if(g->status.outcome!=SH_ONGOING) return SH_GAME_OVER;
    if(g->status.phase!=SH_DEPLOYMENT) return fail(g,SH_WRONG_PHASE,"not deployment phase");
    if(g->queue_first<g->queue_last && g->arrived<g->scenario->arrivals_per_phase) {
        for(entry='A';entry<='F';entry++) if((!g->alarm||entry=='F')&&!g->entry_used[entry-'A']&&entry_position(g,entry,&r,&c)&&occupied(g,r,c)<0) return fail(g,SH_INVALID,"legal arrivals remain; deploy them before activating aliens");
    }
    if(g->status.round==g->scenario->arrival_cutoff) g->queue_first=g->queue_last;
    g->status.phase=SH_ALIEN_PHASE;emit(g,SH_EVENT_PHASE,-1,-1,"Alien phase %d begins.",g->status.round);return SH_OK;
}
size_t sh_board(const SHGame *g,char *buffer,size_t capacity) {
    char text[(SH_COLS+1)*SH_ROWS+1];int r,c,i;size_t n=0;
    if(!g) {if(buffer&&capacity)buffer[0]='\0';return 0;}
    for(r=0;r<SH_ROWS;r++) {
        for(c=0;c<SH_COLS;c++) {
            char tile=sh_view_tile(g,r,c,SH_MARINE);SHEntity viewed;i=occupied(g,r,c);
            if(i>=0&&sh_observed_entity(g,i,SH_MARINE,&viewed)) tile=viewed.team==SH_MARINE?'M':viewed.team==SH_BLIP?'b':'a';
            text[n++]=tile;
        }
        text[n++]='\n';
    }
    text[n]='\0';
    if(buffer&&capacity) {size_t copy=n<capacity-1?n:capacity-1;memcpy(buffer,text,copy);buffer[copy]='\0';}
    return n+1;
}
