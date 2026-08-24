#pragma once

namespace tb {

// ============================================================================
// Tuned weight vector for the evaluator (PRD 4.8).
//
// EVERY WEIGHT CARRIES ITS OWN SIGN and evaluate() is a plain dot product with no sign
// juggling. Note W_MAX_HEIGHT is now POSITIVE -- that is deliberate and is explained at
// its definition below. It is the one intentional departure from "board-health weights
// are negative", and it is a design decision, not a compensating hack.
//
// UNIT: W_ATTACK_DEALT is the anchor. One point of attack (one garbage line in the
// phantom versus match of PRD 1.1) is worth 100 evaluator points. Every other number here
// is therefore denominated in hundredths of a garbage line, which is what makes them
// comparable by eye.
//
// ---------------------------------------------------------------------------
// HOW THESE WERE TUNED -- read before changing any of them.
//
// Every number below was swept at the STOCK 5 ms budget, not at an unlimited one, over
// 1000 pieces x 5 seeds (42, 7, 1234, 99, 2024). That distinction is the single most
// important fact about this vector. Tuning at an unlimited budget produces a completely
// different and WRONG answer: the configuration `health x0.35, maxHeight=+20` meets every
// PRD 11 target at an unbounded budget (3/3 survival, avgH 10.38, sd 3.37, b2b 7) and
// then DIES at the shipped 5 ms budget -- 2/3 survival at maxHeight=+20, and 0/3 at +30.
//
// The mechanism: a tall stack needs deep search to resolve safely. Measured throughput is
// ~50,000 generateMoves calls/sec, so 5 ms buys ~250 calls and the beam truncates to
// roughly depth 2. The bot builds a stack it then cannot dig out of. Stack ambition and
// the time budget are in direct tension, and the budget wins.
//
// FINAL ACCEPTANCE at these values -- seed 42, 10000 pieces, repeated 3x, plus seeds
// 7 / 1234 / 99 at 5000. Every PRD 11 target met on every run:
//     top-outs 0 / 0 / 0        (needs 0)
//     lines ~3995   attack ~3920
//     t-spins 7.98 - 8.20 /100  (needs >= 1.00)
//     max b2b 6 / 7 / 7         (needs >= 5)
//     avg height 9.08 - 9.17    (needs [8, 12])
//     height sd 2.61 - 2.66     (needs >= 2.0)
//     search p99 4.8 ms         (needs < 5.0)
// versus the untuned starting vector, same conditions:
//     attack 58   t-spins 1.22/100   max b2b 1   avg height 2.57   height sd 1.03
// Attack is ~68x the starting vector's per-1000-piece rate and the bot now breathes.
//
// Note these numbers depend on link-time optimization being ON (CMakeLists.txt). LTO buys
// 1.32x throughput, which buys search depth inside the fixed budget; without it this same
// vector tops out 2 times in 10000 rather than 0.
//
// Override any of them at runtime without a rebuild:
//     ./build/tetris_bot --weights tSlotCount=240,maxHeight=15
// Reproduce the tuning runs with --budget 1000000 ONLY if you want the unbounded numbers;
// for anything that must reflect shipped behaviour, leave --budget alone.
// ============================================================================


// RETUNED FOR ALL-SPIN (2026-08-23). J/L/S/Z now spin by the immobile rule, which
// multiplied the available back-to-back material: attack 3813 -> 10267 and max b2b
// 7 -> 78 over 10000 pieces on seed 1, with no weight change at all. The pair below
// was then re-swept against that, 10 seeds x 3000 pieces at the shipped budget:
//
//   maxHeight  heightPenalty  top-outs  attack  max b2b  max height
//          16            -60         1   29584       75          21   <- previous
//          10           -120         0   30032       77          18   <- SHIPPED
//           6           -200         0   29715       63          15
//          16           -200         0   29375       63          17
//
// The shipped pair dominates the previous one on every axis at once: it tops out
// less, attacks more, chains longer, and keeps 2 more rows of headroom under the
// ceiling. That is not a trade-off resolved by taste - the old pair was simply
// mistuned for a board where only T could spin. The previous pair's max height of
// 21 IS its top-out: it reached the ceiling.

// Height above which heightPenalty starts to bite (PRD 4.8, shared contract core/eval.h).
constexpr int HEIGHT_PENALTY_THRESHOLD = 12;

// The unit. Everything else is denominated in it, so changing it rescales the whole vector
// rather than shifting a trade-off -- prefer changing the other weights instead.
constexpr float W_ATTACK_DEALT = 100.0f;

// One hole costs 1.1 attack. HALVED from the starting -220.
//
// THIS IS THE ONLY WEIGHT KEEPING THE BOT ALIVE. Measured by single-weight ablation: zero
// it and the bot tops out at piece 107, while zeroing any OTHER weight individually costs
// no survival at all. If the bot ever starts topping out, this is the weight to look at
// first -- not W_HEIGHT_PENALTY, which the starting notes wrongly nominated.
// Tuned against: survival 5/5. Rejected: full -220 (survives, but pins the stack flat at
// avg height 2.57 and holds attack to 58); -77 i.e. x0.35 (dies 1/3 at the 5 ms budget).
constexpr float W_HOLES = -110.0f;

// Burial depth, not hole count. Halved with the rest of the board-health block.
// Tuned against: no measurable independent effect in ablation; scaled with its siblings.
constexpr float W_COVERED_CELLS = -6.0f;

// DELIBERATELY SMALL. Bumpiness flattens the stack, and a flat stack is a tuning FAILURE
// even though it survives (PRD 1.1 wants the stack breathing 8-12 rows).
// Tuned against: `--heights` reporting height sd >= 2.0. Final value 2.62.
constexpr float W_BUMPINESS = -4.0f;

// POSITIVE ON PURPOSE. THIS IS THE DELIBERATE SIGN INVERSION -- read this before "fixing" it.
//
// Every other board-health term only ever PUNISHES height, so with all of them negative
// nothing in the evaluator ever rewards building and the bot has no reason to stack. That
// is exactly what was measured at the starting vector: average stack height 2.68, peak 5,
// height sd 1.03, and a clear breakdown of 32 singles / 31 doubles / 8 triples / ZERO
// tetrises. A 2.7-row pancake survives and clears lines and fails what PRD 1.1 actually
// asks for. It also explains the starting vector's max b2b of 1: a single pays zero attack,
// but clearing one still improves every negative health term, so the health terms were
// literally paying the bot to break its own back-to-back chain.
//
// Making this a bounded REWARD, with W_HEIGHT_PENALTY as a hard cliff above row 12, gives
// the "climb then collapse" shape the PRD describes. Note this also switches
// W_HEIGHT_PENALTY on: at the starting vector the stack never passed row 5, so a penalty
// gated at row 12 was completely inert and every value of it produced byte-identical runs.
// Tuned against: avg height in [8, 12] with sd >= 2.0 AND top-outs 0, jointly -- those two
// are in direct tension and most of the sweep was spent on the frontier between them.
// Rejected: +5 and 0 (stack stays under 5 rows, b2b stalls at 2); +8 (survives, but avg
// height 7.5, under the band); +10 with the cliff at -4 (avg height 8.3-8.7, in band, but
// tops out 0-4 times per 10000 depending on wall-clock luck -- not reliably 0).
// This value only works paired with the much harder cliff below.
constexpr float W_MAX_HEIGHT = 10.0f;

// Applied to e*e where e = max(0, maxHeight - 12). The cliff that bounds W_MAX_HEIGHT's
// reward -- these two are a MATCHED PAIR and must be changed together. Raising the reward
// without raising the cliff makes the bot stack into the ceiling; raising the cliff without
// the reward pins it flat again.
//
// This is 7.5x the starting -8, and that is what finally made "avg height in [8,12] AND
// zero top-outs" hold simultaneously: it costs literally nothing below row 12, so it buys
// survival without flattening the stack -- which is exactly the property the original notes
// claimed for it, and which only became true once the stack actually reached row 12.
// Tuned against: top-outs 0 across 3 repeated 10000-piece runs. Rejected: -4 (tops out
// 0-4 times per 10000); -25 and -50 paired with a +13 reward (both work, marginally lower
// attack); a raised cliff alone with maxHeight at +10 (survives, but avg height 7.6-7.9,
// under the band).
constexpr float W_HEIGHT_PENALTY = -120.0f;

// Surface roughness. Only the DIFFERENCE between candidate placements matters (2-6, i.e.
// 0.1-0.4 attack). Halved with the board-health block.
constexpr float W_ROW_TRANSITIONS = -3.0f;

// Correlates with buried holes: a cheap second vote for what W_HOLES punishes.
// Halved with the board-health block.
constexpr float W_COLUMN_TRANSITIONS = -4.5f;

// SMALL ON PURPOSE. The bot NEEDS a 4-deep well to score tetrises; a heavy well weight is
// how bots lose their I-column and stop scoring. Halved with the board-health block.
constexpr float W_WELL_DEPTH = -2.0f;

// A live T-slot is worth 1.8 attack. Left at its starting value: the t-spin rate came out
// at 4.54 per 100, over 4x PRD 11's 1.00 floor, so this never became the binding constraint
// and raising it would only trade away attack.
//
// KNOWN GAP: countTSlots() recognises TSD-shaped slots only (ROT_2 T, nub down). T-spin-triple
// slots and side-entry ROT_R / ROT_L slots score zero, so this weight cannot pay the bot to
// keep them open -- verified directly against a TST-shaped board, which scores 0. The bot
// still FINDS those placements, because generateMoves enumerates them and the search rewards
// their attack; it just is not paid to preserve them. Past 400 the answer is a second
// detector in countTSlots(), not more weight.
constexpr float W_T_SLOT_COUNT = 180.0f;

// Holding the chain. RAISED 2.5x from the starting 60, and this is what finally moved
// max b2b off 1. Note the starting vector's b2b problem was structural, not a matter of
// this weight: at the starting values max b2b was stuck at 1-2 under EVERY single-weight
// ablation, including b2bActive = 0. It only became tunable once W_MAX_HEIGHT let the stack
// build enough to have something to chain with.
// Tuned against: max b2b >= 5 over 1000 pieces x 5 seeds (PRD 11).
// Rejected: 60 (b2b stalls at 3); 300 (works, attack 354, but avg height 11.16 sits at the
// top of the [8,12] band and near the cliff); 600 (dies 0/5).
constexpr float W_B2B_ACTIVE = 150.0f;

// One point of attack per line of Surge the chain holds: the charge is garbage the chain
// already owns, so breaking it must not read as free money and holding it is not a loss.
// Retuned in the table at the top of this file.
constexpr float W_B2B_CHARGE = 100.0f;

} // namespace tb
