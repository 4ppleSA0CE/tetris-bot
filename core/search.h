#pragma once
#include "core/types.h"
#include "core/eval.h"
#include "core/movegen.h"

namespace tb {

struct SearchConfig {
    int   depth      = 5;
    int   beamWidth  = 100;
    float gamma      = 0.95f;
    // 4.8, not 5.0. PRD 4.5 requires the search to COMPLETE within 5 ms, and the loop
    // breaks when `elapsed > timeBudgetMs` -- i.e. always just AFTER the budget, never
    // before. Setting this to 5.0 therefore guarantees p99 lands above 5.0: measured
    // p99 = 5.045 ms with a 5.0 budget, which fails the requirement by construction
    // rather than by being slow. Aiming 0.2 ms low absorbs the overshoot so real
    // completion time lands under the limit.
    float timeBudgetMs = 4.5f;
    // Deterministic stand-in for the clock: stop after this many scored children (0 = off).
    // Same horizon as the time budget when calibrated to it, but identical on every run and
    // on every core, which is what tools/tune.py needs. Shipped play never sets it.
    long  nodeBudget   = 0;
    // Diagnostics only: count duplicate states inside each surviving beam (SearchResult.dupes).
    // Off by default; costs a hash of every beam entry per level when on.
    bool  measureDupes = false;
    Weights weights  = defaultWeights();
};

struct SearchResult {
    Placement placement;
    bool      useHold;    // true => swap hold before applying `placement`
    float     score;
    bool      valid;      // false only if no legal placement exists (top-out)
    long      nodes;      // children scored before the answer was returned
    long      dupes;      // duplicate beam entries seen (0 unless cfg.measureDupes)
    long      beamSlots;  // beam entries inspected for dupes (denominator for the fraction)
};

// Beam search over the preview queue with hold branching (PRD 4.5).
//
// `queue` holds the next `queueLen` pieces after `current`; `hold` may be PIECE_NONE.
// `b2bCount` and `comboCount` are the state BEFORE this piece is placed.
//
// The answer is the best ROOT of the deepest level that ran to completion -- scores at
// different depths are on different scales and are never compared against each other.
//
// ANYTIME FROM THE START: the clock is checked at every depth boundary and every 64 scored
// children. An overrun returns the deepest completed level's winner; if depth 0 itself was
// interrupted, its best partial result is returned instead, so there is always a legal move.
// Consequence: results are wall-clock dependent whenever the budget is actually reached. For
// bit-identical replay set timeBudgetMs to something the search cannot reach (1e9f).
// `incoming` = garbage lines pending against the searcher. Attack earned on a path cancels
// them first (exactly Game's rule); whatever remains is charged through evaluate()'s
// pendingRise, so digging under pressure and cancelling both pay off. 0 = solo play.
SearchResult search(const Board& b, PieceType current, PieceType hold,
                    const PieceType* queue, int queueLen,
                    int b2bCount, int comboCount, const SearchConfig& cfg,
                    int incoming = 0);

} // namespace tb
