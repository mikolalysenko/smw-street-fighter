// Keep gesture-sensitive calls synchronous: Safari can discard activation
// between a tap and the first animation frame (or an awaited promise).
export function createGameAudio({onInterruption=()=>{},onChange=()=>{}}={},env=globalThis) {
  const nav=env.navigator||{};
  let context,keeper,session=false,wanted=false,enabled=true,retry=false;
  let generation=0,time=0,peak=0,buffers=0,unhealthy=false;
  const sources=new Set();
  const ios=/iPad|iPhone|iPod/.test(nav.userAgent||'') ||
    (nav.platform==='MacIntel' && nav.maxTouchPoints>1);

  function category(type) {
    try {
      if(nav.audioSession){nav.audioSession.type=type;return true;}
    } catch {} // Older Safari has no Audio Session API.
    return false;
  }
  function clear() {
    for(const source of sources) {try{source.stop();}catch{}source.disconnect();}
    sources.clear();time=0;
  }
  function release() {
    clear();keeper?.pause();
    if(session){category('auto');session=false;}
    if(context && context.state!=='closed')context.suspend().catch(()=>{});
  }
  function pause() {
    wanted=false;++generation;release();onChange();
  }
  function silentMedia() {
    // iOS before AudioSession needs a playing media element to choose the
    // playback category. An unmuted, silent WAV is intentional: a muted
    // element does not switch the category. Never run it in the background.
    const wav=new Uint8Array(44+8000/4),view=new DataView(wav.buffer);
    const text=(at,s)=>{for(let i=0;i<s.length;i++)wav[at+i]=s.charCodeAt(i);};
    text(0,'RIFF');view.setUint32(4,wav.length-8,true);text(8,'WAVEfmt ');
    view.setUint32(16,16,true);view.setUint16(20,1,true);view.setUint16(22,1,true);
    view.setUint32(24,8000,true);view.setUint32(28,8000,true);
    view.setUint16(32,1,true);view.setUint16(34,8,true);text(36,'data');
    view.setUint32(40,wav.length-44,true);wav.fill(128,44);
    const element=new env.Audio('data:audio/wav;base64,'+env.btoa(String.fromCharCode(...wav)));
    element.loop=true;element.preload='auto';element.setAttribute('playsinline','');
    return element;
  }
  function unlock() {
    wanted=true;
    if(!enabled){onChange();return Promise.resolve(false);}
    const token=++generation;
    const Audio=env.AudioContext||env.webkitAudioContext;
    if(!Audio){retry=true;onChange();return Promise.resolve(false);}
    try {
      session=category('playback');
      if(!session && ios && env.Audio) {
        keeper??=silentMedia();
        keeper.play()?.catch(()=>{});
      }
      if(!context || context.state==='closed' || context.state==='interrupted' || unhealthy) {
        clear();const old=context;
        context=new Audio({latencyHint:'interactive'});unhealthy=false;
        if(old && old.state!=='closed')old.close().catch(()=>{});
        const current=context;
        current.addEventListener('statechange',()=>{
          if(current!==context || !wanted || !enabled)return;
          if(current.state==='interrupted' || current.state==='closed' ||
              (current.state==='suspended' && !retry)) {
            unhealthy=current.state==='interrupted' || current.state==='closed';
            retry=true;onChange();onInterruption();
          }
        });
      }
      retry=true;
      const current=context;
      const resume=current.resume();
      const primer=current.createBufferSource();
      primer.buffer=current.createBuffer(1,1,current.sampleRate);
      primer.connect(current.destination);primer.onended=()=>primer.disconnect();primer.start(0);
      // A broken iOS context can leave resume() pending forever. Gameplay
      // remains usable; the Sound button offers a new gesture/context.
      let timer;
      const deadline=new Promise(resolve=>{timer=env.setTimeout(()=>resolve(false),1500);});
      const ready=Promise.resolve(resume).then(()=>current.state==='running',()=>false);
      return Promise.race([ready,deadline]).then(ok=>{
        env.clearTimeout(timer);
        if(token!==generation)return false;
        retry=!ok;unhealthy=!ok;onChange();return ok;
      });
    } catch {
      retry=true;unhealthy=true;onChange();return Promise.resolve(false);
    }
  }
  function setEnabled(value) {
    enabled=value;++generation;
    if(!enabled)release();
    else if(wanted)return unlock();
    onChange();return Promise.resolve(false);
  }
  function push(samples) {
    if(!wanted || !enabled || !context || context.state!=='running' || retry)return;
    const frames=samples.length/2,buffer=context.createBuffer(2,frames,48000);
    for(let ch=0;ch<2;ch++) {
      const out=buffer.getChannelData(ch);
      for(let i=0;i<frames;i++){out[i]=samples[i*2+ch];peak=Math.max(peak,Math.abs(out[i]));}
    }
    const now=context.currentTime;
    if(time<now || time>now+0.18){clear();time=now+0.035;}
    const source=context.createBufferSource();source.buffer=buffer;
    source.connect(context.destination);sources.add(source);
    source.onended=()=>{sources.delete(source);source.disconnect();};
    source.start(time);time+=frames/48000;++buffers;
  }
  function diagnostics() {
    return {state:context?.state,enabled,retry,session:session?'playback':keeper&&!keeper.paused?'media':'auto',
      buffers,peak,queued:sources.size};
  }
  return {unlock,pause,clear,setEnabled,push,diagnostics};
}
