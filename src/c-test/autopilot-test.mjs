import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {autoCommands} from '../../web/autopilot.js';
let instance,events=[];
const decode=p=>{const b=new Uint8Array(instance.exports.memory.buffer);let end=p;while(b[end])end++;return new TextDecoder().decode(b.subarray(p,end));};
({instance}=await WebAssembly.instantiate(readFileSync('web/simple-hulk.wasm'),{env:{feedback:(type,actor,target,row,col,p)=>events.push({type,message:decode(p)})}}));
const api=instance.exports;
const engine={snapshot:view=>JSON.parse(decode(api.snapshot(view))),layout:()=>JSON.parse(decode(api.layout())),canShoot:(a,b)=>!!api.can_shoot(a,b)};
let doors=0,weapons=0,overwatch=0,reactions=0;
for(let mission=0;mission<9;mission++){
  for(const seed of [42,2026]){
    assert.equal(api.start(mission,seed,mission%4,0),1);events=[];
    let ticks=0;
    while(!engine.snapshot(0).outcome&&ticks++<4000){
      const before=engine.snapshot(0),candidates=autoCommands(engine,mission);
      assert(candidates.length,'Ongoing game must have a next step');
      let success=false;
      for(const args of candidates){if(api.command(...args)===0){success=true;if(args[0]===6)reactions++;break;}}
      assert(success,`Mission ${mission} stalled: ${decode(api.error())}`);
      const after=engine.snapshot(0);assert(after.round>=before.round);
      assert(after.entities.every(e=>e.ap>=0));
    }
    const final=engine.snapshot(0);assert(final.outcome>0,`Mission ${mission} did not finish`);
    doors+=events.filter(e=>/opens door/.test(e.message)).length;
    weapons+=events.filter(e=>e.type===3).length;
    overwatch+=events.filter(e=>/covers facing/.test(e.message)).length;
    console.log(`Autopilot mission ${mission}, seed ${seed}: outcome ${final.outcome}, round ${final.round}, ${ticks} legal steps.`);
  }
}
assert(doors>0,'Autopilot must open doors');assert(weapons>0,'Autopilot must attack');assert(overwatch>0,'Autopilot must set overwatch');assert(reactions>0,'Autopilot must fire reactions');
console.log(`PASS: 18 complete games; doors ${doors}, weapon events ${weapons}, overwatch ${overwatch}, reactions ${reactions}.`);
