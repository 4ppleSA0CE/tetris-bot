import assert from 'node:assert/strict';
import { installDom, makeCanvas, TEST_VARS, fakeSnapshot } from './fake_canvas.mjs';
import { loadBotModule } from '../js/index.js';
import { setPieceCells } from '../js/layout.js';
import { EventType } from '../js/types.js';

installDom();
const { mod } = await loadBotModule();
setPieceCells(JSON.parse(mod.getPieceCells()));
const { createRenderer } = await import('../renderers/canvas-mono.js');

const canvas = makeCanvas(360, 720, TEST_VARS);
const r = createRenderer({ canvas, layout: 'demo', chrome: 'full' });
r.resize();

const accentTexts = () => canvas.ops.filter(
  (o) => o.op === 'fillText' && o.fillStyle === TEST_VARS['--bot-accent']);

// No events -> no accent anywhere.
r.draw(fakeSnapshot());
assert.equal(accentTexts().length, 0, 'accent used with no events pending');

// Drain one T-spin double and one B2B extend.
canvas.ops.length = 0;
r.draw(fakeSnapshot({ events: [
  { type: EventType.TSPIN_DOUBLE, param: 2, frame: 1 },
  { type: EventType.B2B_EXTEND, param: 4, frame: 1 },
] }));
let texts = accentTexts().map((o) => o.args[0]);
assert.ok(texts.includes('T-SPIN DOUBLE'), `missing T-SPIN DOUBLE, got ${JSON.stringify(texts)}`);
assert.ok(texts.includes('BACK-TO-BACK ×4'), `missing BACK-TO-BACK x4, got ${JSON.stringify(texts)}`);

// Callouts sit OUTSIDE the well (PRD 7.3: keep them out of the well itself).
const wellRight = canvas.ops.find((o) => o.op === 'strokeRect' &&
  o.strokeStyle === TEST_VARS['--bot-grid']);
const wellX = wellRight.args[0];
const wellW = wellRight.args[2];
for (const t of accentTexts()) {
  const x = t.args[1];
  assert.ok(x <= wellX || x >= wellX + wellW,
    `callout at x=${x} is inside the well [${wellX}, ${wellX + wellW}]`);
}

// The pop grows the type size over the first frames.
const firstFont = accentTexts()[0].font;
canvas.ops.length = 0;
await new Promise((res) => setTimeout(res, 160));
r.draw(fakeSnapshot());
const grownFont = accentTexts()[0].font;
const px = (f) => Number(/(\d+)px/.exec(f)[1]);
assert.ok(px(grownFont) > px(firstFont),
  `callout did not scale in: ${firstFont} -> ${grownFont}`);

// Everything is gone after the 800ms lifetime.
canvas.ops.length = 0;
await new Promise((res) => setTimeout(res, 900));
r.draw(fakeSnapshot());
assert.equal(accentTexts().length, 0, 'callout outlived its 800ms fade');

// TETRIS, and nothing but the callouts is ever accent-coloured.
canvas.ops.length = 0;
r.draw(fakeSnapshot({ events: [{ type: EventType.TETRIS, param: 4, frame: 9 }] }));
assert.deepEqual(accentTexts().map((o) => o.args[0]), ['TETRIS']);
const nonText = canvas.ops.filter((o) => o.op !== 'fillText' &&
  (o.fillStyle === TEST_VARS['--bot-accent'] || o.strokeStyle === TEST_VARS['--bot-accent']));
assert.equal(nonText.length, 0, `accent used on ${nonText.length} non-text ops`);

// chrome: 'none' suppresses callouts entirely.
const bare = makeCanvas(360, 720, TEST_VARS);
const r2 = createRenderer({ canvas: bare, layout: 'sidebar', chrome: 'none' });
r2.resize();
r2.draw(fakeSnapshot({ events: [{ type: EventType.TETRIS, param: 4, frame: 1 }] }));
assert.equal(bare.ops.filter((o) => o.op === 'fillText').length, 0,
  "chrome: 'none' drew a callout");

r.destroy();
r2.destroy();
console.log('renderer callouts OK: accent appears only in callout text');
