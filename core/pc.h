#pragma once
#include "core/types.h"
#include "core/movegen.h"
#include "core/search.h"

namespace tb {

bool pcRegionsOk(const Board& b, int height);

bool pcFillableOk(const Board& b, int height);

bool pcParityOk(const Board& b, int height, PieceType current, PieceType hold,
                const PieceType* queue, int queueLen);

bool pcNextPiece(const Board& b, PieceType p);

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

// on false, out is untouched except nodes (solver cost when it ran)
bool pcPlan(const Board& b, PieceType current, PieceType hold,
            const PieceType* queue, int queueLen, uint8_t bagMask,
            int pendingGarbage, const SearchConfig& cfg, SearchResult* out);

}
