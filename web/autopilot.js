import {CLOSED_DOOR,isFloor} from './tiles.js';
const live=e=>e.alive&&!e.extracted;
const distance=(a,b)=>Math.abs(a.r-b.r)+Math.abs(a.c-b.c);
const action=(type,r=0,c=0,target=-1)=>[1,type,r,c,target];
const direction=(a,b)=>Math.abs(b.r-a.r)>=Math.abs(b.c-a.c)?(b.r<a.r?0:2):(b.c>a.c?1:3);

export function route(map,entities,unit,goal){
  const queue=[{r:unit.r,c:unit.c,path:[]}],visited=new Set([`${unit.r},${unit.c}`]);
  const occupied=new Set(entities.filter(e=>live(e)&&e.id!==unit.id).map(e=>`${e.r},${e.c}`));
  for(let i=0;i<queue.length;i++){
    const node=queue[i];if(node.r===goal.r&&node.c===goal.c)return node.path;
    for(const [dr,dc] of [[-1,0],[0,1],[1,0],[0,-1]]){
      const r=node.r+dr,c=node.c+dc,key=`${r},${c}`,tile=map[r]?.[c];
      if(!tile||visited.has(key)||occupied.has(key)||(!isFloor(tile)&&tile!==CLOSED_DOOR))continue;
      visited.add(key);queue.push({r,c,path:[...node.path,{r,c}]});
    }
  }
  return null;
}

function goal(s,mission,e){
  const f=s.objectives,id=e.id,item=s.items.find(i=>i.id===e.item);
  if(item){
    if(mission===5)return !(f&1)?'1':!(f&2)?'2':!(f&128)?'3':'Z';
    if(mission===6)return item.name.includes('sample')?'H':'Z';
    return mission===8||mission===2?'X':'Z';
  }
  switch(mission){
    case 0:case 2:case 8:
      if(!(f&1)&&(id===0||(f&2)))return 'U';
      if(!(f&2)&&(id===4||(f&1)))return 'V';
      if(mission===8){if(s.clockRound)return 'Z';if(id===3)return 'Q';}
      if(mission===2&&id===3&&!s.clockRound)return 'Q';
      if((f&3)===3)return 'X';break;
    case 1:return !(f&4)?(id===0?'P':null):(id===0||id===4?'R':null);
    case 3:return !(f&4)?(id===2?'H':null):(id===0?'L':id===4?'R':null);
    case 4:
      if(s.clockRound)return 'Z';
      if(id===0)return !(f&1)?'1':'Z';if(id===4)return !(f&128)?'3':'Z';if(id===1)return !(f&2)?'2':'Z';
      if(id===2&&(f&131)===131)return 'X';break;
    case 5:if(s.clockRound)return 'Z';if(s.chargesLeft&&[0,1,4].includes(id))return 'Q';break;
    case 6:if((f&96)===96)return [0,2].includes(id)?'X':null;return id===0&&!(f&32)?'L':id===4&&!(f&64)?'R':null;
    case 7:if(id===0&&!(f&1))return 'L';if(id===4&&!(f&2))return 'R';if((f&3)===3&&s.bruteDead&&s.bruteTagged)return 'X';break;
  }
  return null;
}

/* Return ordered candidates. The caller executes at most one successful command
 * per tick. Marines use observed enemies, not private strengths or unseen bugs. */
export function autoCommands(engine,mission){
  const publicState=engine.snapshot(0);
  const s=publicState.phase===2?engine.snapshot(1):publicState;
  if(s.outcome)return [];
  if(s.pending>=0)return reactionCommands(engine,publicState);
  if(s.phase===1){
    const map=engine.layout(),commands=[];
    for(let r=0;r<map.length;r++)for(let c=0;c<map[r].length;c++)if(/^[A-F]$/.test(map[r][c]))commands.push([4,map[r][c].charCodeAt(0)]);
    return [...commands,[5]];
  }
  if(s.active<0){
    const available=s.entities.filter(e=>live(e)&&!e.done&&(s.phase===0?e.team===0:e.team!==0));
    available.sort((a,b)=>s.phase===0?((a.id+5-s.round%5)%5)-((b.id+5-s.round%5)%5):a.id-b.id);
    return available.length?available.map(e=>[0,e.id]):[[3]];
  }
  const e=s.entities.find(e=>e.id===s.active),commands=[];
  if(!e||!live(e))return [[2]];
  const map=engine.layout();
  if(e.team===0){
    if(e.jam&&e.ap)commands.push(action(8));
    const enemies=s.entities.filter(t=>live(t)&&t.team===1).sort((a,b)=>distance(e,a)-distance(e,b));
    for(const t of enemies){
      if(engine.canShoot(e.id,t.id))commands.push(action(3,t.r,t.c,t.id));
      if(e.weapon===3&&distance(e,t)<=4&&!s.entities.some(m=>live(m)&&m.team===0&&distance(m,t)<=1))commands.push(action(3,t.r,t.c,t.id));
      if(distance(e,t)===1)commands.push(action(4,t.r,t.c,t.id));
      if(e.facing!==direction(e,t)&&distance(e,t)<=10)commands.push(action(1,0,0,direction(e,t)));
    }
    for(const item of s.items.filter(i=>i.holder===-1&&i.dropped&&i.r===e.r&&i.c===e.c))commands.push(action(10,0,0,item.id));
    const label=goal(s,mission,e);
    let destination;
    if(label){for(let r=0;r<map.length;r++){const c=map[r].indexOf(label);if(c>=0){destination={r,c};break;}}}
    else destination={r:15,c:e.id===1?9:e.id===3?21:15};
    if(destination){
      if(distance(e,destination)===0&&label){
        const contact=s.entities.find(t=>live(t)&&t.team===2);
        commands.push(action(9,e.r,e.c,contact?.id??-1));
      }else{
        const next=route(map,s.entities,e,destination)?.[0];
        if(next)commands.push(action(map[next.r][next.c]===CLOSED_DOOR?2:0,next.r,next.c));
      }
    }
    if(e.ap>=2&&!e.jam&&e.ammo!==0&&e.weapon!==3)commands.push(action(7));
  }else{
    const prey=s.entities.filter(t=>live(t)&&t.team===0).sort((a,b)=>distance(e,a)-distance(e,b));
    for(const t of prey){
      if(distance(e,t)===1){commands.push(action(e.team===2?12:4,t.r,t.c,t.id));}
      if(e.team===1&&e.bug===3&&distance(e,t)<=4)commands.push(action(3,t.r,t.c,t.id));
      const paths=[[-1,0],[0,1],[1,0],[0,-1]].map(([dr,dc])=>route(map,s.entities,e,{r:t.r+dr,c:t.c+dc})).filter(p=>p?.length).sort((a,b)=>a.length-b.length);
      const next=paths[0]?.[0];if(next)commands.push(action(map[next.r][next.c]===CLOSED_DOOR?2:0,next.r,next.c));
    }
  }
  return [...commands,[2]];
}

export function reactionCommands(engine,s=engine.snapshot(0)){
  if(s.pending<0)return [];
  return [...s.entities.filter(e=>live(e)&&e.team===0&&e.ow&&engine.canShoot(e.id,s.pending)).map(e=>[6,e.id,1]),[7]];
}
