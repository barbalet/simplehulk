import { loadEngine } from './engine.js';
const $ = id => document.getElementById(id);
const canvas = $('map'), ctx = canvas.getContext('2d');
const SIZE = 32, OFFSET = 3, names = ['Vale','Iona','Kes','Rook','Sen'];
const weaponNames = ['rifle','scattergun','cannon','flame'], bugNames = ['basic','skitter','brute','spitter'];
let engine, state, previous, manifest, sheets = {}, selected = {r:22,c:12}, effects = [], doors = [], motions = new Map(), bugFacing = new Map();
let batch = [], history = [], movingUntil = 0, clock = 0, locked = false, lastResult = '', missionId = 0, gameSeed = 2026;
const reduce = matchMedia('(prefers-reduced-motion: reduce)').matches;
$('motion').checked = !reduce;
function log(message, error = false) {
  history.push(message);
  const li = document.createElement('li');li.textContent = message;if(error)li.className='error';$('log').append(li);
  while($('log').children.length > 150) $('log').firstElementChild.remove();
  $('log').scrollTop = $('log').scrollHeight;
}
function feedback(e) {batch.push(e);log(`[${e.round}] ${e.message}`);}
const get = (s,id) => s?.entities.find(e=>e.id===id);
const alive = e => e.alive && !e.extracted;
function read() { state = engine.snapshot(Number($('viewer').value)); }
function sprite(name,x,y,angle=0,scale=1,opacity=1) {
  const s=manifest.sprites[name];if(!s)throw Error(`Missing sprite ${name}`);
  const atlas=manifest.sheets[s.sheet], image=sheets[s.sheet], w=image.width/atlas.columns,h=image.height/atlas.rows;
  ctx.save();ctx.globalAlpha=opacity;ctx.translate(x,y);ctx.rotate(angle);ctx.drawImage(image,(s.index%atlas.columns)*w,Math.floor(s.index/atlas.columns)*h,w,h,-SIZE*scale/2,-SIZE*scale/2,SIZE*scale,SIZE*scale);ctx.restore();
}
function point(r,c) {return {x:(c+.5)*SIZE,y:(r+OFFSET+.5)*SIZE};}
function heading(a,b) {return Math.atan2(b.c-a.c,-(b.r-a.r));}
function marker(r,c,tile) {
  const mission=missionId;
  if(tile==='L'||tile==='R') {
    if(mission===3)return 'capsule';if(mission===6)return 'sample';if(mission===7)return 'relay';
  }
  if(tile==='H'&&mission===3)return 'relay';
  if(tile==='Q')return mission===2?'capacitor':mission===5?'charges':mission===8?'key':'supply';
  return manifest.mapMarkers[tile];
}
function sync(before, after, args) {
  const duration=$('motion').checked?260:0;clock=performance.now();movingUntil=clock+duration;
  motions.clear();doors=[];
  for(const e of after.entities) {
    const old=get(before,e.id);
    if(old && (old.r!==e.r||old.c!==e.c||old.facing!==e.facing)) {
      let a=old.facing*Math.PI/2,b=e.facing*Math.PI/2;
      if(e.team!==0) {a=bugFacing.get(e.id)||0;b=heading(old,e);bugFacing.set(e.id,b);}
      while(b-a>Math.PI)b-=2*Math.PI;while(b-a<-Math.PI)b+=2*Math.PI;
      motions.set(e.id,{from:old,to:e,a,b});
    }
  }
  if(before)for(let r=0;r<25;r++)for(let c=0;c<31;c++) {
    const a=before.map[r][c],b=after.map[r][c];if(a!==b&&(a==='+'||a==='/'||b==='+'))doors.push({r,c,open:b!=='+',breach:b==='.'});
  }
  for(const event of batch) {
    const actor=get(before,event.actor)||get(after,event.actor), target=get(before,event.target)||get(after,event.target);
    if(!actor)continue;
    if((event.type===2||event.type===3)&&(/fires|Melee:/.test(event.message))) {
      const weapon=actor.team===0?weaponNames[actor.weapon]:'acid';
      const end=target || {r:args?.[2]??selected.r,c:args?.[3]??selected.c};
      const jam=batch.some(e=>e.type===4&&e.actor===event.actor);
      const hit=weapon==='flame'||batch.some(e=>e.type===5&&e.actor===event.actor);
      effects.push({start:clock,life:duration?650:0,from:actor,to:end,weapon:jam?'jam':/Melee:/.test(event.message)?'claw':weapon,hit});
    }
  }
  // Area attacks are chosen squares; damage events alone determine whether models were wounded.
  if(args?.[0]===1 && [5,6].includes(args[1]) && lastResult==='') {
    const actor=get(before,before.active);if(actor)effects.push({start:clock,life:duration?700:0,from:actor,to:{r:args[2],c:args[3]},weapon:args[1]===5?'frag':'stun',hit:true});
  }
  effects=effects.filter(e=>e.life>0);batch=[];
}
function perform(...args) {
  if(locked||!engine)return;
  previous=state;batch=[];const result=engine.command(...args);lastResult=result?engine.error():'';
  if(result)log(`Action rejected: ${lastResult}`,true);
  read();sync(previous,state,args);update();
  if(movingUntil>performance.now()) {locked=true;update();setTimeout(()=>{locked=false;update();},270);}
}
function update() {
  const phases=['Marines','Deploy contacts','Aliens','Finished'], active=get(state,state.active);
  $('status').textContent=`Round ${state.round}/${state.deadline} · ${phases[state.phase]} · active ${active?label(active)+' / '+active.ap+' AP':'none'} · objectives ${state.objectives} · evacuated ${state.evacuated}${missionId===2?' · signal '+state.signal:''}${state.clockRound?' · timer started phase '+state.clockRound:''}${state.rescued?' · rescued '+state.rescued:''}${missionId===5?' · charges in locker '+state.chargesLeft:''}${missionId===7?' · quarry tagged '+state.bruteTagged+' / killed '+state.bruteDead:''}${state.shield?' · shuttle shield active':''}${state.outcome?' · '+(state.outcome===1?'Marines win':'Aliens win'):''}${lastResult?' · '+lastResult:''}`;
  $('units').replaceChildren();
  for(const e of state.entities.filter(alive)) {
    const b=document.createElement('button');b.className='unit'+(e.id===state.active?' active':'');
    const title=document.createElement('div');title.className='name';title.textContent=`${e.id} / ${label(e)}`;
    const detail=document.createElement('div');detail.textContent=`(${e.r+1},${e.c+1}) • ${e.ap} AP • ${e.wounds} wounds${e.done?' • done':''}`;
    const equipment=document.createElement('div');equipment.textContent=e.team===0?`${weaponNames[e.weapon]} • ammo ${e.ammo<0?'∞':e.ammo} • frag ${e.frag} / stun ${e.stun}${e.ow?' • OVERWATCH':''}${e.jam?' • JAM':''}${e.item>=0?' • item '+e.item:''}`:e.team===2?`strength ${e.strength<0?'hidden':e.strength}`:bugNames[e.bug];
    b.append(title,detail,equipment);b.disabled=locked||state.outcome!==0;
    b.onclick=()=>{select(e.r,e.c,e.id);if(state.active!==e.id)perform(0,e.id);};$('units').append(b);
  }
  $('items').replaceChildren();
  for(const item of state.items) {const p=document.createElement('p');p.textContent=`${item.id}: ${item.name} — ${item.holder>=0?'carried by '+item.holder:item.holder===-2?'delivered / consumed':`(${item.r+1},${item.c+1})${item.dropped?' dropped':''}`}`;$('items').append(p);}
  $('reaction').hidden=state.pending<0||state.outcome!==0;
  $('reaction-text').textContent=`Alien ${state.pending} is the reaction target. Choose a Marine, fire or decline, then finish reactions. A jam cancels the whole burst.`;
  const oldValue=$('react-marine').value;$('react-marine').replaceChildren();
  for(const e of state.entities.filter(e=>e.team===0&&alive(e)&&e.ow)) {const o=new Option(`${e.id} / ${label(e)}`,e.id);$('react-marine').add(o);}
  if([...$('react-marine').options].some(o=>o.value===oldValue))$('react-marine').value=oldValue;
  $('arrivals').hidden=state.phase!==1||state.outcome!==0;
  for(const b of document.querySelectorAll('.actions button,.turns button,#entries button,#ready,#reaction button'))b.disabled=locked||state.outcome!==0;
  $('react-fire').disabled=locked||!$('react-marine').options.length;
  $('react-hold').disabled=locked||!$('react-marine').options.length;
}
function label(e) {return e.team===0?names[e.id]||'Marine':e.team===2?'Contact':'Alien '+bugNames[e.bug];}
function select(r,c,target=-1) {
  selected={r,c};$('row').value=r+1;$('col').value=c+1;$('target').value=target;
  const tile=state.map[r]?.[c];$('square').textContent=`Square (${r+1},${c+1}) / ${tile}${target>=0?' / target '+target:''}`;
}
function floor(r,c) {return r>=0&&r<25&&c>=0&&c<31&&state.map[r][c]!=='#';}
function doorAngle(r,c) {return floor(r,c-1)&&floor(r,c+1)?Math.PI/2:0;}
function render(now) {
  if(state&&manifest) {
    ctx.fillStyle='#181b1c';ctx.fillRect(0,0,1024,1024);
    const progress=$('motion').checked?Math.min(1,Math.max(0,(now-clock)/260)):1;
    for(let row=0;row<32;row++)for(let c=0;c<32;c++) {
      const r=row-OFFSET,p=point(r,c),t=state.map[r]?.[c];
      if(!t) {sprite('vacuum',p.x,p.y,0,1,.12);continue;}
      if(t==='#') {
        const adjacent=[[r-1,c],[r,c+1],[r+1,c],[r,c-1]].map(([y,x])=>floor(y,x));
        let name='hull-solid',angle=0;
        if(adjacent.filter(Boolean).length===1) {name='hull-edge';angle=adjacent.indexOf(true)*Math.PI/2;}
        sprite(name,p.x,p.y,angle,1,.65);
        // The outer wall glow breathes very slightly; collisions never move.
        if($('motion').checked&&adjacent.some(Boolean)) {ctx.fillStyle=`rgba(255,255,255,${.015+.01*Math.sin(now/900+c+r)})`;ctx.fillRect(c*32,row*32,32,32);}
      } else {
        ctx.fillStyle='#9a9993';ctx.fillRect(c*32,row*32,32,32);
        sprite((r+c)%9===0?'floor-cables':(r+c)%5===0?'floor-grate':'floor-plate',p.x,p.y,0,1,.8);
        const d=doors.find(d=>d.r===r&&d.c===c);
        if(d&&progress<1) {if(d.breach)sprite('breach-rubble',p.x,p.y);else {let frame=Math.round(progress*4);if(!d.open)frame=4-frame;sprite(manifest.doorFrames[frame],p.x,p.y,doorAngle(r,c));}}
        else if(t==='+'||t==='/')sprite(t==='+'?'door-closed':'door-open',p.x,p.y,doorAngle(r,c));
        else {const mark=marker(r,c,t);if(mark)sprite(mark,p.x,p.y,0,.72);}
      }
      ctx.strokeStyle='rgba(15,15,15,.24)';ctx.lineWidth=.5;ctx.strokeRect(c*32,row*32,32,32);
      if(t&&t!=='.'&&t!=='#'&&t!=='+'&&t!=='/') {ctx.font='bold 9px monospace';ctx.fillStyle='#111';ctx.fillRect(c*32,row*32,10,11);ctx.fillStyle='#fff';ctx.fillText(t,c*32+2,row*32+9);}
    }
    for(const item of state.items.filter(i=>i.holder===-1&&i.dropped)) {const p=point(item.r,item.c);sprite(manifest.items[item.name],p.x,p.y,0,.58);}
    for(const e of state.entities) {
      if(e.extracted)continue;
      const move=motions.get(e.id),m=move&&progress<1;
      const r=m?move.from.r+(e.r-move.from.r)*progress:e.r,c=m?move.from.c+(e.c-move.from.c)*progress:e.c,p=point(r,c);
      let angle=m?move.a+(move.b-move.a)*progress:e.team===0?e.facing*Math.PI/2:bugFacing.get(e.id)||0;
      if(e.team===1)angle-=manifest.bugs[bugNames[e.bug]].artFacing*Math.PI/2;
      let name=e.team===0?'crew-'+weaponNames[e.weapon]+'-idle':e.team===2?'blip':'bug-'+bugNames[e.bug];
      if(!e.alive) {sprite(e.team===0?'marine-fallen':'bug-fallen',p.x,p.y,angle,.82,.4);continue;}
      if(m&&(move.from.r!==e.r||move.from.c!==e.c)) {
        const pose=Math.floor(progress*4)%2===0?'left':'right';name=e.team===0?'crew-'+weaponNames[e.weapon]+'-'+pose:e.team===1?'bug-'+bugNames[e.bug]+'-'+pose:'blip';
      }
      if(e.ow)sprite('overwatch',p.x,p.y,angle,1.04,.85);
      sprite(name,p.x,p.y,angle,e.team===2?.7:1);
      if(e.jam)sprite('jam',p.x+8,p.y+8,0,.38);
      if(e.stunned)sprite('stunned',p.x,p.y,0,1,.65);
      if(e.item>=0)sprite(manifest.items[state.items.find(i=>i.id===e.item)?.name]||'recorder',p.x+10,p.y-8,0,.35);
      if(e.id===state.active) {ctx.strokeStyle='#fff';ctx.lineWidth=2;ctx.strokeRect(p.x-15,p.y-15,30,30);}
      ctx.fillStyle='#111';ctx.fillRect(p.x-15,p.y+7,13,10);ctx.fillStyle='#fff';ctx.font='bold 8px monospace';ctx.fillText(e.id,p.x-14,p.y+15);
      if(e.team===0) {ctx.save();ctx.translate(p.x,p.y);ctx.rotate(angle);ctx.fillStyle='#fff';ctx.beginPath();ctx.moveTo(0,-15);ctx.lineTo(-3,-11);ctx.lineTo(3,-11);ctx.fill();ctx.restore();}
    }
    effects=effects.filter(e=>now<e.start+e.life);
    for(const fx of effects) {
      const t=Math.max(0,Math.min(1,(now-fx.start)/fx.life)),a=point(fx.from.r,fx.from.c),b=point(fx.to.r,fx.to.c),angle=heading(fx.from,fx.to);
      if(fx.weapon==='jam') {sprite('jam',a.x,a.y,0,1,1-t);continue;}
      if(fx.weapon==='claw') {sprite('claw-slash',b.x,b.y,angle,1,1-t);continue;}
      const spec=manifest.weapons[fx.weapon]||manifest.grenades[fx.weapon]||manifest.spitter;
      if(spec.muzzle&&t<.3)sprite(spec.muzzle,a.x,a.y,angle,1,1-t);
      if(t<.6) {const travel=t/.6;sprite(spec.projectile,a.x+(b.x-a.x)*travel,a.y+(b.y-a.y)*travel,angle,fx.weapon==='flame'?1.4:.8);}
      else if(fx.hit) {
        sprite(spec.impact,b.x,b.y,0,1+(t-.6),1-(t-.6)*2);
        if(['flame','frag','stun'].includes(fx.weapon))for(const [dr,dc] of [[-1,0],[1,0],[0,-1],[0,1]])if(floor(fx.to.r+dr,fx.to.c+dc)&&!['#','+'].includes(state.map[fx.to.r+dr]?.[fx.to.c+dc]))sprite(spec.impact,b.x+dc*32,b.y+dr*32,0,.8,(1-t)*1.6);
      }
    }
    const p=point(selected.r,selected.c);ctx.strokeStyle='#fff';ctx.setLineDash([3,3]);ctx.lineWidth=1;ctx.strokeRect(p.x-16,p.y-16,32,32);ctx.setLineDash([]);
  }
  requestAnimationFrame(render);
}
async function start() {
  if(locked)return;
  const seed=Number($('seed').value);if(!Number.isInteger(seed)||seed<0||seed>4294967295){log('Seed must be an integer from 0 to 4294967295.',true);return;}
  state=null;history=[];$('log').replaceChildren();batch=[];effects=[];motions.clear();bugFacing.clear();lastResult='';
  missionId=Number($('mission').value);gameSeed=seed;
  if(!engine.start(missionId,seed,Number($('specialist').value),Number($('house').value)))throw Error('Unable to create mission.');
  read();batch=[];doors=[];$('briefing').textContent=engine.objective(missionId);update();select(22,12);
}
$('map').onclick=e=> {if(!state||locked)return;const rect=canvas.getBoundingClientRect(),c=Math.floor((e.clientX-rect.left)/rect.width*32),r=Math.floor((e.clientY-rect.top)/rect.height*32)-OFFSET;if(r<0||r>=25||c<0||c>=31)return;const target=state.entities.find(u=>alive(u)&&u.r===r&&u.c===c);select(r,c,target?.id??-1);};
$('select-square').onclick=()=>{if(!state)return;const r=Number($('row').value)-1,c=Number($('col').value)-1;if(Number.isInteger(r)&&Number.isInteger(c)&&r>=0&&r<25&&c>=0&&c<31)select(r,c,Number($('target').value));else log('Choose a book row 1–25 and column 1–31.',true);};
$('actions').onclick=e=>{if(!e.target.matches('[data-action]'))return;const a=Number(e.target.dataset.action),target=Number($('target').value);perform(1,a,selected.r,selected.c,a===10?Number($('item').value):a===14?0:target);};
for(const b of document.querySelectorAll('[data-facing]'))b.onclick=()=>perform(1,1,0,0,Number(b.dataset.facing));
$('interact-frag').onclick=()=>perform(1,9,selected.r,selected.c,1);
$('end').onclick=()=>perform(2);$('phase').onclick=()=>perform(3);$('ready').onclick=()=>perform(5);$('resolve').onclick=()=>perform(7);
$('react-fire').onclick=()=>perform(6,Number($('react-marine').value),1);$('react-hold').onclick=()=>perform(6,Number($('react-marine').value),0);
for(const entry of 'ABCDEF') {const b=document.createElement('button');b.textContent=entry;b.onclick=()=>perform(4,entry.charCodeAt(0));$('entries').append(b);}
$('viewer').onchange=()=>{if(engine){read();update();}};
$('zoom').onchange=()=>{canvas.style.width=$('zoom').value==='fit'?'min(100%,70vh)':Number($('zoom').value)*32+'px';canvas.style.height=$('zoom').value==='fit'?'auto':Number($('zoom').value)*32+'px';};
$('motion').onchange=()=>{effects=[];motions.clear();doors=[];};
$('save-log').onclick=()=>{const blob=new Blob([`Simple Hulk / mission ${missionId} / seed ${gameSeed}\n`+history.join('\n')+'\n'],{type:'text/plain'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download='simple-hulk-game.log';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
$('new').onclick=()=>start().catch(e=>log(e.message,true));
try {
  manifest=await fetch('../assets/manifest.json').then(r=>{if(!r.ok)throw Error('Asset manifest unavailable.');return r.json();});
  await Promise.all(Object.entries(manifest.sheets).map(([name,s])=>new Promise((resolve,reject)=>{const image=new Image();image.onload=()=>{sheets[name]=image;resolve();};image.onerror=()=>reject(Error(`Missing artwork: ${s.file}`));image.src='../assets/'+s.file;})));
  engine=await loadEngine('simple-hulk.wasm',feedback);
  for(let i=0;i<9;i++)$('mission').add(new Option(`${i} / ${engine.name(i)}`,i));
  $('new').disabled=false;
  await start();requestAnimationFrame(render);
} catch(e) {$('status').textContent=`Unable to start: ${e.message} Run make wasm and serve the repository over HTTP (make serve), then open /web/.`;log(e.message,true);}
