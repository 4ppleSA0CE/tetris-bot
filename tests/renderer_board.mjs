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

// The board is painted PER PIECE, not in one flat colour. The fake board is 9 I
// cells on the bottom row and 2 Z cells above them; previews sit outside the well.
const wellRight = outline.args[0] + outline.args[2];
const PIECE_VARS = ['--bot-piece-i', '--bot-piece-j', '--bot-piece-l', '--bot-piece-o',
                    '--bot-piece-s', '--bot-piece-t', '--bot-piece-z'].map((k) => TEST_VARS[k]);
const pieceFills = ops.filter((o) => o.op === 'fillRect' && PIECE_VARS.includes(o.fillStyle));
const insideWell = pieceFills.filter((o) => o.args[0] < wellRight);
const previews = pieceFills.filter((o) => o.args[0] >= wellRight);
const byColour = (c) => insideWell.filter((o) => o.fillStyle === c).length;

assert.equal(byColour(TEST_VARS['--bot-piece-i']), 9, 'bottom row must paint 9 I cells');
assert.equal(byColour(TEST_VARS['--bot-piece-z']), 2, 'second row must paint 2 Z cells');
assert.equal(byColour(TEST_VARS['--bot-piece-t']), 4, 'the active T must paint 4 cells');
assert.equal(insideWell.length, 15, `expected 11 locked + 4 active inside the well, drew ${insideWell.length}`);
// hold (1) + queue (5), 4 cells each, all in their own piece colours.
assert.equal(previews.length, 24,
  `expected 24 preview cells outside the well, drew ${previews.length}`);
// Nothing on the board may still be painted with the flat greyscale cell colour.
assert.equal(ops.filter((o) => o.op === 'fillRect' && o.fillStyle === TEST_VARS['--bot-cell']).length, 0,
  'a cell was painted with the neutral --bot-cell instead of its piece colour');
const locked = insideWell;

// Cells are drawn bottom-up: board row 0 must be the LOWEST y on screen.
const ys = locked.map((o) => o.args[1]);
assert.ok(Math.max(...ys) > Math.min(...ys), 'all locked cells landed on one row');

// The ghost is stroked in the ACTIVE PIECE's colour, not a neutral grey.
const ghost = ops.filter((o) => o.op === 'strokeRect' && o.strokeStyle === TEST_VARS['--bot-piece-t']);
assert.equal(ghost.length, 4, `ghost drew ${ghost.length} cells, expected 4`);
assert.equal(ops.filter((o) => o.op === 'strokeRect' && o.strokeStyle === TEST_VARS['--bot-cell-ghost']).length, 0,
  'ghost fell back to the neutral colour instead of using the piece colour');

// THE DISCIPLINE RULE: no accent anywhere on a frame with no events.
const accentUse = ops.filter((o) =>
  o.fillStyle === TEST_VARS['--bot-accent'] || o.strokeStyle === TEST_VARS['--bot-accent']);
assert.equal(accentUse.length, 0,
  `--bot-accent used on ${accentUse.length} ops outside a callout: ${JSON.stringify(accentUse.slice(0, 3))}`);

// --- the well clips the active piece -----------------------------------------
// The piece SPAWNS at row 21, above the visible field, and roughly a fifth of all
// frames during real play have at least one of its cells up there. Anything drawn
// at those rows lands outside the well's top border, floating in the page.
const activeCellsAt = (y) => {
  const c = makeCanvas(360, 720, TEST_VARS);
  const rr = createRenderer({ canvas: c, layout: 'demo', chrome: 'full' });
  rr.resize();
  rr.draw(fakeSnapshot({ activeY: y, activeRot: 0, piecesPlaced: 7 }));
  rr.destroy();
  return c.ops.filter((o) => o.op === 'fillRect' && o.fillStyle === TEST_VARS['--bot-piece-t']).length;
};
// Expected count comes from the piece table, not from hardcoded geometry.
const T_CELLS = JSON.parse(mod.getPieceCells())[5][0];
const inWell = (y) => {
  let n = 0;
  for (let i = 1; i < T_CELLS.length; i += 2) { const r = y + T_CELLS[i]; if (r >= 0 && r < 20) n++; }
  return n;
};
for (const y of [21, 20, 19, 12, 0]) {
  assert.equal(activeCellsAt(y), inWell(y),
    `active piece at row ${y}: drew ${activeCellsAt(y)} cells, only ${inWell(y)} are inside the well`);
}
assert.equal(activeCellsAt(21), 0, 'a piece parked at the spawn row must paint nothing inside the well');

// chrome: 'none' draws no text at all.
const bare = makeCanvas(360, 720, TEST_VARS);
const r2 = createRenderer({ canvas: bare, layout: 'sidebar', chrome: 'none' });
r2.resize();
r2.draw(fakeSnapshot());
assert.equal(bare.ops.filter((o) => o.op === 'fillText').length, 0,
  "chrome: 'none' drew text");

r.destroy();
r2.destroy();
console.log(`renderer board OK: ${insideWell.length} in-well cells, ${previews.length} preview, 4 ghost, 0 accent`);
