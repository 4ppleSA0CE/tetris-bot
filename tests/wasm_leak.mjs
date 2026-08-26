import assert from 'node:assert/strict';
import createBotModule from '../dist/bot.js';

const M = await createBotModule();

const mk = () => M.botCreate(42, 5, 2, 8);

// warm up ticking too: the pc solver's first solve raises dlmalloc's break once
for (let i = 0; i < 50; i++) { const h = mk(); M.botTick(h, 100); M.botDestroy(h); }

const live0 = M.botLiveCount();
const brk0 = M._sbrk(0);
const heap0 = M.HEAPU8.buffer.byteLength;
const probe0 = M._malloc(64);
M._free(probe0);

for (let i = 0; i < 1000; i++) {
  const h = mk();
  M.botTick(h, 100);
  assert.notEqual(M.botSnapshotPtr(h), 0, `null snapshot pointer at iteration ${i}`);
  assert.equal(M.botDestroy(h), true, `destroy returned false at iteration ${i}`);
  assert.equal(M.botDestroy(h), false, `double destroy returned true at iteration ${i}`);
  assert.equal(M.botSnapshotPtr(h), 0, `handle ${h} still resolves after destroy`);
  assert.equal(M.botTick(h, 100), false, `tick on destroyed handle ${h} returned true`);
}

const live1 = M.botLiveCount();
const brk1 = M._sbrk(0);
const heap1 = M.HEAPU8.buffer.byteLength;
const probe1 = M._malloc(64);
M._free(probe1);

console.log(`liveCount    ${live0} -> ${live1}`);
console.log(`sbrk         ${brk0} -> ${brk1}  delta ${brk1 - brk0}`);
console.log(`heap bytes   ${heap0} -> ${heap1}  delta ${heap1 - heap0}`);
console.log(`probe malloc ${probe0} -> ${probe1}  same? ${probe0 === probe1}`);

assert.equal(live1, live0, 'C++ still holds instances after 1000 destroy calls');
assert.equal(brk1 - brk0, 0, 'dlmalloc took new pages across 1000 cycles');
assert.equal(heap1 - heap0, 0, 'wasm heap grew across 1000 cycles');
assert.equal(probe1, probe0, 'free list did not return to its original shape');
console.log('leak OK: 1000 create/destroy cycles, zero drift on all four signals');
