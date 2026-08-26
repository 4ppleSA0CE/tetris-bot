#pragma once
#include "core/types.h"
#include "core/movegen.h"
#include "core/search.h"

namespace tb {

bool pcRegionsOk(const Board& b, int height);

struct PcResult {
    bool      valid;
    bool      aborted;
    float     prob;
    bool      useHold;
    int       height;
    long      nodes;
    Placement move;
};

PcResult pcSolveHeight(const Board& b, int height, PieceType current, PieceType hold,
                       const PieceType* queue, int queueLen, uint8_t bagMask,
                       const PcConfig& cfg);

PcResult pcSolve(const Board& b, PieceType current, PieceType hold,
                 const PieceType* queue, int queueLen, uint8_t bagMask,
                 const PcConfig& cfg);

bool pcPlan(const Board& b, PieceType current, PieceType hold,
            const PieceType* queue, int queueLen, uint8_t bagMask,
            int pendingGarbage, const SearchConfig& cfg, SearchResult* out);

}
