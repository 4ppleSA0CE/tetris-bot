#pragma once

namespace tb {

// Evaluator weights (PRD 4.8). Each carries its own sign; evaluate() is a plain dot product,
// except the move terms (attackDealt, plainClear, wastedT), which the search applies per node
// with the gamma discount.
//
// UNIT: W_ATTACK_DEALT is the anchor - one garbage line of attack is 100 points, so every
// other weight is in hundredths of a garbage line.
//
// TUNE AT THE SHIPPED 5 ms BUDGET, never unlimited. The search is anytime and a taller stack
// needs deeper search to dig out of, so stack ambition and the budget are in direct tension:
// a vector that met every target at --budget 1000000 died 0 for 3 at the real budget.
// Throughput (and therefore depth) also depends on LTO being on in CMakeLists.txt.
//
// Override without a rebuild:  ./build/tetris_bot --weights tSlotCount=240,maxHeight=15
//
// RETUNED BY CROSS-ENTROPY (2026-08-24, plan 6), tools/tune.py: 30 generations, population
// 50, elite 6; every candidate in a generation plays the same seeds, 1500 pieces solo plus
// 1500 under --garbage 4/16; fitness is fewest top-outs then most attack; searches ran at
// --nodes 5200, the deterministic stand-in calibrated to the 4.5 ms clock; attackDealt
// pinned as the unit.
//
// Gate (tools/bench.py, wall clock, one process at a time, 8 seeds x 3000 pieces, against
// the plan-5 12-weight vector):
//
//   solo            attack/piece 0.592 +- 0.014  vs  0.449 +- 0.010   top-outs 0 vs 0
//   --garbage 4/16  attack/piece 0.684 +- 0.017  vs  0.530 +- 0.019   top-outs 9 vs 49
//
// Acceptance, seed 42 x 10000 at the shipped budget: attack 6005 (surge 1647), spins 18.3
// per 100, max b2b 59, top-outs 0, p99 4.5 ms, avg height 5.2, max height 19.
//
// STYLE CHANGED with the retune: the plan-5 vector breathed at 8-9 rows; this one plays a
// low stack (avg ~5.2) with a kept well - more Cold-Clear-shaped. If PRD 1.1's taller
// climb-and-collapse look ever matters more than attack, retune with a height term in the
// fitness rather than hand-bumping maxHeight.

constexpr int HEIGHT_PENALTY_THRESHOLD = 12;

constexpr float W_ATTACK_DEALT = 100.0f;

// THE ONLY WEIGHT KEEPING THE BOT ALIVE: zero it and the run tops out at piece 107; zeroing
// any other weight alone costs no survival. If the bot starts topping out, look here first.
constexpr float W_HOLES = -111.8f;

constexpr float W_COVERED_CELLS = -9.3f;

// POSITIVE, and that is the tuner's answer, not a typo: with rowTransitions at -33.6
// carrying surface tidiness, the old -4 was redundant, and the elite drifted through zero
// to a small reward for texture (steps beside the well are where spins live).
constexpr float W_BUMPINESS = 2.1f;

// Matched pair, same structure as always: a bounded reward for building, and the cliff that
// bounds it, applied to (height - 12)^2. Raise the reward without the cliff and the bot
// stacks into the ceiling; raise the cliff without the reward and the stack pins flat.
constexpr float W_MAX_HEIGHT = 11.3f;
constexpr float W_HEIGHT_PENALTY = -189.3f;

constexpr float W_ROW_TRANSITIONS = -33.6f;
constexpr float W_COLUMN_TRANSITIONS = -8.1f;

// POSITIVE: an open well is worth paying for (Cold Clear ships well_depth positive too).
// The old -2 treated wells as a hazard; under pressure the kept well is how quads and
// surge get banked, and the tuner flipped it.
constexpr float W_WELL_DEPTH = 3.5f;

// countTSlots() recognises TSD-shaped slots only; TST and side-entry slots score 0. The bot
// still finds those placements, it just is not paid to keep them open.
constexpr float W_T_SLOT_COUNT = 235.8f;

constexpr float W_B2B_ACTIVE = 241.3f;

// Per line of Surge the live chain holds. Above the 1:1 attack rate on purpose: the search
// horizon sees a break's payout but not the +1 per clear a held chain keeps earning.
constexpr float W_B2B_CHARGE = 113.8f;

// BCTS's strongest term (Thiery & Scherrer 2009): a second hole in an already-holed row is
// nearly free, a hole in a clean row costs a whole row.
constexpr float W_ROWS_WITH_HOLES = -62.7f;

// Partial refund of W_HOLES for holes a piece can still slide into from the side.
constexpr float W_OVERHANGS = 8.9f;

// Move terms (every versus bot has them: Cold Clear clear1..3 / wasted_t, ZZZ, Hikari).
// A clear that does not maintain B2B spends stack for little attack; a T without a spin
// spends the piece the whole T-slot economy is built around.
constexpr float W_PLAIN_CLEAR = -114.7f;
constexpr float W_WASTED_T    = -109.4f;

} // namespace tb
