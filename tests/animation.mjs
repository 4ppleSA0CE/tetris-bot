import assert from 'node:assert/strict';
import { createTetrisBot } from '../js/index.js';

const FRAME_MS = 1000 / 60;

{
  const bot = await createTetrisBot({ seed: 42, pps: 5 });
  let t = 0;
  let serial = 0;
  let states = 0;
  let worstStates = Infinity;
  let biggestJump = 0;
  let animatedFalls = 0;
  let biggestFall = 0;
  let descentFrames = 0;
  let px = null;
  let py = null;
  let pieces = 0;

  for (let f = 0; f < 3600; f++) {
    t += FRAME_MS;
    bot.tick(t);
    const s = bot.snapshot();
    if (s.piecesPlaced !== serial) {
      if (serial > 0 && states < worstStates) worstStates = states;
      if (serial > 0 && descentFrames >= 2) animatedFalls++;
      serial = s.piecesPlaced;
      pieces++;
      states = 0;
      descentFrames = 0;
      px = null;
      py = null;
    } else if (px !== null) {
      if (s.activeX !== px || s.activeY !== py) states++;

      const jump = Math.abs(s.activeX - px);
      if (jump > biggestJump) biggestJump = jump;
      if (s.activeY < py) {
        descentFrames++;
        if (py - s.activeY > biggestFall) biggestFall = py - s.activeY;
      }
    }
    px = s.activeX;
    py = s.activeY;
    assert.ok(s.pathProgress >= 0 && s.pathProgress <= 255);
  }
  bot.destroy();

  console.log(`slide: ${pieces} pieces, min distinct states/piece ${worstStates}, biggest single-frame sideways jump ${biggestJump}, ${animatedFalls} animated falls, biggest fall step ${biggestFall}`);
  assert.ok(pieces > 250, `only ${pieces} pieces in 60s at 5 pps`);
  assert.ok(worstStates >= 1,
    `some piece never moved on screen (${worstStates} transitions) — that reads as a teleport`);

  assert.ok(biggestJump <= 8,
    `piece moved ${biggestJump} columns in one frame — the path is being skipped`);
  assert.ok(animatedFalls * 2 >= pieces, `only ${animatedFalls} of ${pieces} falls spanned 2+ frames`);
  assert.ok(biggestFall <= 12, `a piece fell ${biggestFall} cells in one frame — that is a teleport, not a fall`);
}

{
  const bot = await createTetrisBot({ seed: 42, pps: 20 });
  let t = 0, serial = 0, pieceStart = 0, spinThisPiece = false;
  let spins = 0, plains = 0, longestSpin = 0, longestPlain = 0;

  for (let f = 0; f < 3600; f++) {
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

  assert.ok(longestSpin <= 70,
    `a spin placement took ${longestSpin.toFixed(1)}ms - tempo dilation has come back`);
  assert.ok(longestPlain <= 70, `a plain placement took ${longestPlain.toFixed(1)}ms`);
}

console.log('animation OK: pieces step along the path and every placement is paced the same');
