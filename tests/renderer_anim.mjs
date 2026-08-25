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

r.draw(fakeSnapshot());
const outline = canvas.ops.find((o) => o.op === 'strokeRect' && o.strokeStyle === TEST_VARS['--bot-grid']);
const wellX = outline.args[0] - 0.5;
const cell = outline.args[2] / 10;

const activeFills = () =>
  canvas.ops.filter((o) => o.op === 'fillRect' && o.fillStyle === TEST_VARS['--bot-piece-t']);
const drawAt = (x) => {
  canvas.ops.length = 0;
  r.draw(fakeSnapshot({ activeX: x, activeY: 12, activeRot: 0, piecesPlaced: 7 }));
  return Math.min(...activeFills().map((o) => o.args[0]));
};

canvas.ops.length = 0;
r.draw(fakeSnapshot());
for (const op of ['translate', 'rotate', 'scale']) {
  assert.equal(canvas.ops.filter((o) => o.op === op).length, 0,
    `renderer emitted a ${op}() - that is an interpolated transform, not discrete movement`);
}

const T_CELLS = JSON.parse(mod.getPieceCells())[5][0];
let minDx = Infinity;
for (let i = 0; i < T_CELLS.length; i += 2) minDx = Math.min(minDx, T_CELLS[i]);

const at4 = drawAt(4);
assert.equal(at4, wellX + (4 + minDx) * cell,
  `piece drawn at x=${at4}, expected exactly ${wellX + (4 + minDx) * cell}`);

const at8 = drawAt(8);
assert.equal(at8 - at4, 4 * cell,
  `moving 4 columns shifted the piece by ${at8 - at4}px, expected exactly ${4 * cell}px in one frame`);

const again = drawAt(8);
assert.equal(again, at8, 'the same snapshot painted the piece in two different places');

canvas.ops.length = 0;
r.draw(fakeSnapshot());
const texts = canvas.ops.filter((o) => o.op === 'fillText');
assert.ok(texts.length > 0, "chrome: 'full' drew no HUD text");
const hud = texts.map((o) => o.args[0]).join(' ');
for (const token of ['PPS', 'PIECES', 'LINES', 'ATK', 'B2B']) {
  assert.ok(hud.includes(token), `HUD is missing "${token}": ${hud}`);
}
assert.ok(texts.every((o) => o.fillStyle === TEST_VARS['--bot-text-dim']), 'HUD text is not --bot-text-dim');

const PIECE_VARS = ['--bot-piece-i', '--bot-piece-j', '--bot-piece-l', '--bot-piece-o',
                    '--bot-piece-s', '--bot-piece-t', '--bot-piece-z'].map((k) => TEST_VARS[k]);
const pieceFills = canvas.ops.filter((o) => o.op === 'fillRect' && PIECE_VARS.includes(o.fillStyle));
assert.ok(pieceFills.length >= 11 + 4 + 24,
  `expected locked + active + 6 previews in piece colours, got ${pieceFills.length}`);
const accentUse = canvas.ops.filter((o) =>
  o.fillStyle === TEST_VARS['--bot-accent'] || o.strokeStyle === TEST_VARS['--bot-accent']);
assert.equal(accentUse.length, 0, '--bot-accent leaked onto the HUD or previews');

const m = makeCanvas(360, 720, TEST_VARS);
const r2 = createRenderer({ canvas: m, layout: 'demo', chrome: 'minimal' });
r2.resize();
r2.draw(fakeSnapshot());
assert.equal(m.ops.filter((o) => o.op === 'fillText').length, 0, "chrome: 'minimal' drew HUD text");

r.destroy();
r2.destroy();
console.log(`renderer anim OK: cell-snapped, 4-column move = ${at8 - at4}px in one frame, 0 transforms`);
