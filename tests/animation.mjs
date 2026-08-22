// PRD section 11: "Piece movement is visibly animated, never teleported."
// PRD section 7.2: tempo dilation stretches the tail of a spin placement.
import assert from 'node:assert/strict';
import { createTetrisBot } from '../js/index.js';

const FRAME_MS = 1000 / 60;

// --- 1. the piece slides ----------------------------------------------------
{
  const bot = await createTetrisBot({ seed: 42, pps: 5 });
  let t = 0;
  let serial = 0;
  let states = 0;
  let worstStates = Infinity;
  let biggestJump = 0;
  let px = null;
  let py = null;
  let pieces = 0;

  for (let f = 0; f < 3600; f++) {          // 60 seconds at 60fps
    t += FRAME_MS;
    bot.tick(t);
    const s = bot.snapshot();
    if (s.piecesPlaced !== serial) {
      if (serial > 0 && states < worstStates) worstStates = states;
      serial = s.piecesPlaced;
      pieces++;
      states = 0;
      px = null;
      py = null;
    } else if (px !== null) {
      if (s.activeX !== px || s.activeY !== py) states++;
      const jump = Math.abs(s.activeX - px) + Math.abs(s.activeY - py);
      if (jump > biggestJump) biggestJump = jump;
    }
    px = s.activeX;
    py = s.activeY;
    assert.ok(s.pathProgress >= 0 && s.pathProgress <= 255);
  }
  bot.destroy();

  console.log(`slide: ${pieces} pieces, min distinct states/piece ${worstStates}, biggest single-frame jump ${biggestJump}`);
  assert.ok(pieces > 250, `only ${pieces} pieces in 60s at 5 pps`);
  assert.ok(worstStates >= 2,
    `some piece showed only ${worstStates} distinct positions — that reads as a teleport`);
  // THE THRESHOLD IS NOT 1, AND CANNOT BE. The core samples its N-step BFS path at
  // the frame rate: a piece gets D/16.7 frames (12 at 5 pps, 3 at 20 pps) to show a
  // path that is typically 25+ steps, so advancing several steps per frame is the
  // design, not a skip. renderers/canvas-mono.ts is what makes that smooth - it eases
  // between these samples with SLIDE_TAU_MS, so a 6-cell core step is a visible slide
  // on screen, not a jump.
  // Measured over 60s x seed 42: max 6 at 5 pps (89% of frames move <=2), 8 at 10 pps,
  // 12 at 20 pps. 8 leaves margin at this speed while staying far below what an actual
  // teleport looks like: the piece spawns at row 21 and locks near the stack, so
  // skipping the replay entirely reads as a single ~20-cell jump.
  assert.ok(biggestJump <= 8,
    `piece moved ${biggestJump} cells in one frame — the path is being skipped`);
}

// --- 2. tempo dilation ------------------------------------------------------
{
  const bot = await createTetrisBot({ seed: 42, pps: 20 });
  let t = 0;
  let spinStart = -1;
  let spinSerial = 0;
  let spins = 0;
  let shortest = Infinity;
  let routineTotal = 0;
  let routineCount = 0;
  let pieceStart = 0;
  let serial = 0;
  let sawSpinThisPiece = false;

  for (let f = 0; f < 36000; f++) {         // 10 minutes at 60fps
    t += FRAME_MS;
    bot.tick(t);
    const s = bot.snapshot();
    if (s.pendingSpin !== 0 && spinStart < 0) {
      spinStart = t;
      spinSerial = s.piecesPlaced;
      sawSpinThisPiece = true;
    }
    if (s.piecesPlaced !== serial) {
      if (spinStart >= 0 && s.piecesPlaced !== spinSerial) {
        const dur = t - spinStart;
        spins++;
        if (dur < shortest) shortest = dur;
        spinStart = -1;
      } else if (!sawSpinThisPiece && serial > 0) {
        routineTotal += t - pieceStart;
        routineCount++;
      }
      serial = s.piecesPlaced;
      pieceStart = t;
      sawSpinThisPiece = false;
    }
  }
  bot.destroy();

  const routineAvg = routineTotal / Math.max(1, routineCount);
  console.log(`dilation: ${spins} spin placements, shortest ${shortest.toFixed(1)}ms, routine avg ${routineAvg.toFixed(1)}ms`);
  assert.ok(spins > 0, 'the bot never produced a spin placement in 10 simulated minutes');
  // 20 pps -> 50ms head + 200ms dilated tail = 250ms, minus one 16.7ms observation frame.
  assert.ok(shortest >= 190,
    `a spin placement took only ${shortest.toFixed(1)}ms — dilation is not applied`);
  // Routine placements stay at 1000/20 = 50ms, well under the dilated floor.
  assert.ok(routineAvg < 120,
    `routine placements average ${routineAvg.toFixed(1)}ms — dilation is leaking onto everything`);
}

console.log('animation OK: pieces slide along the path and spins are dilated');
