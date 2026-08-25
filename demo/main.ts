import { createTetrisBot, prefersReducedMotion } from '../js/index.ts';
import { createRenderer } from '../renderers/canvas-mono.ts';

const params = new URLSearchParams(location.search);
const freshSeed = (): number => Date.now() >>> 0;
const seedParam = Number(params.get('seed'));
const DEMO_SEED = params.has('seed') && Number.isFinite(seedParam) ? seedParam >>> 0 : freshSeed();

const canvas = document.getElementById('well') as HTMLCanvasElement;
const slider = document.getElementById('pps') as HTMLInputElement;
const label = document.getElementById('rate-label') as HTMLSpanElement;

const ignoreReducedMotion = params.has('motion');

const budgetParam = Number(params.get('budget'));
const searchBudgetMs = Number.isFinite(budgetParam) && budgetParam > 0
  ? Math.min(budgetParam, 50) : undefined;
const bot = await createTetrisBot({
  seed: DEMO_SEED,
  pps: Number(slider.value),
  ignoreReducedMotion,
  searchBudgetMs,
});
const renderer = createRenderer({ canvas, layout: 'demo', chrome: 'full' });

let maxB2bSeen = 0;
(window as unknown as Record<string, unknown>).__snap = () => {
  const s = bot.snapshot();
  return { piecesPlaced: s.piecesPlaced, b2bCount: s.b2bCount, maxB2b: maxB2bSeen,
           attackSent: s.attackSent, state: s.state };
};
renderer.resize();

slider.addEventListener('input', () => {
  const pps = Number(slider.value);
  label.textContent = `${pps} PPS`;
  bot.setPPS(pps);
});

window.addEventListener('resize', () => renderer.resize());

if (prefersReducedMotion() && !ignoreReducedMotion) {

  label.textContent = 'REDUCED MOTION — add ?motion=1 to animate';
  slider.disabled = true;
}

const garbageOn = params.get('garbage') !== '0';
const mix = (v: number): number => {
  v = Math.imul(v ^ (v >>> 16), 0x45d9f3b);
  v = Math.imul(v ^ (v >>> 16), 0x45d9f3b);
  return (v ^ (v >>> 16)) >>> 0;
};
let lastAttackAt = 0;

let raf = 0;
const loop = (t: number): void => {
  bot.tick(t);
  const snap = bot.snapshot();
  if (snap.b2bCount > maxB2bSeen) maxB2bSeen = snap.b2bCount;
  if (garbageOn && snap.piecesPlaced >= lastAttackAt + 12) {
    lastAttackAt = snap.piecesPlaced;
    bot.queueGarbage(1 + (mix(snap.piecesPlaced ^ DEMO_SEED) % 4));
  }

  if (snap.state === 2) { bot.reset(freshSeed()); lastAttackAt = 0; }
  renderer.draw(snap);
  raf = requestAnimationFrame(loop);
};
raf = requestAnimationFrame(loop);

window.addEventListener('pagehide', () => {
  cancelAnimationFrame(raf);
  renderer.destroy();
  bot.destroy();
});
