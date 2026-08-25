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

r.draw(fakeSnapshot());
assert.equal(accentTexts().length, 0, 'accent used with no events pending');

canvas.ops.length = 0;
r.draw(fakeSnapshot({ events: [
  { type: EventType.TSPIN_DOUBLE, param: 2, frame: 1 },
  { type: EventType.B2B_EXTEND, param: 4, frame: 1 },
] }));
let texts = accentTexts().map((o) => o.args[0]);
assert.ok(texts.includes('T-SPIN DOUBLE'), `missing T-SPIN DOUBLE, got ${JSON.stringify(texts)}`);
assert.ok(texts.includes('BACK-TO-BACK ×4'), `missing BACK-TO-BACK x4, got ${JSON.stringify(texts)}`);

const wellRight = canvas.ops.find((o) => o.op === 'strokeRect' &&
  o.strokeStyle === TEST_VARS['--bot-grid']);
const wellX = wellRight.args[0];
const wellW = wellRight.args[2];
for (const t of accentTexts()) {
  const x = t.args[1];
  assert.ok(x <= wellX || x >= wellX + wellW,
    `callout at x=${x} is inside the well [${wellX}, ${wellX + wellW}]`);
}

const firstFont = accentTexts()[0].font;
canvas.ops.length = 0;
await new Promise((res) => setTimeout(res, 160));
r.draw(fakeSnapshot());
const grownFont = accentTexts()[0].font;
const px = (f) => Number(/(\d+)px/.exec(f)[1]);
assert.ok(px(grownFont) > px(firstFont),
  `callout did not scale in: ${firstFont} -> ${grownFont}`);

canvas.ops.length = 0;
await new Promise((res) => setTimeout(res, 900));
r.draw(fakeSnapshot());
assert.equal(accentTexts().length, 0, 'callout outlived its 800ms fade');

canvas.ops.length = 0;
r.draw(fakeSnapshot({ events: [{ type: EventType.TETRIS, param: 4, frame: 9 }] }));
assert.deepEqual(accentTexts().map((o) => o.args[0]), ['TETRIS']);
const nonText = canvas.ops.filter((o) => o.op !== 'fillText' &&
  (o.fillStyle === TEST_VARS['--bot-accent'] || o.strokeStyle === TEST_VARS['--bot-accent']));
assert.equal(nonText.length, 0, `accent used on ${nonText.length} non-text ops`);

const bare = makeCanvas(360, 720, TEST_VARS);
const r2 = createRenderer({ canvas: bare, layout: 'sidebar', chrome: 'none' });
r2.resize();
r2.draw(fakeSnapshot({ events: [{ type: EventType.TETRIS, param: 4, frame: 1 }] }));
assert.equal(bare.ops.filter((o) => o.op === 'fillText').length, 0,
  "chrome: 'none' drew a callout");

r.destroy();
r2.destroy();
console.log('renderer callouts OK: accent appears only in callout text');

const cSpin = makeCanvas(360, 720, TEST_VARS);
const rSpin = createRenderer({ canvas: cSpin, layout: 'demo', chrome: 'full' });
rSpin.resize();
const accentOn = (c) => c.ops.filter(
  (o) => o.op === 'fillText' && o.fillStyle === TEST_VARS['--bot-accent']).map((o) => o.args[0]);
rSpin.draw(fakeSnapshot({ events: [
  { type: EventType.PIECE_LOCK, param: 6, frame: 2 },
  { type: EventType.TSPIN_MINI, param: 2, frame: 2 },
] }));
assert.ok(accentOn(cSpin).includes('Z-SPIN MINI'), `missing Z-SPIN MINI, got ${JSON.stringify(accentOn(cSpin))}`);

cSpin.ops.length = 0;
rSpin.draw(fakeSnapshot({ events: [{ type: EventType.B2B_BREAK, param: 7, frame: 3 }] }));
assert.ok(accentOn(cSpin).includes('SURGE +7'), `missing SURGE +7, got ${JSON.stringify(accentOn(cSpin))}`);
const cQuiet = makeCanvas(360, 720, TEST_VARS);
const rQuiet = createRenderer({ canvas: cQuiet, layout: 'demo', chrome: 'full' });
rQuiet.resize();
rQuiet.draw(fakeSnapshot({ events: [{ type: EventType.B2B_BREAK, param: 0, frame: 4 }] }));
assert.equal(accentOn(cQuiet).length, 0, 'a B2B_BREAK with no Surge must not shout');

console.log('renderer callouts OK: piece-lettered spins and SURGE');
