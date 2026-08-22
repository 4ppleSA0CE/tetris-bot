// PRD section 5.1 holds long-lived views over a struct pointer, and
// ALLOW_MEMORY_GROWTH detaches them. This forces a real growth and asserts the
// views still read correct data afterwards.
import assert from 'node:assert/strict';
import { createTetrisBot, loadBotModule } from '../js/index.js';

const bot = await createTetrisBot({ seed: 42, pps: 5 });
for (let i = 1; i <= 200; i++) bot.tick(i * 20);      // 4s of simulated time

const before = bot.snapshot();
const frameBefore = before.frame;
const piecesBefore = before.piecesPlaced;
const rowsBefore = Array.from(before.rows);
assert.ok(piecesBefore > 0, 'bot placed no pieces in 200 ticks');
assert.ok(rowsBefore.some((r) => r !== 0), 'board is still completely empty');

const { mod } = await loadBotModule();
const bufBefore = mod.HEAPU8.buffer;

// INITIAL_MEMORY is 32MB, so a 64MB allocation is guaranteed to grow the heap.
const big = mod._malloc(64 * 1024 * 1024);
assert.notEqual(big, 0, 'the forcing malloc failed');
assert.notEqual(bufBefore, mod.HEAPU8.buffer, 'the heap did not actually grow');
assert.equal(bufBefore.byteLength, 0, 'the old ArrayBuffer was not detached');

// snapshot() must re-derive its views here. If it does not, `rows` reads as
// undefined and silently looks like an empty board.
const after = bot.snapshot();
assert.equal(after.frame, frameBefore, 'frame changed across a heap growth');
assert.equal(after.piecesPlaced, piecesBefore, 'piecesPlaced changed across a heap growth');
assert.deepEqual(Array.from(after.rows), rowsBefore, 'board rows corrupted by heap growth');
assert.ok(after.rows.length === 40, `rows view length is ${after.rows.length}, expected 40`);

for (let i = 201; i <= 400; i++) bot.tick(i * 20);
const later = bot.snapshot();
assert.ok(later.piecesPlaced > piecesBefore, 'simulation stopped advancing after growth');
assert.ok(Array.from(later.rows).some((r) => r !== 0), 'board reads empty after growth');

mod._free(big);
bot.destroy();
console.log(`views OK: survived heap growth, pieces ${piecesBefore} -> ${later.piecesPlaced}`);
