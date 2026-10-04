import assert from 'node:assert/strict';
import { readFileSync, existsSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
const bytes=readFileSync('web/simple-hulk.wasm');
assert(WebAssembly.validate(bytes));
let instance,events=[],checks=0;
const decode=p=>{const b=new Uint8Array(instance.exports.memory.buffer);let end=p;while(b[end])end++;return new TextDecoder().decode(b.subarray(p,end));};
({instance}=await WebAssembly.instantiate(bytes,{env:{feedback:(type,actor,target,row,col,p,round)=>events.push({type,actor,target,row,col,round,message:decode(p)})}}));
const api=instance.exports,snapshot=(viewer=0)=>JSON.parse(decode(api.snapshot(viewer)));
function check(value,message){assert(value,message);checks++;}
const manifest=JSON.parse(readFileSync('assets/manifest.json','utf8'));
for(const sheet of Object.values(manifest.sheets)) {
 check(existsSync('assets/'+sheet.file),`Missing ${sheet.file}`);
 const png=readFileSync('assets/'+sheet.file);check(png.subarray(1,4).toString()==='PNG','PNG signature');
 check(png.readUInt32BE(16)>sheet.columns*32&&png.readUInt32BE(20)>sheet.rows*32,'Atlas resolution');
}
for(const [name,s] of Object.entries(manifest.sprites)) {const sheet=manifest.sheets[s.sheet];check(!!sheet&&s.index>=0&&s.index<sheet.rows*sheet.columns,`Atlas bounds: ${name}`);}
const required=['floor-plate','floor-grate','floor-cables','hull-solid','hull-edge','hull-outer-corner','hull-inner-corner','hull-cracked','door-damaged','breach-rubble','vacuum','blip','claw-slash','marine-fallen','bug-fallen','stunned','overwatch','jam'];
for(const w of Object.values(manifest.weapons)) {required.push(w.projectile,w.impact);if(w.muzzle)required.push(w.muzzle);for(const pose of ['idle','left','right'])required.push(w.actor+'-'+pose);}
for(const b of Object.values(manifest.bugs)) {required.push(b.actor,...b.walk);check(Number.isInteger(b.artFacing)&&b.artFacing>=0&&b.artFacing<4,'Bug source-art heading');}
for(const g of Object.values(manifest.grenades))required.push(g.projectile,g.impact);
required.push(...Object.values(manifest.mapMarkers),...Object.values(manifest.items),...manifest.doorFrames,manifest.spitter.projectile,manifest.spitter.impact);
for(const name of required)check(!!manifest.sprites[name],`Missing required art: ${name}`);
function command(trace,op,a=0,b=0,c=0,d=-1){const args=[op,a,b,c,d],result=api.command(...args);trace.push({args,result,state:snapshot()});return result;}
for(let mission=0;mission<9;mission++) {
 const specialist=mission%4;events=[];check(api.start(mission,2026,specialist,0)===1,'Create mission');
 const initial=snapshot(),trace=[];
 check(initial.map.length===25&&initial.map.every(row=>row.length===31),'Book map dimensions');
 check(initial.entities.filter(e=>e.team===0).length===5,'Five Marines');
 check(initial.entities.filter(e=>e.team===2).every(e=>e.strength===-1),'Marine view hides contact strengths');
 check(snapshot(1).entities.filter(e=>e.team===2).every(e=>e.strength>0),'Alien view exposes contact strengths');
 check(command(trace,0,0)===0,'Activate');const m=snapshot().entities[0];
 check(command(trace,1,0,m.r-1,m.c,-1)===0,'Move north');check(snapshot().entities[0].ap===3,'Movement costs AP');
 check(command(trace,1,0,0,0,-1)!==0,'Reject teleport');
 check(command(trace,1,7,0,0,-1)===0,'Overwatch');check(snapshot().entities[0].ow===1&&snapshot().active===-1,'Overwatch ends activation');
 // Exercise shots, grenades, facing, and interactions while respecting public API rejection.
 for(let id=1;id<5;id++) {
  command(trace,0,id);let e=snapshot().entities[id];
  command(trace,1,1,0,0,1);command(trace,1,1,0,0,0);
  command(trace,1,3,e.r-1,e.c,5);command(trace,1,5,e.r-1,e.c,-1);command(trace,1,6,e.r-1,e.c,-1);command(trace,1,9,0,0,-1);command(trace,2);
 }
 // Every mission reaches its real deadline. Alien movement additionally exercises reveals and reactions.
 let guard=0;
 while(!snapshot().outcome&&guard++<100) {
  const s=snapshot();
  if(s.pending>=0) {for(const e of s.entities.filter(e=>e.team===0&&e.alive&&e.ow))command(trace,6,e.id,1);command(trace,7);continue;}
  if(s.phase===0) {if(s.active>=0)command(trace,2);command(trace,3);}
  else if(s.phase===1) {for(const entry of 'ABCDEF')command(trace,4,entry.charCodeAt(0));command(trace,5);}
  else if(s.phase===2) {
   for(const e of s.entities.filter(e=>e.team!==0&&e.alive&&!e.done)) {
    if(snapshot().outcome)break;command(trace,0,e.id);
    for(const [dr,dc] of [[1,0],[-1,0],[0,1],[0,-1]]) {const current=snapshot().entities[e.id];if(snapshot().pending>=0)break;command(trace,1,0,current.r+dr,current.c+dc,-1);}
    if(snapshot().pending>=0){for(const m of snapshot().entities.filter(e=>e.team===0&&e.alive&&e.ow))command(trace,6,m.id,1);command(trace,7);}
    if(snapshot().active>=0)command(trace,2);
   }
   command(trace,3);
  }
 }
 check(snapshot().outcome!==0,`Mission ${mission} finishes`);
 const native=spawnSync('./build/wasm-parity',[String(mission),String(specialist)],{input:trace.map(t=>t.args.join(' ')).join('\n')+'\n',encoding:'utf8',maxBuffer:30*1024*1024});
 assert.equal(native.status,0,native.stderr);const lines=native.stdout.trim().split('\n');
 assert.deepEqual(JSON.parse(lines[0]),initial);
 trace.forEach((t,i)=>{assert.equal(Number(lines[1+i*2]),t.result,`Result parity mission ${mission} command ${i}`);assert.deepEqual(JSON.parse(lines[2+i*2]),t.state,`State parity mission ${mission} command ${i}`);checks+=2;});
 check(events.some(e=>e.type===1),'Movement feedback');check(events.some(e=>e.type===3),'Overwatch feedback');
 // Replaying the same trace and seed must produce exactly the same event stream.
 const expectedEvents=JSON.stringify(events);events=[];api.start(mission,2026,specialist,0);for(const t of trace)api.command(...t.args);
 check(JSON.stringify(events)===expectedEvents,'Deterministic feedback replay');
 console.log(`Mission ${mission}: native/WASM parity for ${trace.length} commands, legal outcome ${snapshot().outcome}.`);
}
console.log(`PASS: ${checks} checks; all required assets present; all nine missions complete with native/WASM parity.`);
