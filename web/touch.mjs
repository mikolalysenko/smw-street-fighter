import {keyMap} from './input.mjs';

export function isPhone(nav = navigator) {
  const ua = nav.userAgent || '';
  if (/iPad|Macintosh/i.test(ua)) return false;
  return /iPhone|iPod|Android.*Mobile|Windows Phone/i.test(ua) || nav.userAgentData?.mobile === true;
}

export function dpadButtons(x, y, rect) {
  const dx = (x - rect.left) / rect.width * 2 - 1;
  const dy = (y - rect.top) / rect.height * 2 - 1;
  return (dx < -.28 ? 64 : dx > .28 ? 128 : 0) |
    (dy < -.28 ? 16 : dy > .28 ? 32 : 0);
}

export function installTouchControls(root, onPress) {
  const pointers = new Map();
  const enabled = isPhone();
  root.hidden = !enabled;
  document.documentElement.classList.toggle('phone', enabled);
  const buttons = () => [...pointers.values()].reduce((mask, p) => mask | p.mask, 0);
  const paint = () => {
    const mask = buttons();
    for (const button of root.querySelectorAll('[data-key]')) {
      const pressed = !!(mask & keyMap[button.dataset.key]);
      button.classList.toggle('pressed', pressed);
      button.setAttribute('aria-pressed', String(pressed));
    }
  };
  function update(id, event) {
    const p = pointers.get(id);
    if (!p) return;
    const before = buttons();
    p.mask = p.pad ? dpadButtons(event.clientX, event.clientY, p.pad.getBoundingClientRect()) : p.mask;
    onPress(buttons() & ~before);
    paint();
  }
  root.addEventListener('pointerdown', event => {
    if (!enabled || event.pointerType === 'mouse') return;
    const pad = event.target.closest('[data-dpad]');
    const button = event.target.closest('[data-key]');
    if (!pad && !button) return;
    event.preventDefault();
    const before = buttons();
    const mask = pad ? dpadButtons(event.clientX, event.clientY, pad.getBoundingClientRect()) : keyMap[button.dataset.key];
    pointers.set(event.pointerId, {pad, mask});
    root.setPointerCapture(event.pointerId);
    onPress(buttons() & ~before);paint();
  });
  root.addEventListener('pointermove', event => {
    if (!pointers.has(event.pointerId)) return;
    event.preventDefault();update(event.pointerId, event);
  });
  for (const event of ['pointerup', 'pointercancel', 'lostpointercapture'])
    root.addEventListener(event, e => {pointers.delete(e.pointerId);paint();});
  root.addEventListener('contextmenu', event => event.preventDefault());
  const clear = () => {pointers.clear();paint();};
  window.addEventListener('blur', clear);
  window.addEventListener('resize', clear);
  document.addEventListener('visibilitychange', clear);
  return {buttons, clear, enabled};
}
