// PRD section 6: host-environment behavior belongs in the wrapper, not the core.
import assert from 'node:assert/strict';

// --- prefers-reduced-motion -------------------------------------------------
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

// --- viewport check ---------------------------------------------------------
assert.equal(isViewportBelow(768), false, '1440px reported as below 768px');
globalThis.window.innerWidth = 500;
assert.equal(isViewportBelow(768), true, '500px not reported as below 768px');

console.log(`host env OK: reduced motion froze at ${settled} pieces, viewport check works`);
