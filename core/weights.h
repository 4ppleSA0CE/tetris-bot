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
// RETUNED BY DUEL-FITNESS CROSS-ENTROPY (2026-08-24, plan 8), tools/tune.py --duel: 20
// generations, population 40, elite 6; fitness is paired versus wins against the moving
// incumbent mean (5 seed pairs, both orientations, 400-piece cap) with net attack margin
// as the tie-break; searches at --nodes 5200; attackDealt pinned as the unit. This replaced
// solo-attack fitness after plan 7 proved it a broken proxy in both directions.
//
// Gate against the plan-6 solo-tuned vector:
//
//   tools/duel.py, 40 pairs x 2000 pieces:  56W 24L 0D  score 0.700  elo +147  LOS 1.000
//                                           (average game only 88 pieces - it rushes the kill)
//   tools/bench.py 4/16 (8 x 3000):         top-outs 0 vs 6, p99 4.6 ms
//
// KNOWN COST, accepted on purpose: solo attack/piece DROPPED to 0.374 vs 0.613 (4/16:
// 0.546 vs 0.702). The vector plays a ~3-row flat stack, holds b2b chains of ~11 instead
// of 40+, and converts stack into immediate attack. Versus strength is the product goal;
// the solo demo looks tamer for it. If the demo look ever matters more, the plan-6 vector
// in git history is the solo-shaped one.
//
// A post-plan-9 duel-fitness retune at the new 6400-node calibration (20 more self-play
// generations, best-vs-incumbent scores looked healthy throughout) LOST the held-out duel
// against this vector 36-44 (LOS 0.19) and was rejected: self-play fitness tracks the
// moving incumbent's lineage, not held-out strength. The duel gate on fresh seeds is the
// only arbiter; a mid-looking training curve proves nothing either way.
//
// A plan-7 refinement (solo-attack CE after the transposition fold) had the mirror failure:
// +0.07 attack/piece solo, REJECTED on a 35-45 duel loss (LOS 0.13) and 14 vs 3 top-outs.
// Solo bench and duel disagree in both directions - the duel is the gate, always.

constexpr int HEIGHT_PENALTY_THRESHOLD = 12;

constexpr float W_ATTACK_DEALT = 100.0f;

// THE ONLY WEIGHT KEEPING THE BOT ALIVE: zero it and the run tops out at piece 107; zeroing
// any other weight alone costs no survival. If the bot starts topping out, look here first.
constexpr float W_HOLES = -142.0f;

constexpr float W_COVERED_CELLS = -10.6f;

// Back to ~0 from plan-6's +2.1: rowTransitions at -57.9 now carries all surface tidiness,
// and the duel meta stopped paying for texture.
constexpr float W_BUMPINESS = -1.1f;

// Matched pair: a bounded reward for building and the cliff that bounds it, applied to
// (height - 12)^2. The duel tune softened the cliff from -189 to -51 - a bot that plays
// at 3 rows almost never sees row 12, so the fitness stopped defending the wall.
constexpr float W_MAX_HEIGHT = 12.0f;
constexpr float W_HEIGHT_PENALTY = -50.7f;

constexpr float W_ROW_TRANSITIONS = -57.9f;
constexpr float W_COLUMN_TRANSITIONS = -8.3f;

// ~0 from plan-6's +3.5: at a 3-row stack the well is one hard drop away from existing
// anyway, so the tuner stopped paying rent on it.
constexpr float W_WELL_DEPTH = 0.3f;

// countTSlots() recognises TSD-shaped slots only; TST and side-entry slots score 0. The bot
// still finds those placements, it just is not paid to keep them open.
constexpr float W_T_SLOT_COUNT = 255.5f;

// Down from plan-6's 241: duels average 88 pieces, so a long-held chain rarely pays out.
// Spend the chain, send the lines, kill first.
constexpr float W_B2B_ACTIVE = 51.0f;

// Per line of Surge the live chain holds. Near the 1:1 attack rate: the horizon argument
// (the search sees a break's payout but not the held chain's future +1s) still applies,
// but the duel meta discounts futures that arrive after the opponent is dead.
constexpr float W_B2B_CHARGE = 96.5f;

// BCTS's strongest term (Thiery & Scherrer 2009): a second hole in an already-holed row is
// nearly free, a hole in a clean row costs a whole row.
constexpr float W_ROWS_WITH_HOLES = -71.9f;

// Partial refund of W_HOLES for holes a piece can still slide into from the side.
constexpr float W_OVERHANGS = 8.3f;

// Move terms (every versus bot has them: Cold Clear clear1..3 / wasted_t, ZZZ, Hikari).
// A clear that does not maintain B2B spends stack for little attack; a T without a spin
// spends the piece the whole T-slot economy is built around.
constexpr float W_PLAIN_CLEAR = -118.6f;
constexpr float W_WASTED_T    = -95.1f;

// Times the extra height-cliff area pending garbage would add if it rose right now (plan 7).
// Deliberately much softer than W_HEIGHT_PENALTY: charging the full cliff for garbage that
// has not landed yet made the bot flatten, break its chain and DIE MORE (bench: 25 top-outs
// vs 9 at 8x3000 under 4/16).
constexpr float W_INCOMING_RISK = -29.4f;

} // namespace tb
