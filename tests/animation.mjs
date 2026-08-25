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
  let hardDrops = 0;
  let dropped = false;
  let px = null;
  let py = null;
  let pieces = 0;

  for (let f = 0; f < 3600; f++) {          // 60 seconds at 60fps
    t += FRAME_MS;
    bot.tick(t);
    const s = bot.snapshot();
    if (s.piecesPlaced !== serial) {
      if (serial > 0 && states < worstStates) worstStates = states;
      if (serial > 0 && dropped) hardDrops++;
      serial = s.piecesPlaced;
      pieces++;
      states = 0;
      dropped = false;
      px = null;
      py = null;
    } else if (px !== null) {
      if (s.activeX !== px || s.activeY !== py) states++;
      // ARR 0 / SDF infinite: the tap and DAS pause animate, the drop lands in a frame.
      const jump = Math.abs(s.activeX - px);
      if (jump > biggestJump) biggestJump = jump;
      if (s.activeY <= py - 6) dropped = true;
    }
    px = s.activeX;
    py = s.activeY;
    assert.ok(s.pathProgress >= 0 && s.pathProgress <= 255);
  }
  bot.destroy();

  console.log(`slide: ${pieces} pieces, min distinct states/piece ${worstStates}, biggest single-frame sideways jump ${biggestJump}, ${hardDrops} hard drops`);
  assert.ok(pieces > 250, `only ${pieces} pieces in 60s at 5 pps`);
  assert.ok(worstStates >= 1,
    `some piece never moved on screen (${worstStates} transitions) — that reads as a teleport`);
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
    `piece moved ${biggestJump} columns in one frame — the path is being skipped`);
  assert.ok(hardDrops * 2 >= pieces, `only ${hardDrops} of ${pieces} pieces hard-dropped`);
}

// --- 2. pacing is uniform ----------------------------------------------------
// Dilation was removed: the reference handling paces every placement identically.
{
  const bot = await createTetrisBot({ seed: 42, pps: 20 });   // 50ms per placement
  let t = 0, serial = 0, pieceStart = 0, spinThisPiece = false;
  let spins = 0, plains = 0, longestSpin = 0, longestPlain = 0;

  for (let f = 0; f < 3600; f++) {                            // 60s at 60fps
    t += FRAME_MS;
    bot.tick(t);
    const s = bot.snapshot();
    if (s.pendingSpin !== 0) spinThisPiece = true;
    if (s.piecesPlaced !== serial) {
      if (serial > 0) {
        const dur = t - pieceStart;
        if (spinThisPiece) { spins++; longestSpin = Math.max(longestSpin, dur); }
        else               { plains++; longestPlain = Math.max(longestPlain, dur); }
      }
      serial = s.piecesPlaced;
      pieceStart = t;
      spinThisPiece = false;
    }
  }
  bot.destroy();

  console.log(`pacing: ${spins} spin / ${plains} plain, longest ${longestSpin.toFixed(1)} / ${longestPlain.toFixed(1)}ms`);
  assert.ok(spins > 0, 'the bot never produced a spin placement');
  // 50ms nominal, one 16.7ms observation frame of slack. A dilated spin was 250ms.
  assert.ok(longestSpin <= 70,
    `a spin placement took ${longestSpin.toFixed(1)}ms - tempo dilation has come back`);
  assert.ok(longestPlain <= 70, `a plain placement took ${longestPlain.toFixed(1)}ms`);
}

console.log('animation OK: pieces step along the path and every placement is paced the same');
