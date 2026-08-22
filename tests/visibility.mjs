// PRD section 6: visibilitychange -> pause ticking on hidden tabs.
import assert from 'node:assert/strict';

const listeners = new Map();
globalThis.document = {
  visibilityState: 'visible',
  addEventListener: (t, fn) => { listeners.set(t, (listeners.get(t) ?? []).concat(fn)); },
  removeEventListener: (t, fn) => {
    listeners.set(t, (listeners.get(t) ?? []).filter((f) => f !== fn));
  },
};
const fire = (t) => (listeners.get(t) ?? []).slice().forEach((fn) => fn());

const { createTetrisBot, loadBotModule } = await import('../js/index.js');
const bot = await createTetrisBot({ seed: 42, pps: 5 });

let t = 0;
const run = (ms) => { for (let i = 0; i < ms / 16.7; i++) { t += 16.7; bot.tick(t); } };

run(4000);
const visiblePieces = bot.snapshot().piecesPlaced;
assert.ok(visiblePieces >= 15, `only ${visiblePieces} pieces in 4 visible seconds at 5 pps`);

globalThis.document.visibilityState = 'hidden';
fire('visibilitychange');
run(4000);
assert.equal(bot.snapshot().piecesPlaced, visiblePieces,
  'simulation advanced while the tab was hidden');

// The tab comes back 10 minutes later; rAF delivers one enormous timestamp.
t += 600000;
globalThis.document.visibilityState = 'visible';
fire('visibilitychange');
bot.tick(t);
const afterReturn = bot.snapshot().piecesPlaced;
assert.ok(afterReturn - visiblePieces <= 2,
  `catch-up burst of ${afterReturn - visiblePieces} pieces on the first visible frame`);

run(4000);
assert.ok(bot.snapshot().piecesPlaced > afterReturn, 'simulation did not resume');

assert.equal((listeners.get('visibilitychange') ?? []).length, 1);
bot.destroy();
assert.equal((listeners.get('visibilitychange') ?? []).length, 0,
  'destroy() leaked the visibilitychange listener');

const { mod } = await loadBotModule();
assert.equal(mod.botLiveCount(), 0);
console.log(`visibility OK: paused while hidden, resumed cleanly, listener released`);
