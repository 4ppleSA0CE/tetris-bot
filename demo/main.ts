import { createTetrisBot, prefersReducedMotion } from '../js/index.ts';
import { createRenderer } from '../renderers/canvas-mono.ts';

// Every load is a new game. ?seed=42 is the README GIF seed (a T-spin lands within the
// first few seconds); any other ?seed=N replays that game.
const params = new URLSearchParams(location.search);
const freshSeed = (): number => Date.now() >>> 0;
const seedParam = Number(params.get('seed'));
const DEMO_SEED = params.has('seed') && Number.isFinite(seedParam) ? seedParam >>> 0 : freshSeed();

const canvas = document.getElementById('well') as HTMLCanvasElement;
const slider = document.getElementById('pps') as HTMLInputElement;
const label = document.getElementById('rate-label') as HTMLSpanElement;

// ?motion=1 animates even under an OS reduce-motion preference, so the demo can be
// watched and screen-recorded without changing a system-wide accessibility setting.
const ignoreReducedMotion = params.has('motion');
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
  const snap = bot.snapshot();
  // A top-out is rare but TERMINAL: the core stops advancing and the page would sit
  // on a dead board forever. The portfolio's attract loop restarts on game over and
  // this does the same, on a fresh seed so it does not replay the game it just lost.
  if (snap.state === 2) bot.reset(freshSeed());
  renderer.draw(snap);
  raf = requestAnimationFrame(loop);
};
raf = requestAnimationFrame(loop);

window.addEventListener('pagehide', () => {
  cancelAnimationFrame(raf);
  renderer.destroy();
  bot.destroy();
});
