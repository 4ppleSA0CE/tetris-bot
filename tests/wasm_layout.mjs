// Layout export contract check. PRD section 12 names struct-layout mismatch as a
// silent-corruption risk; this is the assertion that catches it.
import assert from 'node:assert/strict';
import createBotModule from '../dist/bot.js';

const M = await createBotModule();

const layout = JSON.parse(M.getSnapshotLayout());
const align = M.getSnapshotAlign();
const declared = M.getSnapshotSize();

const EXPECTED_FIELDS = [
  'frame', 'rows', 'activePiece', 'activeRot', 'activeX', 'activeY', 'ghostY',
  'pendingSpin', 'pathProgress', 'holdPiece', 'queue', 'eventCount', 'events',
  'piecesPlaced', 'linesCleared', 'attackSent', 'b2bCount', 'comboCount', 'pps', 'state',
  'cellPiece', 'pendingGarbage',
  'event.type', 'event.param', 'event.frame',
];
for (const f of EXPECTED_FIELDS) {
  assert.ok(layout[f], `getSnapshotLayout() is missing field "${f}"`);
  assert.equal(typeof layout[f].offset, 'number', `${f}.offset is not a number`);
  assert.equal(typeof layout[f].size, 'number', `${f}.size is not a number`);
  assert.equal(typeof layout[f].count, 'number', `${f}.count is not a number`);
  assert.equal(typeof layout[f].type, 'string', `${f}.type is not a string`);
}
assert.equal(Object.keys(layout).length, EXPECTED_FIELDS.length,
  `layout has ${Object.keys(layout).length} entries, expected ${EXPECTED_FIELDS.length}`);

// Fields named "event.*" are offsets WITHIN one Event, not within Snapshot.
let max = 0;
for (const [name, f] of Object.entries(layout)) {
  if (name.startsWith('event.')) continue;
  max = Math.max(max, f.offset + f.size * f.count);
}
const computed = Math.ceil(max / align) * align;
assert.equal(computed, declared,
  `JS-computed sizeof is ${computed} but C++ sizeof(Snapshot) is ${declared}`);

assert.equal(layout.rows.count, 40);
assert.equal(layout.rows.size, 2);
assert.equal(layout.queue.count, 5);
assert.equal(layout.events.count, 8);
assert.equal(layout.events.size, 4);
assert.equal(layout.cellPiece.count, 400);
assert.equal(layout.cellPiece.size, 1);
assert.equal(layout.pendingGarbage.size, 1);
assert.ok(layout.pendingGarbage.offset > layout.cellPiece.offset);
assert.equal(layout.pendingSpin.offset + 1, layout.pathProgress.offset,
  'pendingSpin and pathProgress must be adjacent, in that order');

const weights = JSON.parse(M.getWeightsInfo());
assert.equal(weights.length, 21, `expected 21 weights, got ${weights.length}`);
weights.forEach((w, i) => {
  assert.equal(w.index, i, `weight ${w.name} has index ${w.index}, expected ${i}`);
  assert.equal(typeof w.default, 'number');
});
assert.deepEqual(weights.map((w) => w.name), [
  'holes', 'coveredCells', 'bumpiness', 'maxHeight', 'heightPenalty',
  'rowTransitions', 'columnTransitions', 'wellDepth', 'tSlotCount', 'tslot1', 'tslot2',
  'b2bActive', 'b2bLevel', 'attackDealt', 'b2bCharge', 'rowsWithHoles', 'overhangs',
  'plainClear', 'b2bBreak', 'wastedT', 'incomingRisk',
]);

const cells = JSON.parse(M.getPieceCells());
assert.equal(cells.length, 7, 'expected 7 pieces');
for (let p = 0; p < 7; p++) {
  assert.equal(cells[p].length, 4, `piece ${p} must have 4 rotations`);
  for (let r = 0; r < 4; r++) {
    assert.equal(cells[p][r].length, 8, `piece ${p} rot ${r} must be 4 (dx,dy) pairs`);
  }
}

console.log(`layout OK: ${Object.keys(layout).length} fields, sizeof=${declared}, alignof=${align}`);
