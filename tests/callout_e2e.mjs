// A real C++ event, drained through the real SnapshotView, painted by the real
// renderer. PRD section 5.1: this is the one path callouts cross on.
import assert from 'node:assert/strict';
import { installDom, makeCanvas, TEST_VARS } from './fake_canvas.mjs';
import { createTetrisBot, loadBotModule } from '../js/index.js';
import { setPieceCells } from '../js/layout.js';
import { EventType, PieceLetter } from '../js/types.js';

installDom();
const { mod } = await loadBotModule();
setPieceCells(JSON.parse(mod.getPieceCells()));
const { createRenderer } = await import('../renderers/canvas-mono.js');

// Spin callouts are prefixed with the letter of the piece that locked (PIECE_LOCK param).
const EXPECTED = {
  [EventType.TETRIS]: 'TETRIS',
  [EventType.TSPIN_MINI]: '-SPIN MINI',
  [EventType.TSPIN_SINGLE]: '-SPIN SINGLE',
  [EventType.TSPIN_DOUBLE]: '-SPIN DOUBLE',
  [EventType.TSPIN_TRIPLE]: '-SPIN TRIPLE',
};

const canvas = makeCanvas(360, 720, TEST_VARS);
const r = createRenderer({ canvas, layout: 'demo', chrome: 'full' });
r.resize();

const bot = await createTetrisBot({ seed: 42, pps: 20 });
const FRAME_MS = 1000 / 60;

let t = 0;
let shouted = null;
let shoutedDrawn = null;
let sawLock = false;
let lockPiece = -1;
let b2bChecks = 0;

// Run PAST the first shout. B2B_EXTEND only fires on the SECOND difficult clear,
// and the first difficult clear is itself a tetris or t-spin - so a loop that
// stops at the first shout can never reach a single B2B_EXTEND, and the param
// assertion below would be dead code that always reports 0 checks.
const WANT_B2B_CHECKS = 5;

for (let f = 0; f < 36000; f++) {
  t += FRAME_MS;
  bot.tick(t);
  const s = bot.snapshot();

  // The ring is 8 deep and eventCount must never claim more than that.
  assert.ok(s.events.length <= 8, `eventCount ${s.events.length} exceeds the 8-slot ring`);

  for (const ev of s.events) {
    assert.ok(ev.type >= 0 && ev.type <= 10, `event type ${ev.type} is out of range`);
    if (ev.type === EventType.PIECE_LOCK) {
      // param is the PieceType that locked, NOT the line count.
      assert.ok(ev.param >= 0 && ev.param <= 6,
        `PIECE_LOCK param ${ev.param} is not a PieceType — the param convention drifted`);
      sawLock = true;
    }
    if (ev.type === EventType.B2B_EXTEND) {
      assert.equal(ev.param, Math.min(255, s.b2bCount),
        `B2B_EXTEND param ${ev.param} != snapshot b2bCount ${s.b2bCount}`);
      b2bChecks++;
    }
    // Exactly one clear event per placement: LINE_CLEAR never rides along with
    // TETRIS or a TSPIN_*.
    if (ev.type === EventType.LINE_CLEAR) {
      const others = s.events.filter((e) => e.type >= EventType.TETRIS && e.type <= EventType.TSPIN_TRIPLE);
      assert.equal(others.length, 0,
        `LINE_CLEAR fired alongside ${JSON.stringify(others)} — clear events must be exclusive`);
    }
    if (ev.type === EventType.PIECE_LOCK) lockPiece = ev.param;
    if (EXPECTED[ev.type] !== undefined && shouted === null) {
      shouted = ev.type === EventType.TETRIS ? EXPECTED[ev.type] : PieceLetter[lockPiece] + EXPECTED[ev.type];
    }
  }

  const shoutingThisFrame = shouted !== null && shoutedDrawn === null;
  canvas.ops.length = 0;
  r.draw(s);
  if (shoutingThisFrame) {
    shoutedDrawn = canvas.ops
      .filter((o) => o.op === 'fillText' && o.fillStyle === TEST_VARS['--bot-accent'])
      .map((o) => o.args[0]);
  }
  if (shoutedDrawn !== null && b2bChecks >= WANT_B2B_CHECKS) break;
}

assert.ok(sawLock, 'no PIECE_LOCK event ever reached JS — the events getter is not reading the ring');
assert.notEqual(shouted, null,
  'the bot produced no tetris or t-spin in 10 simulated minutes, so nothing could be shouted');

assert.ok(shoutedDrawn !== null && shoutedDrawn.includes(shouted),
  `expected the renderer to shout "${shouted}", drew ${JSON.stringify(shoutedDrawn)}`);
assert.ok(b2bChecks >= WANT_B2B_CHECKS,
  `only ${b2bChecks} B2B_EXTEND params were checked — the param assertion never ran`);

bot.destroy();
r.destroy();
console.log(`callout e2e OK: real C++ event -> "${shouted}", ${b2bChecks} b2b params checked`);
