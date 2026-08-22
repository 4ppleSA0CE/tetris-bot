// PRD section 6.1: the target embed, run headlessly. This is the wrapper's
// definition of correct - the exact call sequence a host writes, with a fake
// requestAnimationFrame standing in for the browser.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createTetrisBot, loadBotModule } from '../js/index.js';

// --- 1. the embed is under 20 lines of host code (PRD section 11) ------------
const src = readFileSync(new URL('../demo/embed.ts', import.meta.url), 'utf8');
const lines = src.split('\n')
  .map((l) => l.trim())
  .filter((l) => l.length > 0 && !l.startsWith('//') && !l.startsWith('*') && !l.startsWith('/*'));
console.log(`embed.ts: ${lines.length} lines of host code`);
assert.ok(lines.length <= 20, `embed is ${lines.length} lines, PRD section 11 allows 20`);

// --- 2. the call sequence actually runs -------------------------------------
const queue = [];
globalThis.requestAnimationFrame = (cb) => { queue.push(cb); return queue.length; };
globalThis.cancelAnimationFrame = () => { queue.length = 0; };

const drawn = [];
const renderer = { draw: (s) => drawn.push(s.piecesPlaced) };
const bot = await createTetrisBot({ seed: 42, pps: 5 });

let raf;
let t = 0;
const loop = (ts) => {
  bot.tick(ts);
  renderer.draw(bot.snapshot());
  raf = requestAnimationFrame(loop);
};
raf = requestAnimationFrame(loop);

for (let i = 0; i < 900 && queue.length > 0; i++) {   // 15s at 60fps
  const cb = queue.shift();
  t += 1000 / 60;
  cb(t);
}
cancelAnimationFrame(raf);

const placed = drawn.at(-1);
console.log(`embed ran ${drawn.length} frames, placed ${placed} pieces in ${(t / 1000).toFixed(1)}s`);
assert.equal(drawn.length, 900, 'the rAF loop stopped early');
assert.ok(placed >= 35, `expected >=35 pieces in 15s at 5 pps, got ${placed}`);
assert.ok(drawn[0] <= drawn.at(-1), 'piece count went backwards');

// --- 3. teardown frees the instance -----------------------------------------
bot.destroy();
const { mod } = await loadBotModule();
assert.equal(mod.botLiveCount(), 0, 'destroy() did not free the instance');
assert.throws(() => bot.snapshot(), /after destroy/,
  'snapshot() after destroy() must throw, not read freed memory');

console.log('embed OK: PRD section 6.1 sequence runs and tears down cleanly');
