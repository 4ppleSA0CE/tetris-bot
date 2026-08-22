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

const ops = canvas.ops;
assert.ok(ops.length > 0, 'renderer drew nothing');

// Background is --bot-bg and covers the whole canvas.
const bg = ops.find((o) => o.op === 'fillRect' && o.fillStyle === TEST_VARS['--bot-bg']);
assert.ok(bg, 'background was not painted with --bot-bg');
assert.deepEqual(bg.args.slice(0, 2), [0, 0]);

// Well outline is --bot-grid, never --bot-accent.
const outline = ops.find((o) => o.op === 'strokeRect' && o.strokeStyle === TEST_VARS['--bot-grid']);
assert.ok(outline, 'well outline was not stroked with --bot-grid');

// 9 + 2 = 11 locked cells in the fake board.
const locked = ops.filter((o) => o.op === 'fillRect' && o.fillStyle === TEST_VARS['--bot-cell']);
assert.equal(locked.length, 11, `expected 11 locked cells, drew ${locked.length}`);

// Cells are drawn bottom-up: board row 0 must be the LOWEST y on screen.
const ys = locked.map((o) => o.args[1]);
assert.ok(Math.max(...ys) > Math.min(...ys), 'all locked cells landed on one row');

// The active piece is --bot-cell-active, and it is 4 cells.
const active = ops.filter((o) => o.op === 'fillRect' && o.fillStyle === TEST_VARS['--bot-cell-active']);
assert.equal(active.length, 4, `active piece drew ${active.length} cells, expected 4`);

// The ghost is --bot-cell-ghost and is stroked, not filled.
const ghost = ops.filter((o) => o.op === 'strokeRect' && o.strokeStyle === TEST_VARS['--bot-cell-ghost']);
assert.equal(ghost.length, 4, `ghost drew ${ghost.length} cells, expected 4`);

// THE DISCIPLINE RULE: no accent anywhere on a frame with no events.
const accentUse = ops.filter((o) =>
  o.fillStyle === TEST_VARS['--bot-accent'] || o.strokeStyle === TEST_VARS['--bot-accent']);
assert.equal(accentUse.length, 0,
  `--bot-accent used on ${accentUse.length} ops outside a callout: ${JSON.stringify(accentUse.slice(0, 3))}`);

// chrome: 'none' draws no text at all.
const bare = makeCanvas(360, 720, TEST_VARS);
const r2 = createRenderer({ canvas: bare, layout: 'sidebar', chrome: 'none' });
r2.resize();
r2.draw(fakeSnapshot());
assert.equal(bare.ops.filter((o) => o.op === 'fillText').length, 0,
  "chrome: 'none' drew text");

r.destroy();
r2.destroy();
console.log(`renderer board OK: ${locked.length} locked, 4 active, 4 ghost, 0 accent ops`);
