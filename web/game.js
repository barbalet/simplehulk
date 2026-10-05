import { loadEngine } from './engine.js';
import { CLOSED_DOOR, OPEN_DOOR, isWall, isFloor, wallMask } from './tiles.js';
const $ = id => document.getElementById(id);
const canvas = $('map'), ctx = canvas.getContext('2d');
const SIZE = 32, OFFSET = 3, names = ['Vale','Iona','Kes','Rook','Sen'];
const weaponNames = ['rifle','scattergun','cannon','flame'], bugNames = ['basic','skitter','brute','spitter'];
let engine, state, previous, manifest, sheets = {}, selected = {r:22,c:12}, effects = [], doors = [], motions = new Map(), bugFacing = new Map();
let menuUnit = -1, intent = null, cellSize = 64, pointers = new Map(), gesture = null, pinch = null;
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
function read() { const view=$('viewer').value; state = engine.snapshot(view==='auto'?(state && state.phase!==0?1:0):Number(view)); if(view==='auto')state=engine.snapshot(state.phase===0?0:1); }
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
    const a=before.map[r][c],b=after.map[r][c];if(a!==b&&(a===CLOSED_DOOR||a===OPEN_DOOR||b===CLOSED_DOOR))doors.push({r,c,open:b!==CLOSED_DOOR,breach:b==='.'});
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
  $('status').textContent=`Round ${state.round}/${state.deadline} · ${phases[state.phase]} · ${active?label(active)+' / '+active.ap+' AP':'select a piece'} · objectives ${state.objectives} · evacuated ${state.evacuated}${state.outcome?' · '+(state.outcome===1?'Marines win':'Aliens win'):''}${lastResult?' · '+lastResult:''}`;
  $('phase').textContent=state.pending>=0?'Resume alien attack':state.phase===1?'Finish deployment':'End phase';
  $('phase').disabled=locked||!!state.outcome;
  $('do-action').disabled=locked||!!state.outcome;
  if(menuUnit>=0&&!$('context').hidden)populateMenu();
}
function label(e) {return e.team===0?names[e.id]||'Marine':e.team===2?'Contact':'Alien '+bugNames[e.bug];}
function select(r,c,target=-1) {
  selected={r,c};const e=get(state,target),tile=state.map[r]?.[c];
  const kind=tile===' '?'exterior space':isWall(tile)?'hull wall':tile===CLOSED_DOOR?'closed door':tile===OPEN_DOOR?'open door':tile;
  $('square').textContent=`(${r+1},${c+1}) · ${e?label(e)+' · '+e.ap+' AP':kind}`;
}
function closeMenu(){ $('context').hidden=true;menuUnit=-1; }
function cancelIntent(){intent=null;canvas.classList.remove('targeting');$('hint').textContent='Click a friendly piece to activate; click it again for actions. Click an adjacent square or drag the active piece to move. Drag empty space to pan; scroll or pinch to zoom.';}
function populateMenu(){
  const e=get(state,menuUnit);if(!e||!alive(e)){closeMenu();return;}
  $('context-name').textContent=label(e);
  $('context-detail').textContent=e.team===0?`${e.ap} AP · ${weaponNames[e.weapon]} · ammo ${e.ammo<0?'∞':e.ammo} · frag ${e.frag} / stun ${e.stun}${e.ow?' · OVERWATCH':''}${e.jam?' · JAM':''}`:`${e.ap} AP · ${e.team===2?'strength '+(e.strength<0?'hidden':e.strength):bugNames[e.bug]}`;
  const old=$('unit-action').value, options=[];
  const add=(v,t)=>options.push(new Option(t,v));
  if(state.pending>=0){if(e.team===0&&e.ow){add('reaction:1','Fire reaction');add('reaction:0','Decline reaction');}}
  else if(e.id!==state.active){if(!e.done&&((state.phase===0&&e.team===0)||(state.phase===2&&e.team!==0)))add('activate','Activate piece');}
  else {
    if(e.team===0){if(e.ap>=2&&!e.jam&&e.weapon!==3)add('now:7','Overwatch · 2 AP');if(e.jam)add('now:8','Clear jam');}
    add('target:0','Move → square');
    for(const [i,name] of ['North ↑','East →','South ↓','West ←'].entries())add('turn:'+i,'Face '+name);
    if(e.team!==2){add('target:3',e.weapon===3&&e.team===0?'Flame → square':'Fire → enemy');add('target:4','Melee → enemy');}
    if(e.team===0){if(e.frag)add('target:5','Frag → square');if(e.stun)add('target:6','Stun → square');add('target:2','Open / close → door');add('target:13','Breach → door');add('target:9','Interact / inspect → square');add('frag','Take frag → supply / locker');add('target:14','Take ammo → supply');if(e.item>=0)add('target:11','Transfer item → Marine');for(const item of state.items.filter(i=>i.holder===-1&&i.dropped))add('pickup:'+item.id,'Pick up '+item.name+' → square');}
    if(e.team===2)add('now:12','Reveal contact');
    add('end','End activation');
  }
  $('unit-action').replaceChildren(...options);
  if(options.some(o=>o.value===old))$('unit-action').value=old;
  $('do-action').disabled=locked||!!state.outcome||!options.length;
}
function openMenu(e){
  menuUnit=e.id;$('context').hidden=false;populateMenu();
  const rect=canvas.getBoundingClientRect(),p=point(e.r,e.c),x=rect.left+p.x*rect.width/1024,y=rect.top+p.y*rect.height/1024;
  $('context').style.left=Math.max(8,Math.min(innerWidth-300,x+cellSize/2))+'px';
  $('context').style.top=Math.max(8,Math.min(innerHeight-260,y-cellSize/2))+'px';
}
function cellAt(x,y){const rect=canvas.getBoundingClientRect();return {r:Math.floor((y-rect.top)/rect.height*32)-OFFSET,c:Math.floor((x-rect.left)/rect.width*32)};}
function occupant(p){return state.entities.find(e=>alive(e)&&e.r===p.r&&e.c===p.c);}
function boardClick(p){
  if(!state||locked||p.r<0||p.r>=25||p.c<0||p.c>=31)return;
  const e=occupant(p);select(p.r,p.c,e?.id??-1);
  if(intent){const chosen=intent;cancelIntent();perform(1,chosen.action,p.r,p.c,chosen.item??chosen.option??e?.id??-1);return;}
  closeMenu();
  if(state.phase===1&&/^[A-F]$/.test(state.map[p.r][p.c])){perform(4,state.map[p.r][p.c].charCodeAt(0));return;}
  if(e){
    if(state.pending<0&&state.active<0&&!e.done&&((state.phase===0&&e.team===0)||(state.phase===2&&e.team!==0)))perform(0,e.id);
    openMenu(get(state,e.id));return;
  }
  const active=get(state,state.active);
  if(active&&state.pending<0&&isFloor(state.map[p.r][p.c])&&Math.abs(active.r-p.r)+Math.abs(active.c-p.c)===1)perform(1,0,p.r,p.c);
}
function focusUnit(){const e=get(state,state.active)||state.entities.find(e=>alive(e)&&e.team===(state.phase===0?0:1))||state.entities.find(alive);if(!e)return;const p=point(e.r,e.c),box=canvas.parentElement;box.scrollLeft=p.x/32*cellSize-box.clientWidth/2;box.scrollTop=p.y/32*cellSize-box.clientHeight/2;select(e.r,e.c,e.id);}
function zoomTo(size,x,y){const box=canvas.parentElement,rect=box.getBoundingClientRect(),px=x??rect.left+box.clientWidth/2,py=y??rect.top+box.clientHeight/2,ox=px-rect.left,oy=py-rect.top,old=cellSize;cellSize=Math.max(8,Math.min(128,size));canvas.style.width=canvas.style.height=cellSize*32+'px';box.scrollLeft=(box.scrollLeft+ox)*cellSize/old-ox;box.scrollTop=(box.scrollTop+oy)*cellSize/old-oy;$('zoom-level').textContent=Math.round(cellSize)+' px';closeMenu();}
function floor(r,c) {return isFloor(state.map[r]?.[c]);}
function corridor(r,c) {const tile=state.map[r]?.[c];return isFloor(tile)||tile===CLOSED_DOOR;}
function doorAngle(r,c) {return corridor(r,c-1)&&corridor(r,c+1)?Math.PI/2:0;}
function render(now) {
  if(state&&manifest) {
    ctx.fillStyle='#181b1c';ctx.fillRect(0,0,1024,1024);
    const progress=$('motion').checked?Math.min(1,Math.max(0,(now-clock)/260)):1;
    for(let row=0;row<32;row++)for(let c=0;c<32;c++) {
      const r=row-OFFSET,p=point(r,c),t=state.map[r]?.[c];
      if(!t||t===' ') {sprite(manifest.hull.exterior,p.x,p.y,((r+c)%4)*Math.PI/2,1,.25);continue;}
      if(isWall(t)) {
        const mask=wallMask(state.map,r,c);
        sprite(manifest.hull.connections[mask],p.x,p.y,0,1,1);
        // Visual light only: hull cells remain fixed and inaccessible.
        if($('motion').checked) {sprite(manifest.hull.connections[mask],p.x,p.y,0,1,.03+.025*Math.sin(now/900+c+r));}
      } else {
        ctx.fillStyle='#9a9993';ctx.fillRect(c*32,row*32,32,32);
        sprite((r+c)%9===0?'floor-cables':(r+c)%5===0?'floor-grate':'floor-plate',p.x,p.y,0,1,.8);
        const d=doors.find(d=>d.r===r&&d.c===c);
        if(d&&progress<1) {if(d.breach)sprite('breach-rubble',p.x,p.y);else {let frame=Math.round(progress*4);if(!d.open)frame=4-frame;sprite(manifest.doorFrames[frame],p.x,p.y,doorAngle(r,c));}}
        else if(t===CLOSED_DOOR||t===OPEN_DOOR)sprite(t===CLOSED_DOOR?'door-closed':'door-open',p.x,p.y,doorAngle(r,c));
        else {const mark=marker(r,c,t);if(mark)sprite(mark,p.x,p.y,0,.72);}
      }
      ctx.strokeStyle='rgba(15,15,15,.24)';ctx.lineWidth=.5;ctx.strokeRect(c*32,row*32,32,32);
      if(t&&/^[A-Z1-3]$/.test(t)) {ctx.font='bold 9px monospace';ctx.fillStyle='#111';ctx.fillRect(c*32,row*32,10,11);ctx.fillStyle='#fff';ctx.fillText(t,c*32+2,row*32+9);}
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
      if(e.id===state.active||(state.pending>=0&&e.team===0&&e.ow)) {ctx.strokeStyle='#fff';ctx.lineWidth=2;ctx.strokeRect(p.x-15,p.y-15,30,30);}
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
        if(['flame','frag','stun'].includes(fx.weapon))for(const [dr,dc] of [[-1,0],[1,0],[0,-1],[0,1]])if(floor(fx.to.r+dr,fx.to.c+dc))sprite(spec.impact,b.x+dc*32,b.y+dr*32,0,.8,(1-t)*1.6);
      }
    }
    const active=get(state,state.active);
    if(active&&state.pending<0&&!intent)for(const [dr,dc] of [[-1,0],[1,0],[0,-1],[0,1]]) {
      const r=active.r+dr,c=active.c+dc;if(floor(r,c)&&!occupant({r,c})){const p=point(r,c);ctx.strokeStyle='rgba(255,255,255,.7)';ctx.lineWidth=2;ctx.strokeRect(p.x-12,p.y-12,24,24);}
    }
    if(gesture?.drag&&gesture.moved&&active){const p=point(selected.r,selected.c);sprite(active.team===0?'crew-'+weaponNames[active.weapon]+'-idle':active.team===2?'blip':'bug-'+bugNames[active.bug],p.x,p.y,active.facing*Math.PI/2,1,.5);}
    const p=point(selected.r,selected.c);ctx.strokeStyle='#fff';ctx.setLineDash([3,3]);ctx.lineWidth=1;ctx.strokeRect(p.x-16,p.y-16,32,32);ctx.setLineDash([]);
  }
  requestAnimationFrame(render);
}
async function start() {
  if(locked)return;
  const seed=Number($('seed').value);if(!Number.isInteger(seed)||seed<0||seed>4294967295){log('Seed must be an integer from 0 to 4294967295.',true);return;}
  state=null;history=[];$('log').replaceChildren();batch=[];effects=[];motions.clear();bugFacing.clear();lastResult='';
  const variant=JSON.parse($('mission').value);missionId=variant.mission;gameSeed=seed;cancelIntent();closeMenu();
  if(!engine.start(missionId,seed,variant.bug,variant.house))throw Error('Unable to create mission.');
  read();batch=[];doors=[];$('briefing').textContent=engine.objective(missionId);update();focusUnit();
}
$('do-action').onclick=()=>{
  const value=$('unit-action').value,[kind,arg]=value.split(':'),e=get(state,menuUnit);if(!e||locked)return;
  closeMenu();cancelIntent();
  if(kind==='activate')perform(0,e.id);
  else if(kind==='end')perform(2);
  else if(kind==='reaction')perform(6,e.id,Number(arg));
  else if(kind==='turn')perform(1,1,0,0,Number(arg));
  else if(kind==='now')perform(1,Number(arg),e.r,e.c);
  else {intent={action:kind==='pickup'?10:kind==='frag'?9:Number(arg)};if(kind==='pickup')intent.item=Number(arg);if(kind==='frag')intent.option=1;if(intent.action===14)intent.option=0;canvas.classList.add('targeting');$('hint').textContent=`${label(e)}: ${value==='frag'?'take frag':$('unit-action').selectedOptions[0]?.textContent||'choose action'} — click the target on the map. Escape cancels.`;}
};
$('close-menu').onclick=closeMenu;
$('phase').onclick=()=>{cancelIntent();closeMenu();perform(state.pending>=0?7:state.phase===1?5:3);};
$('viewer').onchange=()=>{if(engine){read();closeMenu();cancelIntent();update();}};
$('focus').onclick=focusUnit;
$('zoom-in').onclick=()=>zoomTo(cellSize*1.25);
$('zoom-out').onclick=()=>zoomTo(cellSize/1.25);
$('fit').onclick=()=>{const box=canvas.parentElement;zoomTo(Math.min(box.clientWidth,box.clientHeight)/32);box.scrollTop=box.scrollLeft=0;};
canvas.addEventListener('wheel',e=>{e.preventDefault();zoomTo(cellSize*Math.exp(-e.deltaY*.002),e.clientX,e.clientY);},{passive:false});
canvas.addEventListener('pointerdown',e=>{
  if(!state)return;canvas.focus();canvas.setPointerCapture(e.pointerId);pointers.set(e.pointerId,{x:e.clientX,y:e.clientY});
  if(pointers.size===2){const [a,b]=[...pointers.values()];pinch={distance:Math.max(1,Math.hypot(a.x-b.x,a.y-b.y)),size:cellSize};gesture=null;return;}
  const p=cellAt(e.clientX,e.clientY),unit=occupant(p),box=canvas.parentElement;
  gesture={x:e.clientX,y:e.clientY,left:box.scrollLeft,top:box.scrollTop,unit:unit?.id,moved:false,drag:unit?.id===state.active&&state.pending<0&&!intent};
});
canvas.addEventListener('pointermove',e=>{
  if(!pointers.has(e.pointerId))return;pointers.set(e.pointerId,{x:e.clientX,y:e.clientY});
  if(pinch&&pointers.size===2){const [a,b]=[...pointers.values()];zoomTo(pinch.size*Math.hypot(a.x-b.x,a.y-b.y)/pinch.distance,(a.x+b.x)/2,(a.y+b.y)/2);return;}
  if(!gesture)return;const dx=e.clientX-gesture.x,dy=e.clientY-gesture.y;
  if(Math.hypot(dx,dy)>6){gesture.moved=true;closeMenu();if(!gesture.drag){canvas.parentElement.scrollLeft=gesture.left-dx;canvas.parentElement.scrollTop=gesture.top-dy;}else{const p=cellAt(e.clientX,e.clientY);if(p.r>=0&&p.r<25&&p.c>=0&&p.c<31)select(p.r,p.c);}}
});
canvas.addEventListener('pointerup',e=>{
  pointers.delete(e.pointerId);if(pinch){if(pointers.size===0)pinch=null;gesture=null;return;}
  const g=gesture;gesture=null;if(!g)return;const p=cellAt(e.clientX,e.clientY);
  if(!g.moved)boardClick(p);else if(g.drag&&!locked){const active=get(state,state.active);if(active&&Math.abs(active.r-p.r)+Math.abs(active.c-p.c)===1)boardClick(p);else $('hint').textContent='Drag to an adjacent square. Each move spends AP; choose the next step on the board.';}
});
canvas.addEventListener('pointercancel',e=>{pointers.delete(e.pointerId);gesture=null;pinch=null;});
canvas.addEventListener('keydown',e=>{
  const deltas={ArrowUp:[-1,0],ArrowDown:[1,0],ArrowLeft:[0,-1],ArrowRight:[0,1]};
  if(deltas[e.key]){e.preventDefault();const [dr,dc]=deltas[e.key];select(Math.max(0,Math.min(24,selected.r+dr)),Math.max(0,Math.min(30,selected.c+dc)));}
  else if(e.key==='Enter'){e.preventDefault();boardClick(selected);}
  else if(e.key.toLowerCase()==='m'){const unit=occupant(selected)||get(state,state.active);if(unit)openMenu(unit);}
});
document.addEventListener('keydown',e=>{if(e.key==='Escape'){closeMenu();cancelIntent();}});
canvas.parentElement.addEventListener('scroll',closeMenu);
$('motion').onchange=()=>{effects=[];motions.clear();doors=[];};
$('save-log').onclick=()=>{const blob=new Blob([`Simple Hulk / mission ${missionId} / seed ${gameSeed}\n`+history.join('\n')+'\n'],{type:'text/plain'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download='simple-hulk-game.log';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
$('new').onclick=()=>start().catch(e=>log(e.message,true));
try {
  manifest=await fetch('../assets/manifest.json').then(r=>{if(!r.ok)throw Error('Asset manifest unavailable.');return r.json();});
  await Promise.all(Object.entries(manifest.sheets).map(([name,s])=>new Promise((resolve,reject)=>{const image=new Image();image.onload=()=>{sheets[name]=image;resolve();};image.onerror=()=>reject(Error(`Missing artwork: ${s.file}`));image.src='../assets/'+s.file;})));
  engine=await loadEngine('simple-hulk.wasm',feedback);
  const houses=[[0,'Standard'],[1,'Training (+2 phases)'],[2,'Hard vacuum'],[4,'Reliable bursts'],[8,'Ghost contacts'],[16,'Sealed bulkheads'],[32,'Supply cache']];
  for(let i=0;i<9;i++){const group=document.createElement('optgroup');group.label=engine.name(i);for(let bug=0;bug<4;bug++)for(const [house,title] of houses)group.append(new Option(`${engine.name(i)} · ${title}${bug?' · '+bugNames[bug]:''}`,JSON.stringify({mission:i,bug,house})));$('mission').append(group);}
  $('new').disabled=false;
  await start();requestAnimationFrame(render);
} catch(e) {$('status').textContent=`Unable to start: ${e.message} Run make wasm and serve the repository over HTTP (make serve), then open /web/.`;log(e.message,true);}
