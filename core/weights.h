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
// RETUNED (2026-08-24, plan 11) by pool-fitness CE: 25 generations, pop 40, elite 6;
// fitness = paired versus wins against a 2-opponent POOL (compiled ship + previous mean,
// 2000-piece caps so chain economy has room) THEN solo top-outs THEN SOLO ATTACK (the
// round's goal: the plan-8 vector won fights but skimmed at 0.62 attack/piece and looked
// bad doing it) THEN margin; --nodes 6400.
//
// Gates against the plan-8 vector (all at the 4.5 ms clock unless noted):
//
//   tools/duel.py 80 games @6400 nodes:  41W 39L  score 0.512  (not-worse, slight edge)
//   bench solo 8x3000:  attack/piece 0.700 vs 0.613 (+14%), top-outs 0 vs 0, p99 4.5
//   bench 4/16 8x3000:  attack/piece 0.797 vs 0.727 (+10%), top-outs 0 vs 0
//   spin rate ~20-24 per 100 (was ~17); stack ~3.4-5 rows (was ~2.9)
//
// UNMET BAR, recorded honestly: mean max-b2b is ~20-30 against a target of 40 -- the
// chains are longer than plan-8's ~11 but still short. Longer chains likely need the
// cutout actually ON with sane weights (see below) plus a pressure regime in the tuner
// fitness; next tuning round's problem.
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
constexpr float W_HOLES = -84.4f;

constexpr float W_COVERED_CELLS = -3.9f;

// Back to ~0 from plan-6's +2.1: rowTransitions at -57.9 now carries all surface tidiness,
// and the duel meta stopped paying for texture.
constexpr float W_BUMPINESS = -0.1f;

// Matched pair: a bounded reward for building and the cliff that bounds it, applied to
// (height - 12)^2. The duel tune softened the cliff from -189 to -51 - a bot that plays
// at 3 rows almost never sees row 12, so the fitness stopped defending the wall.
constexpr float W_MAX_HEIGHT = 17.1f;
constexpr float W_HEIGHT_PENALTY = -28.0f;

constexpr float W_ROW_TRANSITIONS = -56.0f;
constexpr float W_COLUMN_TRANSITIONS = 2.1f;

// ~0 from plan-6's +3.5: at a 3-row stack the well is one hard drop away from existing
// anyway, so the tuner stopped paying rent on it.
constexpr float W_WELL_DEPTH = 2.2f;

// countTSlots() recognises TSD-shaped slots only; TST and side-entry slots score 0. The bot
// still finds those placements, it just is not paid to keep them open. Since plan 11 this
// counts the slots REMAINING after the virtual cutouts below.
constexpr float W_T_SLOT_COUNT = 234.9f;

// Virtual T-spin cutout rewards (plan 11, Cold Clear's tslot idea): a ready slot is
// virtually executed and the board evaluated as if the spin gets taken. Hand-seeded
// 100/300 LOST the duel against zeroed rewards 11-29 (LOS 0.002, avg game 65 pieces) --
// the rest of the vector cannot absorb rewards that large, the bot chases slots and
// dies. Shipped at 0 (the cutout still reshapes the other features); the tuner owns
// the values from plan 11's retune on.
constexpr float W_TSLOT_1 = 0.0f;
constexpr float W_TSLOT_2 = 0.0f;

// Down from plan-6's 241: duels average 88 pieces, so a long-held chain rarely pays out.
// Spend the chain, send the lines, kill first.
constexpr float W_B2B_ACTIVE = 29.2f;

// Per line of Surge the live chain holds. Near the 1:1 attack rate: the horizon argument
// (the search sees a break's payout but not the held chain's future +1s) still applies,
// but the duel meta discounts futures that arrive after the opponent is dead.
constexpr float W_B2B_CHARGE = 95.4f;

// BCTS's strongest term (Thiery & Scherrer 2009): a second hole in an already-holed row is
// nearly free, a hole in a clean row costs a whole row.
constexpr float W_ROWS_WITH_HOLES = -97.2f;

// Partial refund of W_HOLES for holes a piece can still slide into from the side.
constexpr float W_OVERHANGS = 9.7f;

// Move terms (every versus bot has them: Cold Clear clear1..3 / wasted_t, ZZZ, Hikari).
// A clear that does not maintain B2B spends stack for little attack; a T without a spin
// spends the piece the whole T-slot economy is built around.
constexpr float W_PLAIN_CLEAR = -182.6f;
constexpr float W_WASTED_T    = -95.1f;

// Times the extra height-cliff area pending garbage would add if it rose right now (plan 7).
// Deliberately much softer than W_HEIGHT_PENALTY: charging the full cliff for garbage that
// has not landed yet made the bot flatten, break its chain and DIE MORE (bench: 25 top-outs
// vs 9 at 8x3000 under 4/16).
constexpr float W_INCOMING_RISK = -35.9f;

} // namespace tb
