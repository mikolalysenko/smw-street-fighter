import assert from 'node:assert/strict';
import {createGameAudio} from '../web/audio.mjs';

function fixture({legacy=false,resume='ok'}={}) {
  const events=[],contexts=[],timers=new Map();let timerId=0,interruptions=0;
  const env={navigator:{userAgent:'iPhone'},btoa:s=>Buffer.from(s,'binary').toString('base64'),
    setTimeout:fn=>{timers.set(++timerId,fn);return timerId;},clearTimeout:id=>timers.delete(id)};
  if(!legacy)env.navigator.audioSession={set type(v){events.push('category:'+v);}};
  env.Audio=class {
    constructor(src){assert(src.startsWith('data:audio/wav;base64,'));this.paused=true;}
    setAttribute(){}
    play(){this.paused=false;events.push('media:play');return Promise.resolve();}
    pause(){this.paused=true;events.push('media:pause');}
  };
  env.AudioContext=class {
    constructor(){this.state='suspended';this.sampleRate=44100;this.currentTime=1;this.destination={};contexts.push(this);}
    addEventListener(name,fn){this.changed=fn;}
    resume(){events.push('resume');if(resume==='reject')return Promise.reject(new Error('blocked'));
      if(resume==='hang')return new Promise(()=>{});this.state='running';return Promise.resolve();}
    suspend(){this.state='suspended';this.changed();events.push('suspend');return Promise.resolve();}
    close(){this.state='closed';this.changed();return Promise.resolve();}
    createBuffer(channels,frames,rate){return {channels,frames,rate,getChannelData:()=>new Float32Array(frames)};}
    createBufferSource(){return {connect(){},disconnect(){},stop(){events.push('stop');},start(t){events.push(['start',t,this.buffer]);}};}
  };
  let player;player=createGameAudio({onInterruption:()=>{interruptions++;player.pause();}},env);
  return {player,events,contexts,timers,get interruptions(){return interruptions;},setResume:v=>{resume=v;}};
}

{
  const f=fixture(),ready=f.player.unlock();
  // All category/resume/source calls have happened before returning to the
  // event loop; the game must not wait for RAF to unlock iPhone audio.
  assert.equal(f.events[0],'category:playback');assert.equal(f.events[1],'resume');
  assert.equal(f.events[2][0],'start');assert.equal(f.events[2][2].frames,1);
  assert(await ready);
  const samples=new Float32Array(1602);samples[0]=0.5;samples[1]=-0.25;
  f.player.push(samples);assert.equal(f.player.diagnostics().buffers,1);
  assert.equal(f.player.diagnostics().peak,0.5);
  const output=f.events.at(-1);assert.equal(output[2].rate,48000);assert.equal(output[2].frames,801);
  f.player.pause();assert.equal(f.interruptions,0);assert(f.events.includes('category:auto'));
  f.player.push(samples);assert.equal(f.player.diagnostics().buffers,1);
  await f.player.unlock();assert.equal(f.contexts.length,1,'Normal background resume reuses a healthy context');
  await f.player.setEnabled(false);f.player.push(samples);assert.equal(f.player.diagnostics().buffers,1);
  await f.player.setEnabled(true);f.player.push(samples);assert.equal(f.player.diagnostics().buffers,2);
  f.contexts[0].state='interrupted';f.contexts[0].changed();assert.equal(f.interruptions,1);
  await f.player.unlock();assert.equal(f.contexts.length,2,'Interrupted iOS context is replaced on a gesture');
}
{
  const f=fixture({legacy:true});await f.player.unlock();
  assert.equal(f.events[0],'media:play');assert.equal(f.player.diagnostics().session,'media');
  f.player.pause();assert(f.events.includes('media:pause'));await f.player.unlock();
  assert.equal(f.player.diagnostics().session,'media');
  await f.player.setEnabled(false);assert.equal(f.player.diagnostics().session,'auto');
}
for(const resume of ['reject','hang']) {
  const f=fixture({resume}),ready=f.player.unlock();
  if(resume==='hang')for(const fn of f.timers.values())fn();
  assert.equal(await ready,false);assert(f.player.diagnostics().retry);
  f.setResume('ok');assert(await f.player.unlock());assert.equal(f.contexts.length,2);
}
{
  const f=fixture(),ready=f.player.unlock();f.player.pause();
  assert.equal(await ready,false,'An old resume cannot revive sound after backgrounding');
  assert.equal(f.player.diagnostics().session,'auto');
  await f.player.setEnabled(false);await f.player.unlock();
  assert.equal(f.player.diagnostics().enabled,false,'Play respects the mute choice');
}
console.log('Web audio: synchronous iOS playback unlock, legacy fallback, mute, interruption and failed/hung resume passed.');
