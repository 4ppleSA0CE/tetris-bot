import assert from 'node:assert/strict';
import { installDom, makeCanvas, TEST_VARS, fakeSnapshot } from './fake_canvas.mjs';
import { loadBotModule } from '../js/index.js';
import { setPieceCells } from '../js/layout.js';

installDom();
const { mod } = await loadBotModule();
setPieceCells(JSON.parse(mod.getPieceCells()));
const { createRenderer } = await import('../renderers/canvas-mono.js');

const canvas = makeCanvas(360, 720, TEST_VARS);
const r = createRenderer({ canvas, layout: 'demo', chrome: 'full' });
r.resize();

const activeTranslate = () => {
  const i = canvas.ops.map((o) => o.op).lastIndexOf('translate');
  assert.notEqual(i, -1, 'renderer never translated for the active piece');
  return canvas.ops[i].args;
};
const lastRotate = () => {
  const i = canvas.ops.map((o) => o.op).lastIndexOf('rotate');
  assert.notEqual(i, -1, 'renderer never rotated the active piece');
  return canvas.ops[i].args[0];
};

// The smoothing is frame-rate independent (alpha = 1 - exp(-dt/tau)), so the
// test must let real time pass between frames or dt is 0 and nothing moves.
const frame = () => new Promise((res) => setTimeout(res, 17));

// Frame 1 at x=4: the animation snaps to spawn, no slide.
r.draw(fakeSnapshot({ activeX: 4, piecesPlaced: 7 }));
const x0 = activeTranslate()[0];

// Now the core reports x=8 on the same piece. The renderer must ease there over
// several frames, never jump in one.
const xs = [];
for (let i = 0; i < 6; i++) {
  await frame();
  canvas.ops.length = 0;
  r.draw(fakeSnapshot({ activeX: 8, piecesPlaced: 7 }));
  xs.push(activeTranslate()[0]);
}
assert.ok(xs[0] > x0, 'piece did not move at all');
assert.ok(xs[0] < x0 + (xs[5] - x0), 'piece teleported to the target on frame 1');
for (let i = 1; i < xs.length; i++) {
  assert.ok(xs[i] > xs[i - 1], `frame ${i} did not advance (${xs[i - 1]} -> ${xs[i]})`);
}
const step0 = xs[0] - x0;
const step5 = xs[5] - xs[4];
assert.ok(step5 < step0, 'motion did not decelerate — this is a linear jump, not easing');

// A rotation change must produce a non-zero residual angle on the frame it lands.
await frame();
canvas.ops.length = 0;
r.draw(fakeSnapshot({ activeX: 8, activeRot: 1, piecesPlaced: 7 }));
assert.ok(Math.abs(lastRotate()) > 0.05,
  `rotation residual was ${lastRotate()}, expected a visible swing`);

// A new piece SNAPS: no diagonal drift from the old lock site to spawn.
await frame();
canvas.ops.length = 0;
r.draw(fakeSnapshot({ activeX: 4, activeRot: 0, piecesPlaced: 8 }));
const snapX = activeTranslate()[0];
assert.equal(lastRotate(), 0, 'rotation did not snap on a piece change');
await frame();
canvas.ops.length = 0;
r.draw(fakeSnapshot({ activeX: 4, activeRot: 0, piecesPlaced: 8 }));
assert.equal(activeTranslate()[0], snapX, 'piece drifted after a piece change');

// HUD: chrome 'full' writes a stat line in --bot-text-dim and nothing in accent.
await frame();
canvas.ops.length = 0;
r.draw(fakeSnapshot());
const texts = canvas.ops.filter((o) => o.op === 'fillText');
assert.ok(texts.length > 0, "chrome: 'full' drew no HUD text");
const hud = texts.map((o) => o.args[0]).join(' ');
for (const token of ['PPS', 'PIECES', 'LINES', 'ATK', 'B2B']) {
  assert.ok(hud.includes(token), `HUD is missing "${token}": ${hud}`);
}
assert.ok(texts.every((o) => o.fillStyle === TEST_VARS['--bot-text-dim']),
  'HUD text is not --bot-text-dim');

// Hold and queue previews exist and use --bot-cell (never the accent).
const preview = canvas.ops.filter((o) => o.op === 'fillRect' && o.fillStyle === TEST_VARS['--bot-cell']);
assert.ok(preview.length >= 11 + 4 * 6,
  `expected locked cells plus 6 previews, got ${preview.length} --bot-cell fills`);

const accentUse = canvas.ops.filter((o) =>
  o.fillStyle === TEST_VARS['--bot-accent'] || o.strokeStyle === TEST_VARS['--bot-accent']);
assert.equal(accentUse.length, 0, '--bot-accent leaked onto the HUD or previews');

// chrome 'minimal': no HUD, no previews.
const m = makeCanvas(360, 720, TEST_VARS);
const r2 = createRenderer({ canvas: m, layout: 'demo', chrome: 'minimal' });
r2.resize();
r2.draw(fakeSnapshot());
assert.equal(m.ops.filter((o) => o.op === 'fillText').length, 0, "chrome: 'minimal' drew HUD text");

r.destroy();
r2.destroy();
console.log(`renderer anim OK: slide ${xs.map((v) => v.toFixed(1)).join(' -> ')}`);
