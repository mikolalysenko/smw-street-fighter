import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import createGame from '../dist/web/game.mjs';
import {gamepadButtons,keyMap} from '../web/input.mjs';
const pkg=await readFile(new URL('../dist/web/game.data',import.meta.url));
const wasm=await readFile(new URL('../dist/web/game.wasm',import.meta.url));
const g=await createGame({wasmBinary:wasm,getPreloadedPackage:()=>pkg.buffer.slice(pkg.byteOffset,pkg.byteOffset+pkg.byteLength)});
assert.equal(g._ww_web_init(),1);
const state=i=>g._ww_web_state(i);
let map=0,level=0,selected=false,title=false,selection=false,walking=false,airShot=false,rising=false,superSeen=false,audioEnergy=0,baseX;
for(let tick=1;tick<4200;tick++) {
  const mode=state(0);let input=0;
  title ||= mode===6 || mode===8;
  selection ||= !!state(16);
  if(mode===0x0e) {
    map++;input=map<240?(map%30<20?128:0):map%40===0?1:0;
  } else if(mode===0x14 && state(5))input=tick%45===0?1:0;
  else if(mode===0x14) {
    level++;
    if(!selected) {
      selection ||= !!state(16);
      if(level%30===0){input=8;selected=true;}
    } else {
      baseX ??= state(3);
      if(level>60 && level<72)input=128;
      walking ||= state(3)>baseX+4;
      if(level>=100 && level<117)input=1;
      if(level===108)input|=512;
      airShot ||= !!state(12) && !!state(14);
      if(level>=175 && level<195)input=1;
      if(level===182)input|=512|16;
      rising ||= state(7)===4 && state(13)<-50;
      if(level===265)input=2048;
      superSeen ||= state(7)===8;
      if(level>330)break;
    }
  } else input=tick%45===0?8:tick%45===22?1:0;
  assert.equal(g._ww_web_step(input),1);
  const pointer=g._ww_web_audio();
  const samples=g.HEAPF32.subarray(pointer/4,pointer/4+1602);
  for(const sample of samples){assert(Number.isFinite(sample) && Math.abs(sample)<=1);audioEnergy+=sample*sample;}
}
assert(title && selection && selected && walking && airShot && rising && superSeen,
  JSON.stringify({title,selection,selected,walking,airShot,rising,superSeen,mode:state(0),level}));
assert(audioEnergy>1);assert.equal(g._ww_web_sram_size(),2048);
const px=g.HEAPU8.subarray(g._ww_web_pixels(),g._ww_web_pixels()+256*224*4);
assert(new Set(px).size>32,'Original PPU and fighter pixels rendered');
assert.equal(gamepadButtons({connected:true,mapping:'standard',axes:[1,0],buttons:[{pressed:true}]}),129);
assert.equal(keyMap.KeyA,512);assert.equal(keyMap.KeyC,256);
console.log('WASM: original title, selection/music, real level, walk, air Hadouken, air Shoryuken, super, pixels, audio and save RAM passed.');
