// Optional real-browser checks: npm install --prefix build/browser-tools playwright@1.58.2
import assert from 'node:assert/strict';
import {mkdir} from 'node:fs/promises';
import {chromium,webkit,devices} from '../build/browser-tools/node_modules/playwright/index.mjs';
import {isPhone,dpadButtons} from '../web/touch.mjs';
const url=process.env.WW_TEST_URL||'http://127.0.0.1:8765';
for(const [ua,mobile] of [['iPhone',true],['Android 14 Mobile',true],['iPad',false],['Macintosh',false],['Android 14 Tablet',false]])
  assert.equal(isPhone({userAgent:ua}),mobile);
assert.equal(dpadButtons(0,100,{left:0,top:0,width:100,height:100}),64|32);
await mkdir('build/browser-checks',{recursive:true});
const safari=await webkit.launch({headless:true});
const chrome=await chromium.launch({headless:true});
async function playWithTouch(page,diagnostics,name) {
  const tap=key=>page.locator(`[data-key="${key}"]`).tap();
  for(let i=0;i<90;i++) {
    const s=await diagnostics();if(s.mode===0x0e)break;
    if(s.selecting || (s.mode>=6 && s.mode<=10))await tap('Enter');
    if(s.mode===0x14 && s.intro)await tap('KeyZ');
    await page.waitForTimeout(150);
  }
  assert.equal((await diagnostics()).mode,0x0e,'Touch Start/Jump reaches character select, intro and map');
  for(let i=0;i<8;i++){await tap('ArrowRight');await page.waitForTimeout(400);}
  for(let i=0;i<30;i++) {
    const s=await diagnostics();if(s.mode===0x14 && !s.intro)break;
    if(s.mode===0x0e)await tap('KeyZ');await page.waitForTimeout(150);
  }
  assert.equal((await diagnostics()).mode,0x14,'Touch directions and Jump enter a level');
  await page.waitForTimeout(800);
  for(const [key,move] of [['KeyX',1],['KeyC',2],['KeyA',3],['KeyS',8]]) {
    await tap(key);
    let seen=false;
    for(let i=0;i<12;i++){if((await diagnostics()).move===move){seen=true;break;}await page.waitForTimeout(15);}
    assert(seen,`Touch ${key} triggers move ${move}`);
    await page.waitForTimeout(key==='KeyS'?2200:700);
  }
  const floor=(await diagnostics()).y;await tap('KeyZ');await page.waitForTimeout(130);
  assert((await diagnostics()).y<floor,'Touch jump moves the fighter upward');
  await tap('KeyA');await page.waitForTimeout(90);
  assert.equal((await diagnostics()).move,3,'Touch special works in midair');
  await page.screenshot({path:`build/browser-checks/${name}-gameplay.png`});
  await page.waitForTimeout(1000);
  await page.evaluate(()=>{
    const root=document.querySelector('#touch-controls'),pad=root.querySelector('[data-dpad]');
    root.setPointerCapture=()=>{};const r=pad.getBoundingClientRect();
    window.testTouch('[data-dpad]','pointerdown',4,r.x+r.width/2,r.y+r.height-2);
    window.testTouch('[data-key=KeyD]','pointerdown',5);
  });
  await page.waitForTimeout(150);
  assert((await diagnostics()).guard && (await diagnostics()).crouchGuard,'Touch Down + Guard crouch-blocks in the actual game');
  await page.evaluate(()=>{
    window.testTouch('#touch-controls','pointerup',4);window.testTouch('#touch-controls','pointerup',5);
    delete document.querySelector('#touch-controls').setPointerCapture;
  });
  await tap('Enter');await page.waitForTimeout(150);
  assert.equal((await diagnostics()).menu,2,'Touch Start pauses');
  await page.screenshot({path:`build/browser-checks/${name}-pause.png`});
  await tap('Escape');await page.waitForTimeout(150);
  assert.equal((await diagnostics()).menu,0,'Touch Back resumes');
  console.log(name,'touch-only start, selection, map, movement, punch, kick, fireball, super, air attack and pause passed.');
}
async function inspect(name,browser,options,phone) {
  const context=await browser.newContext(options),page=await context.newPage(),errors=[];
  page.on('pageerror',e=>errors.push(String(e)));
  await page.goto(url);await page.locator('#play').waitFor();
  await page.waitForFunction(()=>!document.querySelector('#play').disabled);
  const diagnostics=()=>page.evaluate(async()=>{
    const app=await import(document.querySelector('script[type=module]').src);return app.diagnostics();
  });
  assert.equal(await page.locator('.touch-actions').isVisible(),phone);
  assert.equal((await diagnostics()).phone,phone);
  assert.equal((await diagnostics()).debug,false);
  assert.equal(await page.locator('[data-debug]').isVisible(),false);
  await page.locator('#play').click();
  await page.waitForTimeout(1600);
  const state=await diagnostics();assert(state.running && state.frames>65 && state.audio==='running',JSON.stringify(state));
  if(phone) {
    const canvas=await page.locator('#screen').boundingBox(),pad=await page.locator('[data-dpad]').boundingBox();
    assert(canvas.width>180 && canvas.height>150 && pad.width>=140);
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
    for(const b of await page.locator('.touch-actions button,.touch-menu button').all()) {
      const box=await b.boundingBox();assert(box.width>=44 && box.height>=44);
      assert(box.x>=0 && box.x+box.width<=options.viewport.width+1);
    }
    // Isolated touch event plumbing: two attack fingers plus a sliding pad.
    // Pointer capture is supplied by physical touch; synthetic events lack it.
    await page.evaluate(()=>{
      const root=document.querySelector('#touch-controls');root.setPointerCapture=()=>{};
      window.testTouch=(selector,type,id,x=0,y=0)=>document.querySelector(selector).dispatchEvent(
        new PointerEvent(type,{bubbles:true,cancelable:true,pointerType:'touch',pointerId:id,clientX:x,clientY:y}));
    });
    const event=(selector,type,id,x,y)=>page.evaluate(args=>window.testTouch(...args),[selector,type,id,x,y]);
    await event('[data-dpad]','pointerdown',1,pad.x+pad.width-2,pad.y+pad.height-2);
    await event('.touch-jump','pointerdown',2);await event('[data-key=KeyA]','pointerdown',3);
    assert.equal((await diagnostics()).touch,128|32|1|512);
    await event('#touch-controls','pointermove',1,pad.x+2,pad.y+2);
    assert.equal((await diagnostics()).touch,64|16|1|512);
    await event('#touch-controls','pointercancel',2);
    assert.equal((await diagnostics()).touch,64|16|512);
    await page.evaluate(()=>window.dispatchEvent(new Event('blur')));
    assert.equal((await diagnostics()).touch,0);
    // iPhone's in-page expansion is usable without the fullscreen API.
    await page.evaluate(()=>Object.defineProperty(document,'fullscreenEnabled',{value:false,configurable:true}));
    await page.locator('#touch-expand').click();
    assert(await page.locator('#cabinet').evaluate(el=>el.classList.contains('expanded')));
    await page.screenshot({path:`build/browser-checks/${name}-expanded.png`});
    await page.locator('#touch-expand').click();
    // Safari suspends background audio: require a gesture to resume once.
    await page.evaluate(()=>{Object.defineProperty(document,'hidden',{value:true,configurable:true});document.dispatchEvent(new Event('visibilitychange'));});
    await page.waitForTimeout(100);assert.equal((await diagnostics()).running,false);
    await page.evaluate(()=>{Object.defineProperty(document,'hidden',{value:false,configurable:true});document.dispatchEvent(new Event('visibilitychange'));});
    await page.locator('#play').click();await page.waitForTimeout(200);
    assert.equal((await diagnostics()).audio,'running');
    await page.evaluate(()=>delete document.querySelector('#touch-controls').setPointerCapture);
    if(name==='iphone-portrait' || name==='android')await playWithTouch(page,diagnostics,name);
  }
  await page.screenshot({path:`build/browser-checks/${name}.png`,fullPage:true});
  assert.deepEqual(errors,[]);console.log(name,JSON.stringify(await diagnostics()));
  await context.close();
}
try {
  await inspect('iphone-portrait',safari,{...devices['iPhone 13']},true);
  await inspect('iphone-small',safari,{...devices['iPhone SE'],viewport:{width:320,height:568}},true);
  await inspect('iphone-se-landscape',safari,{...devices['iPhone SE'],viewport:{width:667,height:375}},true);
  await inspect('ipad',safari,{...devices['iPad (gen 7)']},false);
  await inspect('desktop',chrome,{viewport:{width:1280,height:900}},false);
  await inspect('android',chrome,{...devices['Pixel 5']},true);
  const context=await chrome.newContext(),page=await context.newPage();
  for(const value of ['1','0','true']) {
    await page.goto(url+'?debug='+value);
    await page.waitForFunction(()=>!document.querySelector('#play').disabled);
    const debug=await page.evaluate(async()=>(await import(document.querySelector('script[type=module]').src)).diagnostics().debug);
    assert.equal(debug,value==='1');assert.equal(await page.locator('[data-debug]').isVisible(),value==='1');
  }
  await context.close();
} finally {await safari.close();await chrome.close();}
console.log('Browser: WASM/audio boot, phone-only controls, multi-touch, safe layouts, expand and resume passed.');
