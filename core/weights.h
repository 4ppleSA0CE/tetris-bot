#pragma once

namespace tb {

// Evaluator weights (PRD 4.8). Each carries its own sign; evaluate() is a plain dot product.
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
// RETUNED FOR TETR.IO SEASON 2 RULES (2026-08-23, plan 5). Non-T spins are minis (0/1/2/4
// attack, not 2/4/6), the immobile rule checks up, and a broken chain releases its Surge.
// b2bCharge swept at the shipped budget, 3 seeds x 3000 pieces:
//
//   b2bCharge  attack  max b2b  surge  top-outs  avg height  max height  p99 ms
//           0    1179      6.0    130         0        7.39          17     4.6
//          50    1187      7.3    139         0        7.48          16     4.6
//         100    1234     15.7    173         0        7.68          16     4.7
//         150    1392     31.3    260         0        8.72          18     4.5  <- SHIPPED
//         200    1253     30.7    235         1        9.73          21     5.5
//
// Acceptance at the shipped vector (PRD 11), seed 42 x 10000 and seeds 7 / 1234 / 99 x 5000:
//   top-outs 0 / 0 / 0 / 0   spins 12.8-14.6 per 100   max b2b 26-45   p99 4.5-4.6 ms
//   avg height 8.3-8.7   max height 17-19   attack 4490 per 10000 pieces (surge 810)

constexpr int HEIGHT_PENALTY_THRESHOLD = 12;

constexpr float W_ATTACK_DEALT = 100.0f;

// THE ONLY WEIGHT KEEPING THE BOT ALIVE: zero it and the run tops out at piece 107; zeroing
// any other weight alone costs no survival. If the bot starts topping out, look here first.
constexpr float W_HOLES = -110.0f;

constexpr float W_COVERED_CELLS = -6.0f;

// Small on purpose: a flat stack survives and is a tuning failure (PRD 1.1 wants it
// breathing between 8 and 12 rows).
constexpr float W_BUMPINESS = -4.0f;

// POSITIVE ON PURPOSE. With every health term negative nothing rewards building, and the
// bot plays a 2.7-row pancake that breaks its own chain with singles. This is a bounded
// reward; W_HEIGHT_PENALTY is the cliff that bounds it, applied to (height - 12)^2. The two
// are a MATCHED PAIR: raise the reward without the cliff and the bot stacks into the
// ceiling, raise the cliff without the reward and the stack pins flat again.
constexpr float W_MAX_HEIGHT = 10.0f;
constexpr float W_HEIGHT_PENALTY = -120.0f;

constexpr float W_ROW_TRANSITIONS = -3.0f;
constexpr float W_COLUMN_TRANSITIONS = -4.5f;

// Small on purpose: the bot needs a 4-deep well to score quads.
constexpr float W_WELL_DEPTH = -2.0f;

// countTSlots() recognises TSD-shaped slots only; TST and side-entry slots score 0. The bot
// still finds those placements, it just is not paid to keep them open.
constexpr float W_T_SLOT_COUNT = 180.0f;

constexpr float W_B2B_ACTIVE = 150.0f;

// Per line of Surge the live chain holds. Above the 1:1 attack rate on purpose: the search
// horizon sees a break's payout but not the +1 per clear a held chain keeps earning.
constexpr float W_B2B_CHARGE = 150.0f;

// BCTS's strongest term (Thiery & Scherrer 2009): a second hole in an already-holed row is
// nearly free, a hole in a clean row costs a whole row.
constexpr float W_ROWS_WITH_HOLES = -40.0f;

// Partial refund of W_HOLES for holes a piece can still slide into from the side.
constexpr float W_OVERHANGS = 30.0f;

// Move terms (every versus bot has them: Cold Clear clear1..3 / wasted_t, ZZZ, Hikari).
// A clear that does not maintain B2B spends stack for little attack; a T without a spin
// spends the piece the whole T-slot economy is built around.
constexpr float W_PLAIN_CLEAR = -50.0f;
constexpr float W_WASTED_T    = -80.0f;

} // namespace tb
