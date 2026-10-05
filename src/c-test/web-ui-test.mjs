/* Exercise actual board handlers with a small DOM adapter; no browser required. */
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
import * as tiles from '../../web/tiles.js';
class Element {
  constructor(){this.value='';this.children=[];this.style={};this.hidden=true;this.handlers={};this.clientWidth=800;this.clientHeight=600;this.scrollLeft=0;this.scrollTop=0;this.classList={add(){},remove(){}};}
  append(...nodes){this.children.push(...nodes);}
  replaceChildren(...nodes){this.children=nodes;if(nodes.length)this.value=nodes[0].value;}
  get options(){return this.children;}
  get selectedOptions(){return this.children.filter(o=>o.value===this.value);}
  get firstElementChild(){return {remove(){}};}
  addEventListener(name,fn){this.handlers[name]=fn;}
  getBoundingClientRect(){return {left:0,top:0,width:2048,height:2048};}
  setPointerCapture(){} focus(){} getContext(){return {};}
}
const elements=new Map(),el=id=>{if(!elements.has(id))elements.set(id,new Element());return elements.get(id);};
el('map').parentElement=new Element();el('viewer').value='auto';el('motion').checked=false;
const sandbox={console,...tiles,document:{getElementById:el,createElement:()=>new Element(),addEventListener(){}},matchMedia:()=>({matches:true}),performance:{now:()=>100},setTimeout(){},innerWidth:1000,innerHeight:800,Option:class{constructor(text,value){this.textContent=text;this.value=String(value);}}};
const source=readFileSync(new URL('../../web/game.js',import.meta.url),'utf8').replace(/^import .*;\n/gm,'').split('try {\n  manifest=')[0];
vm.createContext(sandbox);vm.runInContext(source+`\nglobalThis.ui={boardClick,openMenu,zoomTo,focusUnit,select,set(s,e){state=s;engine=e;},getState(){return state;}};`,sandbox);
const marine={id:0,team:0,alive:1,extracted:0,r:10,c:10,ap:4,weapon:0,frag:1,stun:1,ammo:-1,item:-1,done:0,facing:0};
let state={phase:0,active:-1,pending:-1,outcome:0,round:1,deadline:12,objectives:0,evacuated:0,items:[],entities:[marine],map:Array.from({length:25},()=>'.'.repeat(31))};
const calls=[];
const engine={snapshot:()=>state,error:()=>'',command(...args){calls.push(args);if(args[0]===0)state.active=args[1];if(args[0]===1&&args[1]===0){marine.r=args[2];marine.c=args[3];marine.ap--;}if(args[0]===2)state.active=-1;return 0;}};
sandbox.ui.set(state,engine);
sandbox.ui.boardClick({r:10,c:10});assert.deepEqual(calls.pop(),[0,0]);
assert.ok(el('unit-action').options.some(o=>o.value==='now:7'));
el('unit-action').value='now:7';el('do-action').onclick();assert.equal(calls.pop()[1],7);
sandbox.ui.boardClick({r:10,c:11});assert.deepEqual(calls.pop(),[1,0,10,11]);assert.equal(marine.ap,3);
sandbox.ui.openMenu(marine);el('unit-action').value='target:5';el('do-action').onclick();sandbox.ui.boardClick({r:8,c:11});assert.deepEqual(calls.pop(),[1,5,8,11,-1]);
const enemy={...marine,id:5,team:1,r:10,c:12};state.entities.push(enemy);
sandbox.ui.openMenu(marine);el('unit-action').value='target:3';el('do-action').onclick();sandbox.ui.boardClick({r:10,c:12});assert.deepEqual(calls.pop(),[1,3,10,12,5]);
state.pending=5;marine.ow=1;sandbox.ui.openMenu(marine);assert.deepEqual(el('unit-action').options.map(o=>o.value),['reaction:1','reaction:0']);
el('unit-action').value='reaction:1';el('do-action').onclick();assert.deepEqual(calls.pop(),[6,0,1]);el('phase').onclick();assert.deepEqual(calls.pop(),[7]);
state.pending=-1;state.phase=1;state.active=-1;state.map[0]='A'+'.'.repeat(30);sandbox.ui.boardClick({r:0,c:0});assert.deepEqual(calls.pop(),[4,65]);el('phase').onclick();assert.deepEqual(calls.pop(),[5]);
sandbox.ui.zoomTo(80);assert.equal(el('map').style.width,'2560px');sandbox.ui.zoomTo(500);assert.equal(el('map').style.width,'4096px');
state.phase=0;state.active=0;sandbox.ui.select(10,11);el('map').handlers.keydown({key:'ArrowDown',preventDefault(){}});assert.match(el('square').textContent,/12,12/);
// Pointer movement drags an active piece one legal adjacent cell; empty space pans.
el('map').handlers.pointerdown({pointerId:1,clientX:736,clientY:864});el('map').handlers.pointermove({pointerId:1,clientX:800,clientY:864});el('map').handlers.pointerup({pointerId:1,clientX:800,clientY:864});
// Occupied destination opens a target menu rather than moving into another model.
assert.equal(marine.c,11);
el('map').handlers.pointerdown({pointerId:2,clientX:736,clientY:864});el('map').handlers.pointermove({pointerId:2,clientX:736,clientY:928});el('map').handlers.pointerup({pointerId:2,clientX:736,clientY:928});assert.deepEqual(calls.pop(),[1,0,11,11]);
sandbox.ui.openMenu(marine);el('unit-action').value='target:14';el('do-action').onclick();sandbox.ui.boardClick({r:11,c:12});assert.deepEqual(calls.pop(),[1,14,11,12,0]);
state.viewer=0;state.visible=Array.from({length:25},()=> '0'.repeat(31));state.items=[{id:0,holder:-1,dropped:1,name:'recorder',r:11,c:11}];sandbox.ui.openMenu(marine);assert.ok(!el('unit-action').options.some(o=>o.value==='pickup:0'));
state.visible[11]='0'.repeat(11)+'1'+'0'.repeat(19);sandbox.ui.openMenu(marine);assert.ok(el('unit-action').options.some(o=>o.value==='pickup:0'));
console.log('PASS: board activation, movement, targeted grenades/weapons, overwatch reactions, deployment, keyboard cursor and zoom.');
