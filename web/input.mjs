export const keyMap = {
  ArrowUp: 1 << 4, ArrowDown: 1 << 5, ArrowLeft: 1 << 6, ArrowRight: 1 << 7,
  KeyZ: 1, Space: 1, KeyX: 2, KeyC: 1 << 8, KeyA: 1 << 9,
  KeyS: 1 << 11, KeyD: 1 << 10, Enter: 8, Escape: 4,
};

export function gamepadButtons(pad) {
  if (!pad?.connected || pad.mapping !== 'standard') return 0;
  const down = i => !!pad.buttons[i]?.pressed;
  const x = pad.axes[0] || 0, y = pad.axes[1] || 0;
  let b = 0;
  for (const [i, bit] of [[0,0],[2,1],[8,2],[9,3],[12,4],[13,5],[14,6],[15,7],[1,8],[3,9],[4,10],[5,11]])
    if (down(i)) b |= 1 << bit;
  if (x < -0.25) b |= 1 << 6;
  if (x > 0.25) b |= 1 << 7;
  if (y < -0.25) b |= 1 << 4;
  if (y > 0.25) b |= 1 << 5;
  return b;
}
