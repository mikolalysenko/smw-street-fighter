import createWorldWarrior from './game.mjs?v=20261012-080044';
import {keyMap,gamepadButtons} from './input.mjs?v=20261012-080044';
import {createPresenter} from './presenter.mjs?v=20261012-080044';
import {installTouchControls} from './touch.mjs?v=20261012-080044';

const $=selector=>document.querySelector(selector);
const status=$('#status'), play=$('#play'), cabinet=$('#cabinet');
const SAVE_KEY='world-warrior-smw-sram-v1';
const STEP=1000*801/48000;
const debug=new URLSearchParams(location.search).get('debug')==='1';
for(const el of document.querySelectorAll('[data-debug]'))el.hidden=!debug;
let game,presenter,audioContext,audioTime=0,started=false,running=false;
let held=new Set(),pending=0,last=0,accumulator=0,frames=0,muted=false;
let savedHighScore=0;
const sounds=new Set();
const touch=installTouchControls($('#touch-controls'),mask=>{pending|=mask;});

function keyboardButtons() { let b=0;for(const code of held)b|=keyMap[code]||0;return b; }
function padButtons() {
  let b=0;
  for(const pad of navigator.getGamepads?.()||[])b|=gamepadButtons(pad);
  return b;
}
function clearAudio() {
  for(const source of sounds)source.stop();sounds.clear();audioTime=0;
}
function save() {
  if(!game)return;
  const bytes=game.HEAPU8.subarray(game._ww_web_sram(),game._ww_web_sram()+game._ww_web_sram_size());
  try {localStorage.setItem(SAVE_KEY,btoa(String.fromCharCode(...bytes)));}
  catch {$('#save-status').textContent='Browser storage unavailable';}
}
function restore() {
  try {
    const saved=localStorage.getItem(SAVE_KEY);
    if(!saved)return;
    const bytes=Uint8Array.from(atob(saved),c=>c.charCodeAt(0));
    if(bytes.length===game._ww_web_sram_size())game.HEAPU8.set(bytes,game._ww_web_sram());
  } catch {$('#save-status').textContent='Could not restore browser save';}
}
function renderAudio() {
  const pointer=game._ww_web_audio(); // Also advances the original SPC state when muted.
  if(!audioContext || audioContext.state!=='running' || muted)return;
  const buffer=audioContext.createBuffer(2,801,48000);
  const samples=game.HEAPF32.subarray(pointer/4,pointer/4+1602);
  for(let ch=0;ch<2;ch++) {
    const out=buffer.getChannelData(ch);for(let i=0;i<801;i++)out[i]=samples[i*2+ch];
  }
  const now=audioContext.currentTime;
  if(audioTime<now || audioTime>now+0.18) {clearAudio();audioTime=now+0.035;}
  const source=audioContext.createBufferSource();source.buffer=buffer;
  source.connect(audioContext.destination);sounds.add(source);
  source.onended=()=>sounds.delete(source);source.start(audioTime);audioTime+=801/48000;
}
async function start() {
  if(!game)return;
  const Audio=window.AudioContext||window.webkitAudioContext;
  if(!audioContext && Audio) {
    audioContext=new Audio({latencyHint:'interactive'});
    audioContext.addEventListener('statechange',()=>{
      if(audioContext.state==='interrupted' && running) {
        running=false;clearAudio();play.hidden=false;play.textContent='RESUME';
      }
    });
  }
  if(audioContext)await audioContext.resume();
  started=running=true;last=0;accumulator=0;
  play.hidden=true;$('#screen').focus();
}
function failure(error) {
  running=false;clearAudio();status.textContent='Game stopped: '+error.message;
  status.dataset.error='true';console.error(error);
}
function animate(now) {
  requestAnimationFrame(animate);
  if(!running || document.hidden) {last=0;return;}
  if(!last)last=now;
  accumulator+=Math.min(now-last,STEP*5);last=now;
  try {
    while(accumulator>=STEP) {
      const input=keyboardButtons()|padButtons()|touch.buttons()|pending;pending=0;
      if(!game._ww_web_step(input)) {
        running=false;play.hidden=false;play.textContent='RELOAD GAME';
        play.onclick=()=>location.reload();save();clearAudio();return;
      }
      renderAudio();accumulator-=STEP;
      const highScore=game._ww_web_state(19);
      if(++frames%300===0 || highScore!==savedHighScore) {
        save();savedHighScore=highScore;
      }
    }
    const pointer=game._ww_web_pixels();
    presenter.draw(game.HEAPU8.subarray(pointer,pointer+256*224*4));
  } catch(error) {failure(error);}
}
window.addEventListener('keydown',event=>{
  if(event.ctrlKey || event.metaKey || event.altKey || !keyMap[event.code])return;
  if(event.target instanceof HTMLButtonElement && event.code==='Enter')return;
  event.preventDefault();
  if(!running && game) {start().catch(failure);}
  if(!held.has(event.code))pending|=keyMap[event.code];
  held.add(event.code);
});
window.addEventListener('keyup',event=>{
  held.delete(event.code);if(keyMap[event.code])event.preventDefault();
});
window.addEventListener('blur',()=>{held.clear();pending=0;});
document.addEventListener('visibilitychange',()=>{
  held.clear();pending=0;last=0;accumulator=0;clearAudio();save();
  touch.clear();
  if(document.hidden) {
    running=false;audioContext?.suspend().catch(()=>{});
    if(started){play.hidden=false;play.textContent='RESUME';}
  }
});
window.addEventListener('pagehide',save);
play.onclick=()=>start().catch(failure);
async function expand() {
  try {
    if(document.fullscreenElement)await document.exitFullscreen();
    else if(document.fullscreenEnabled && cabinet.requestFullscreen)await cabinet.requestFullscreen();
    else cabinet.classList.toggle('expanded');
  } catch {cabinet.classList.toggle('expanded');}
  document.body.classList.toggle('game-expanded',cabinet.classList.contains('expanded'));
  $('#touch-expand').textContent=cabinet.classList.contains('expanded')||document.fullscreenElement?'EXIT':'EXPAND';
  touch.clear();
}
$('#fullscreen').onclick=expand;$('#touch-expand').onclick=expand;
document.addEventListener('fullscreenchange',()=>{
  $('#touch-expand').textContent=document.fullscreenElement||cabinet.classList.contains('expanded')?'EXIT':'EXPAND';
  touch.clear();
});
$('#pause').onclick=()=>{pending|=8;$('#screen').focus();};
$('#mute').onclick=()=>{muted=!muted;clearAudio();$('#mute').textContent=muted?'SOUND OFF':'SOUND ON';};
$('#export').onclick=()=>{
  save();const pointer=game._ww_web_sram();
  const data=game.HEAPU8.slice(pointer,pointer+game._ww_web_sram_size());
  const url=URL.createObjectURL(new Blob([data],{type:'application/octet-stream'}));
  const link=document.createElement('a');link.href=url;link.download='smw.srm';link.click();
  setTimeout(()=>URL.revokeObjectURL(url),1000);
};

try {
  presenter=await createPresenter($('#screen'),kind=>{
    document.documentElement.dataset.renderer=kind;
    status.textContent=kind+(game?' · WASM · 20261012-080044':' · Loading original game assets…');
  });
  game=await createWorldWarrior({
    locateFile:path=>path+'?v=20261012-080044',
    printErr:message=>console.warn(message),
  });
  game._ww_web_set_debug(debug?1:0);
  if(!game._ww_web_init())throw new Error('Engine initialization failed');
  restore();game._ww_web_step(0);savedHighScore=game._ww_web_state(19);
  const pointer=game._ww_web_pixels();presenter.draw(game.HEAPU8.subarray(pointer,pointer+256*224*4));
  play.disabled=false;play.textContent='PLAY';
  $('#export').disabled=false;
  status.textContent=presenter.kind+' · WASM · 20261012-080044';
  requestAnimationFrame(animate);
} catch(error) {failure(error);}

// Read-only diagnostics for browser checks; save/combat state stays private.
export function diagnostics() {
  return {running,frames,touch:touch.buttons(),phone:touch.enabled,audio:audioContext?.state,
    renderer:presenter?.kind,mode:game?game._ww_web_state(0):-1,
    selecting:game?game._ww_web_state(16):0,debug:game?!!game._ww_web_state(23):false,
    intro:game?game._ww_web_state(5):0,x:game?game._ww_web_state(3):0,
    y:game?game._ww_web_state(4):0,move:game?game._ww_web_state(7):0,
    shots:game?game._ww_web_state(14):0,menu:game?game._ww_web_state(24):0,
    guard:game?!!game._ww_web_state(25):false,crouchGuard:game?!!game._ww_web_state(26):false};
}
