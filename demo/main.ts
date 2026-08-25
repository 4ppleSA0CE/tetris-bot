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
// ?budget=10 raises the per-piece planning budget (ms). The 4.5 default protects the
// frame rate; the chain-holding style needs ~10 ms of wasm search to sustain long b2b.
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
// Live handles for headless probes and console poking; not part of the page UI.
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
  // Say WHY it is holding still, and how to override it. A static board with no
  // explanation is indistinguishable from a hang.
  label.textContent = 'REDUCED MOTION — add ?motion=1 to animate';
  slider.disabled = true;
}

// Deterministic attacker: every 12th locked piece brings 1-4 garbage lines, derived
// from the seed so ?seed=N replays the same siege. ?garbage=0 turns it off.
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
  // A top-out is rare but TERMINAL: the core stops advancing and the page would sit
  // on a dead board forever. The portfolio's attract loop restarts on game over and
  // this does the same, on a fresh seed so it does not replay the game it just lost.
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
