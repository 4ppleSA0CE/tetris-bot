// computeStructSize() is the JS half of the layout check. It must reproduce the
// C++ compiler's padding rule from field records alone.
import assert from 'node:assert/strict';
import { computeStructSize, setPieceCells, getPieceCells } from '../js/layout.js';

const fake = {
  a: { offset: 0, size: 4, count: 1, type: 'u32' },
  b: { offset: 4, size: 2, count: 40, type: 'u16' },
  c: { offset: 152, size: 1, count: 1, type: 'u8' },
  'event.type': { offset: 0, size: 1, count: 1, type: 'u8' },
};
assert.equal(computeStructSize(fake, 4), 156, 'must pad 153 up to the 4-byte alignment');
assert.equal(computeStructSize({ a: { offset: 0, size: 1, count: 1, type: 'u8' } }, 1), 1);
assert.equal(computeStructSize({ a: { offset: 0, size: 1, count: 3, type: 'u8' } }, 4), 4);

assert.throws(() => getPieceCells(0, 0), /createTetrisBot/,
  'reading the piece table before load must throw, not return junk');
setPieceCells([[[0, 0, 1, 0, 2, 0, 3, 0], [], [], []], [], [], [], [], [], []]);
assert.deepEqual(getPieceCells(0, 0), [0, 0, 1, 0, 2, 0, 3, 0]);
assert.deepEqual(getPieceCells(-1, 0), []);
assert.deepEqual(getPieceCells(99, 0), []);
console.log('layout math OK');
