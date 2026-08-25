import assert from 'node:assert/strict';

globalThis.window = {
  innerWidth: 1440,
  matchMedia: (q) => ({ matches: q.includes('prefers-reduced-motion') }),
};
const { createTetrisBot, prefersReducedMotion, isViewportBelow } =
  await import('../js/index.js');

assert.equal(prefersReducedMotion(), true, 'reduced motion not detected');
const frozen = await createTetrisBot({ seed: 42, pps: 5 });
const settled = frozen.snapshot().piecesPlaced;
assert.ok(settled >= 20, `reduced-motion board settled at only ${settled} pieces`);
assert.ok(frozen.snapshot().rows.some((r) => r !== 0), 'static board is empty');
for (let i = 1; i <= 600; i++) frozen.tick(i * 16.7);
assert.equal(frozen.snapshot().piecesPlaced, settled,
  'tick() advanced the simulation under prefers-reduced-motion');
frozen.destroy();

const moving = await createTetrisBot({ seed: 42, pps: 5, ignoreReducedMotion: true });
assert.equal(moving.snapshot().piecesPlaced, 0,
  'ignoreReducedMotion must skip the settle-a-static-board step, not just unfreeze after it');
let mt = 0;
for (let i = 1; i <= 600; i++) { mt += 16.7; moving.tick(mt); }
const moved = moving.snapshot().piecesPlaced;
assert.ok(moved >= 40, `override ticked only ${moved} pieces in 10s at 5 pps`);
moving.destroy();

assert.equal(prefersReducedMotion(), true, 'the OS preference must still report reduce');

assert.equal(isViewportBelow(768), false, '1440px reported as below 768px');
globalThis.window.innerWidth = 500;
assert.equal(isViewportBelow(768), true, '500px not reported as below 768px');

console.log(`host env OK: reduced motion froze at ${settled} pieces, override ran ${moved}, viewport check works`);
