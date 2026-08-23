import { createTetrisBot, prefersReducedMotion } from '../js/index.ts';
import { createRenderer } from '../renderers/canvas-mono.ts';

/**
 * The README GIF seed. Task 22 explains how this number was chosen: it is a
 * seed where a T-spin lands within the first few seconds.
 */
const DEMO_SEED = 42;

const canvas = document.getElementById('well') as HTMLCanvasElement;
const slider = document.getElementById('pps') as HTMLInputElement;
const label = document.getElementById('rate-label') as HTMLSpanElement;

// ?motion=1 animates even under an OS reduce-motion preference, so the demo can be
// watched and screen-recorded without changing a system-wide accessibility setting.
const ignoreReducedMotion = new URLSearchParams(location.search).has('motion');
const bot = await createTetrisBot({
  seed: DEMO_SEED,
  pps: Number(slider.value),
  ignoreReducedMotion,
});
const renderer = createRenderer({ canvas, layout: 'demo', chrome: 'full' });
renderer.resize();

slider.addEventListener('input', () => {
  const pps = Number(slider.value);
  label.textContent = `${pps} PPS`;
  bot.setPPS(pps);
});

window.addEventListener('resize', () => renderer.resize());

if (prefersReducedMotion() && !ignoreReducedMotion) {
  // Say WHY it is holding still, and how to override it. A static board with no
  // explanation is indistinguishable from a hang.
  label.textContent = 'REDUCED MOTION — add ?motion=1 to animate';
  slider.disabled = true;
}

let raf = 0;
const loop = (t: number): void => {
  bot.tick(t);
  renderer.draw(bot.snapshot());
  raf = requestAnimationFrame(loop);
};
raf = requestAnimationFrame(loop);

window.addEventListener('pagehide', () => {
  cancelAnimationFrame(raf);
  renderer.destroy();
  bot.destroy();
});
