#pragma once

namespace tb {

// ============================================================================
// Starting weight vector for the evaluator (PRD 4.8).
//
// EVERY WEIGHT CARRIES ITS OWN SIGN. Board-health weights are negative, attack-posture
// weights are positive, and evaluate() is a plain dot product. If you "fix" a weight by
// flipping it positive because the feature is bad, you have inverted the evaluator.
//
// UNIT: W_ATTACK_DEALT is the anchor. One point of attack (one garbage line in the
// phantom versus match of PRD 1.1) is worth 100 evaluator points. Every other number here
// is therefore denominated in hundredths of a garbage line, which is what makes them
// comparable by eye.
//
// Tuning procedure that produced / re-tunes these: docs/superpowers/plans (Task 20).
// Override any of them at runtime without a rebuild:
//     ./build/tetris_bot --weights tSlotCount=240,maxHeight=-14
// ============================================================================

// Height above which heightPenalty starts to bite (PRD 4.8, shared contract core/eval.h).
constexpr int HEIGHT_PENALTY_THRESHOLD = 12;

// The unit. Sweep 60 / 80 / 100 / 140 / 180 in tuning step 4.
// Watch: the `attack` and `t-spins` lines. Raise it if the bot clears singles for tidiness;
// lower it if it dives for damage and the `max height` line climbs past 16.
constexpr float W_ATTACK_DEALT = 100.0f;

// One hole costs 2.2 attack. Tuned against: the bot must refuse a T-spin single (2 attack)
// that buries a cell, but accept a T-spin double (4 + 1 B2B) that does.
// Watch: the `lines` line. If lines-per-piece collapses below 0.25, this is too heavy.
constexpr float W_HOLES = -220.0f;

// Burial depth, not hole count. A hole under six cells costs an extra 0.72 attack on top of
// W_HOLES. Tuned against: the bot digging out a shallow hole instead of paving over it.
constexpr float W_COVERED_CELLS = -12.0f;

// DELIBERATELY SMALL. Bumpiness is the weight that flattens the stack, and a flat stack is a
// tuning FAILURE here even though it survives (PRD 1.1 wants the stack breathing 8-12 rows).
// Tuned against: `--heights` reporting `height sd` >= 2.0. If sd drops under 2.0, cut this first.
constexpr float W_BUMPINESS = -8.0f;

// Gentle on purpose: at height 10 this is only 1.2 attack. The stack is supposed to be
// allowed to climb to 12 before anything pushes back hard. W_HEIGHT_PENALTY does the pushing.
// Tuned against: `avg height` landing in [8, 12].
constexpr float W_MAX_HEIGHT = -12.0f;

// Applied to e*e where e = max(0, maxHeight - 12). At height 16 that is an extra 1.28 attack;
// at height 20 it is 5.12. This is the cliff. Tuned against: `top-outs` == 0 over 10000 pieces.
// Raise this, not W_MAX_HEIGHT, when the bot tops out -- it costs nothing below row 12.
constexpr float W_HEIGHT_PENALTY = -8.0f;

// Surface roughness. Absolute value is large (40-80 on a 10-high board), but only the
// DIFFERENCE between candidate placements matters, and that is 2-6, i.e. 0.1-0.4 attack.
// Tuned against: the bot not leaving isolated one-wide spikes.
constexpr float W_ROW_TRANSITIONS = -6.0f;

// Correlates with buried holes: a cheap second vote for what W_HOLES punishes. Tuned against:
// the bot preferring a clean overhang-free surface when the attack payoff is a wash.
constexpr float W_COLUMN_TRANSITIONS = -9.0f;

// SMALL ON PURPOSE. The bot NEEDS a 4-deep well to score tetrises; a heavy well weight is how
// bots lose their I-column and stop scoring. Depth-4 well = triangular 10 = 0.4 attack.
// Tuned against: tetrises still appearing in the `max b2b` chain. If the bot never clears 4
// lines at once, this is too heavy.
constexpr float W_WELL_DEPTH = -4.0f;

// A live T-slot is worth 1.8 attack. Reasoning: a T arrives roughly every 7 pieces from the
// bag, a slot survives ~3 pieces, so the expected payoff is about 0.5 * (4 TSD + 1 B2B) = 2.5
// attack, discounted for the chance the stack forces the slot closed first.
// THIS IS THE FIRST KNOB TO RAISE IF THE BOT NEVER T-SPINS (PRD 4.8 tuning heuristic).
// Tuned against: the `t-spins` per-100 rate >= 1.00. Sweep 120 / 180 / 240 / 300 / 400.
//
// KNOWN GAP: countTSlots() recognises TSD-shaped slots only (ROT_2 T, nub down). T-spin-triple
// slots and side-entry ROT_R / ROT_L slots score zero, so this weight cannot pay the bot to
// keep them open. Past 400 the answer is a second detector in countTSlots(), not more weight.
constexpr float W_T_SLOT_COUNT = 180.0f;

// Holding the chain is worth 0.6 attack: the next B2B-maintaining clear collects +1 attack,
// at maybe 60% likelihood inside the depth-5 horizon.
// Tuned against: `max b2b` >= 5 over 1000 pieces (PRD 11). Sweep 40 / 60 / 90 / 120.
constexpr float W_B2B_ACTIVE = 60.0f;

} // namespace tb
