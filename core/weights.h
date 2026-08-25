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
// RETUNED (2026-08-24, plan 12): the chain-holding vector -- gen-20 BEST INDIVIDUAL of a
// pool-fitness CE run whose fitness added max-b2b after attack (the mean of the same run
// was mediocre: elite averaging washed the chain style out; a dominant individual re-gated
// on held-out seeds held up). Cutout weights and b2bLevel clamped to 0 at ship (cutout ON
// was worth 9 solo top-outs; the tuner's tslot2=16.6 did not survive its own gate).
//
// Gates vs the plan-11 vector, deterministic --nodes 6400 (machine was under load; the
// wall-clock p99 check is deferred to an idle machine and eval cost is unchanged):
//
//   duel.py 80 games:   49W 31L  score 0.613  elo +80  LOS 0.978   <- the gate that rules
//   solo 8x3000:        0.635 vs 0.680 attack/piece, top-outs 5 vs 0, max b2b ~140 vs ~25
//   4/16  8x3000:       0.751 vs 0.811, top-outs 0 vs 0
//
// THE TRADE, taken deliberately: b2b chains went from ~25 to 118-161 (the round's goal,
// and the user's explicit ask) and held-out duels are decisively BETTER -- surge bursts
// win fights -- but scoreboard attack/piece dropped ~0.05 and pure-solo runs die about
// once per 5000 pieces refusing to break the chain to downstack. If solo APP ever matters
// more than fights and chains, the plan-11 vector in git history is the efficient one.
//
// A plan-7 refinement (solo-attack CE after the transposition fold) had the mirror failure:
// +0.07 attack/piece solo, REJECTED on a 35-45 duel loss (LOS 0.13) and 14 vs 3 top-outs.
// Solo bench and duel disagree in both directions - the duel is the gate, always.

constexpr int HEIGHT_PENALTY_THRESHOLD = 12;

constexpr float W_ATTACK_DEALT = 100.0f;

// THE ONLY WEIGHT KEEPING THE BOT ALIVE: zero it and the run tops out at piece 107; zeroing
// any other weight alone costs no survival. If the bot starts topping out, look here first.
constexpr float W_HOLES = -169.7f;

constexpr float W_COVERED_CELLS = -3.6f;

// Back to ~0 from plan-6's +2.1: rowTransitions at -57.9 now carries all surface tidiness,
// and the duel meta stopped paying for texture.
constexpr float W_BUMPINESS = 3.1f;

// Matched pair: a bounded reward for building and the cliff that bounds it, applied to
// (height - 12)^2. The duel tune softened the cliff from -189 to -51 - a bot that plays
// at 3 rows almost never sees row 12, so the fitness stopped defending the wall.
constexpr float W_MAX_HEIGHT = 14.2f;
constexpr float W_HEIGHT_PENALTY = -19.5f;

constexpr float W_ROW_TRANSITIONS = -65.5f;
constexpr float W_COLUMN_TRANSITIONS = 0.8f;

// ~0 from plan-6's +3.5: at a 3-row stack the well is one hard drop away from existing
// anyway, so the tuner stopped paying rent on it.
constexpr float W_WELL_DEPTH = -8.8f;

// countTSlots() recognises TSD-shaped slots only; TST and side-entry slots score 0. The bot
// still finds those placements, it just is not paid to keep them open. Since plan 11 this
// counts the slots REMAINING after the virtual cutouts below.
constexpr float W_T_SLOT_COUNT = 189.0f;

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
// Chain-holding levers (plan 12), tuner-owned, shipped inert at 0: b2bLevel pays per held
// b2b level (min(count, 8)) where Surge charge is still silent; b2bBreak is charged by the
// search on top of plainClear when a clear kills a LIVE chain.
constexpr float W_B2B_LEVEL = 0.0f;
constexpr float W_B2B_BREAK = -1.4f;

constexpr float W_B2B_ACTIVE = 34.1f;

// Per line of Surge the live chain holds. Near the 1:1 attack rate: the horizon argument
// (the search sees a break's payout but not the held chain's future +1s) still applies,
// but the duel meta discounts futures that arrive after the opponent is dead.
constexpr float W_B2B_CHARGE = 126.2f;

// BCTS's strongest term (Thiery & Scherrer 2009): a second hole in an already-holed row is
// nearly free, a hole in a clean row costs a whole row.
constexpr float W_ROWS_WITH_HOLES = -93.3f;

// Partial refund of W_HOLES for holes a piece can still slide into from the side.
constexpr float W_OVERHANGS = -4.4f;

// Move terms (every versus bot has them: Cold Clear clear1..3 / wasted_t, ZZZ, Hikari).
// A clear that does not maintain B2B spends stack for little attack; a T without a spin
// spends the piece the whole T-slot economy is built around.
constexpr float W_PLAIN_CLEAR = -130.7f;
constexpr float W_WASTED_T    = -101.9f;

// Times the extra height-cliff area pending garbage would add if it rose right now (plan 7).
// Deliberately much softer than W_HEIGHT_PENALTY: charging the full cliff for garbage that
// has not landed yet made the bot flatten, break its chain and DIE MORE (bench: 25 top-outs
// vs 9 at 8x3000 under 4/16).
constexpr float W_INCOMING_RISK = -34.3f;

} // namespace tb
