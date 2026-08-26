#pragma once
#include "core/types.h"
#include "core/eval.h"
#include "core/movegen.h"

namespace tb {

struct PcConfig {
    bool  enabled    = true;
    float threshold  = 0.6f;
    long  nodeBudget = 200000;
};

struct SearchConfig {

    int   depth      = 7;
    int   beamWidth  = 30;
    float gamma      = 0.95f;

    float timeBudgetMs = 4.5f;

    long  nodeBudget   = 0;

    bool  measureDupes = false;
    Weights weights  = defaultWeights();
    PcConfig pc;
};

struct SearchResult {
    Placement placement;
    bool      useHold;
    float     score;
    bool      valid;
    long      nodes;
    long      dupes;
    long      beamSlots;
};

SearchResult search(const Board& b, PieceType current, PieceType hold,
                    const PieceType* queue, int queueLen,
                    int b2bCount, int comboCount, const SearchConfig& cfg,
                    int incoming = 0);

}
