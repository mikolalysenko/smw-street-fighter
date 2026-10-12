import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import createGame from '../dist/web/game.mjs';
import {gamepadButtons,keyMap} from '../web/input.mjs';
const pkg=await readFile(new URL('../dist/web/game.data',import.meta.url));
const wasm=await readFile(new URL('../dist/web/game.wasm',import.meta.url));
const g=await createGame({wasmBinary:wasm,getPreloadedPackage:()=>pkg.buffer.slice(pkg.byteOffset,pkg.byteOffset+pkg.byteLength)});
assert.equal(g._ww_web_init(),1);
const state=i=>g._ww_web_state(i);
let map=0,level=0,selectTicks=0,selected=false,title=false,selection=false,walking=false,airShot=false,rising=false,superSeen=false,crouchPunch=false,crouchKick=false,audioEnergy=0,baseX;
for(let tick=1;tick<4200;tick++) {
  const mode=state(0);let input=0;
  title ||= mode===6 || mode===8;
  selection ||= !!state(16);
  if(state(16)) {
    assert.equal(mode,0x0b,'Choose a fighter before loading the intro/map');
    if(++selectTicks===6){input=8;selected=true;}
  } else if(mode===0x0e) {
    map++;input=map<240?(map%30<20?128:0):map%40===0?1:0;
  } else if(mode===0x14 && state(5))input=tick%45===0?1:0;
  else if(mode===0x14) {
    level++;
    assert(selected);
    {
      baseX ??= state(3);
      if(level>60 && level<72)input=128;
      walking ||= state(3)>baseX+4;
      if(level>=100 && level<117)input=1;
      if(level===108)input|=512;
      airShot ||= !!state(12) && !!state(14);
      if(level>=175 && level<195)input=1;
      if(level===182)input|=512|16;
      rising ||= state(7)===4 && state(13)<-50;
      if(level>=365 && level<395)input=32;
      if(level===365)input|=2;
      crouchPunch ||= state(7)===1 && !!state(17);
      if(level===265)input=2048;
      superSeen ||= state(7)===8;
      if(level>=415 && level<450)input=32;
      if(level===415)input|=256;
      crouchKick ||= state(7)===2 && !!state(17);
      if(level>460)break;
    }
  } else input=tick%45===0?8:tick%45===22?1:0;
  assert.equal(g._ww_web_step(input),1);
  const pointer=g._ww_web_audio();
  const samples=g.HEAPF32.subarray(pointer/4,pointer/4+1602);
  for(const sample of samples){assert(Number.isFinite(sample) && Math.abs(sample)<=1);audioEnergy+=sample*sample;}
}
assert(title && selection && selected && walking && airShot && rising && superSeen && crouchPunch && crouchKick,
  JSON.stringify({title,selection,selected,walking,airShot,rising,superSeen,crouchPunch,crouchKick,mode:state(0),level}));
assert(audioEnergy>1);assert.equal(g._ww_web_sram_size(),2048);
const px=g.HEAPU8.subarray(g._ww_web_pixels(),g._ww_web_pixels()+256*224*4);
assert(new Set(px).size>32,'Original PPU and fighter pixels rendered');
assert.equal(gamepadButtons({connected:true,mapping:'standard',axes:[1,0],buttons:[{pressed:true}]}),129);
assert.equal(keyMap.KeyA,512);assert.equal(keyMap.KeyC,256);
const highScore=state(19);
assert(highScore>0 && highScore===state(18),'Actual gameplay points set the high score');
const saved=g.HEAPU8.slice(g._ww_web_sram(),g._ww_web_sram()+g._ww_web_sram_size());
const reopened=await createGame({wasmBinary:wasm,getPreloadedPackage:()=>pkg.buffer.slice(pkg.byteOffset,pkg.byteOffset+pkg.byteLength)});
assert.equal(reopened._ww_web_init(),1);
// Browser restoration happens after init, as in app.mjs. No score state may
// be cached before the saved bytes arrive from localStorage.
reopened.HEAPU8.set(saved,reopened._ww_web_sram());
for(let i=0;i<900;i++) {
  assert.equal(reopened._ww_web_step(0),1);
  assert.equal(reopened._ww_web_state(19),highScore,'High score survives a fresh WASM instance and demo scoring');
}
console.log('WASM: title, early selection/music, real level, movement, crouching/air attacks, super, audio and persistent high score passed.');
// Use only player controls to reach every new fighter and the practice shortcut.
const step=b=>assert.equal(g._ww_web_step(b),1);
const tap=b=>{step(b);step(0);};
for(let i=0;i<100;i++)step(0);
for(let fighter=1;fighter<12;fighter++) {
  tap(8);tap(32);tap(8);assert(state(16),'Pause → Player Select');
  tap(128);tap(8);assert.equal(state(20),fighter);
  for(let i=0;i<5;i++)step(0);
}
tap(8);tap(32);tap(32);tap(8);
assert.equal(state(21),1,'Pause → Akuma Battle');
let sawBowser=false,sawAkuma=false;
for(let i=0;i<2200;i++) {
  step(0);
  sawBowser ||= state(22)===0;
  if(state(22)===4){sawAkuma=true;break;}
}
assert(sawBowser && sawAkuma,'Original Bowser entrance and Akuma takeover run in WASM');
assert.equal(state(20),11);assert.equal(state(2),300);
assert.equal(state(19),highScore,'Practice never writes a high score');
console.log('WASM: twelve-player menu, full-meter Akuma practice and original takeover passed.');
