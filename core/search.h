#pragma once
#include "core/types.h"
#include "core/eval.h"
#include "core/movegen.h"

namespace tb {

struct SearchConfig {
    int   depth      = 5;
    int   beamWidth  = 100;
    float gamma      = 0.95f;
    float timeBudgetMs = 5.0f;
    Weights weights  = defaultWeights();
};

struct SearchResult {
    Placement placement;
    bool      useHold;    // true => swap hold before applying `placement`
    float     score;
    bool      valid;      // false only if no legal placement exists (top-out)
};

// Beam search over the preview queue with hold branching (PRD 4.5).
//
// `queue` holds the next `queueLen` pieces after `current`; `hold` may be PIECE_NONE.
// `b2bActive` and `comboCount` are the state BEFORE this piece is placed.
//
// The answer is the best ROOT of the deepest level that ran to completion -- scores at
// different depths are on different scales and are never compared against each other.
//
// ANYTIME FROM THE START: the clock is checked at every depth boundary and every 64 scored
// children. An overrun returns the deepest completed level's winner; if depth 0 itself was
// interrupted, its best partial result is returned instead, so there is always a legal move.
// Consequence: results are wall-clock dependent whenever the budget is actually reached. For
// bit-identical replay set timeBudgetMs to something the search cannot reach (1e9f).
SearchResult search(const Board& b, PieceType current, PieceType hold,
                    const PieceType* queue, int queueLen,
                    bool b2bActive, int comboCount, const SearchConfig& cfg);

} // namespace tb
