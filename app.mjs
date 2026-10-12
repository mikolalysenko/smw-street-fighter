import createWorldWarrior from './game.mjs?v=20261012-145547';
import {keyMap,gamepadButtons} from './input.mjs?v=20261012-145547';
import {createPresenter} from './presenter.mjs?v=20261012-145547';
import {installTouchControls} from './touch.mjs?v=20261012-145547';
import {createGameAudio} from './audio.mjs?v=20261012-145547';

const $=selector=>document.querySelector(selector);
const status=$('#status'), play=$('#play'), cabinet=$('#cabinet');
const SAVE_KEY='world-warrior-smw-sram-v1';
const STEP=1000*801/48000;
const debug=new URLSearchParams(location.search).get('debug')==='1';
for(const el of document.querySelectorAll('[data-debug]'))el.hidden=!debug;
let game,presenter,started=false,running=false;
let held=new Set(),pending=0,last=0,accumulator=0,frames=0,muted=false;
let savedHighScore=0;
const audio=createGameAudio({onInterruption:pausePlayback,onChange:updateSound});
const touch=installTouchControls($('#touch-controls'),mask=>{pending|=mask;});

function keyboardButtons() { let b=0;for(const code of held)b|=keyMap[code]||0;return b; }
function padButtons() {
  let b=0;
  for(const pad of navigator.getGamepads?.()||[])b|=gamepadButtons(pad);
  return b;
}
function updateSound() {
  const state=audio.diagnostics(),button=$('#mute');
  button.textContent=muted?'Sound off':state.retry?'Enable sound':'Sound on';
  button.setAttribute('aria-pressed',String(!muted && !state.retry));
}
function pausePlayback() {
  running=false;held.clear();pending=0;last=0;accumulator=0;touch.clear();audio.pause();save();
  play.hidden=false;play.textContent=started?'Resume':'Play';
}
function save() {
  if(!game)return;
  const bytes=game.HEAPU8.subarray(game._ww_web_sram(),game._ww_web_sram()+game._ww_web_sram_size());
  try {localStorage.setItem(SAVE_KEY,btoa(String.fromCharCode(...bytes)));}
  catch {$('#save-status').textContent='Browser storage unavailable';$('#help').open=true;}
}
function restore() {
  try {
    const saved=localStorage.getItem(SAVE_KEY);
    if(!saved)return;
    const bytes=Uint8Array.from(atob(saved),c=>c.charCodeAt(0));
    if(bytes.length===game._ww_web_sram_size())game.HEAPU8.set(bytes,game._ww_web_sram());
  } catch {$('#save-status').textContent='Could not restore browser save';$('#help').open=true;}
}
function renderAudio() {
  const pointer=game._ww_web_audio(); // Also advances the original SPC state when muted.
  const samples=game.HEAPF32.subarray(pointer/4,pointer/4+1602);
  audio.push(samples);
}
async function start() {
  if(!game || document.hidden)return;
  $('#teaser-video')?.pause();
  audio.unlock(); // Starts sources/resume synchronously inside this user gesture.
  started=running=true;last=0;accumulator=0;
  play.hidden=true;$('#screen').focus();
}
function failure(error) {
  running=false;audio.pause();status.textContent='Game stopped: '+error.message;
  status.hidden=false;status.dataset.error='true';console.error(error);
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
        running=false;play.hidden=false;play.textContent='Reload game';
        play.onclick=()=>location.reload();save();audio.pause();return;
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
  if(event.target.closest?.('a,button,input,textarea,select,video,summary'))return;
  event.preventDefault();
  if(!running && game) {start().catch(failure);}
  if(!held.has(event.code))pending|=keyMap[event.code];
  held.add(event.code);
});
window.addEventListener('keyup',event=>{
  held.delete(event.code);
  if(keyMap[event.code] && !event.target.closest?.('a,button,input,textarea,select,video,summary'))event.preventDefault();
});
window.addEventListener('blur',()=>{held.clear();pending=0;});
document.addEventListener('visibilitychange',()=>{
  held.clear();pending=0;last=0;accumulator=0;audio.clear();save();
  touch.clear();
  if(document.hidden) {
    pausePlayback();
  }
});
window.addEventListener('pagehide',pausePlayback);
play.onclick=()=>start().catch(failure);
$('#teaser-video')?.addEventListener('play',pausePlayback);
$('#teaser').addEventListener('toggle',()=>{if(!$('#teaser').open)$('#teaser-video').pause();});
function updateExpansion() {
  const expanded=cabinet.classList.contains('expanded')||document.fullscreenElement===cabinet;
  document.body.classList.toggle('game-expanded',cabinet.classList.contains('expanded'));
  $('#touch-expand').textContent=expanded?'EXIT':'EXPAND';
  $('#fullscreen').textContent=expanded?'Exit fullscreen':'Fullscreen';
  $('#fullscreen').setAttribute('aria-pressed',String(expanded));
}
async function expand() {
  try {
    if(document.fullscreenElement)await document.exitFullscreen();
    else if(document.fullscreenEnabled && cabinet.requestFullscreen)await cabinet.requestFullscreen();
    else cabinet.classList.toggle('expanded');
  } catch {cabinet.classList.toggle('expanded');}
  updateExpansion();
  touch.clear();
}
$('#fullscreen').onclick=expand;$('#touch-expand').onclick=expand;
document.addEventListener('fullscreenchange',()=>{
  updateExpansion();
  touch.clear();
});
$('#pause').onclick=()=>{pending|=8;$('#screen').focus();};
$('#mute').onclick=()=>{
  if(audio.diagnostics().retry && !muted){if(running)audio.unlock();else start().catch(failure);}
  else {muted=!muted;audio.setEnabled(!muted);updateSound();}
};
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
    status.textContent=game?kind+' · WASM · 20261012-145547':'Loading game…';
    status.hidden=!!game && !debug;
  });
  game=await createWorldWarrior({
    locateFile:path=>path+'?v=20261012-145547',
    printErr:message=>console.warn(message),
  });
  game._ww_web_set_debug(debug?1:0);
  if(!game._ww_web_init())throw new Error('Engine initialization failed');
  restore();game._ww_web_step(0);savedHighScore=game._ww_web_state(19);
  const pointer=game._ww_web_pixels();presenter.draw(game.HEAPU8.subarray(pointer,pointer+256*224*4));
  play.disabled=false;play.textContent='Play';
  $('#export').disabled=false;
  status.textContent=presenter.kind+' · WASM · 20261012-145547';
  status.hidden=!debug;
  requestAnimationFrame(animate);
} catch(error) {failure(error);}

// Read-only diagnostics for browser checks; save/combat state stays private.
export function diagnostics() {
  return {running,frames,touch:touch.buttons(),phone:touch.enabled,audio:audio.diagnostics().state,
    sound:audio.diagnostics(),
    renderer:presenter?.kind,mode:game?game._ww_web_state(0):-1,
    selecting:game?game._ww_web_state(16):0,debug:game?!!game._ww_web_state(23):false,
    intro:game?game._ww_web_state(5):0,x:game?game._ww_web_state(3):0,
    y:game?game._ww_web_state(4):0,move:game?game._ww_web_state(7):0,
    shots:game?game._ww_web_state(14):0,menu:game?game._ww_web_state(24):0,
    guard:game?!!game._ww_web_state(25):false,crouchGuard:game?!!game._ww_web_state(26):false};
}
