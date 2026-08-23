#pragma once
#include <cstdint>

#include "bindings/snapshot.h"
#include "core/types.h"
#include "core/rng.h"
#include "core/eval.h"
#include "core/movegen.h"
#include "core/search.h"

namespace tb {

// --- weight sweeping -------------------------------------------------------
// The names, the count, and the order all live in core/eval.h:
//     int         weightNameCount();
//     const char* weightName(int i);                 // "" when out of range
//     bool        setWeightByName(Weights&, const char*, float);
// DO NOT redeclare any of those here - core/eval.cpp already defines them and a
// second definition is a duplicate symbol in tb_tests and in dist/bot.js.
//
// The one thing core/eval.h does not offer is a READ by index, which
// getWeightsInfo() needs for the "default" field it exports to JS. That, and
// only that, lives here. Indices are core/eval.h's indices; tests/tests.cpp
// pins the two together.
float* bindingsWeightSlot(Weights& w, int index);   // nullptr when out of range

// --- pacing -----------------------------------------------------------------
// Every placement occupies exactly one piece interval, 1000/pps ms, and the piece
// is stepped along its BFS path linearly across it. There is deliberately no spin
// dilation and no easing: the reference handling this build follows (jstris /
// TETR.IO, and the portfolio's own engine) paces every placement identically, and
// warping the clock for spins is precisely the animation this build does not do.

class BotInstance {
public:
    BotInstance(uint32_t seed, float pps, int searchDepth, int beamWidth);

    void tick(double nowMs);
    void setPPS(float pps);
    void setWeight(int index, float value);
    // Not exposed through embind - BotConfig has no budget field and the browser
    // must keep the shipped 5 ms. This exists so tests can make the anytime search
    // deterministic: search() returns best-so-far the moment it crosses the budget,
    // so ANY wall-clock hiccup changes its answer. A run that must be reproducible
    // sets this high enough that the budget is never reached.
    void setTimeBudget(float ms);
    void reset(uint32_t seed);

    const Snapshot* snapshotPtr() const { return &snap_; }
    uint32_t seed() const { return seed_; }

private:
    void plan();               // search, install the next placement, replay its path
    void buildPathStates();    // fill pathX_/pathY_/pathR_ by replaying plan_.path
    void recomputeTempo();     // pieceMs_ from pps_
    void lockCurrent();        // apply plan_, fire events, advance the queue
    void writeActive(double nowMs);
    void writeStats();
    void pushEvent(uint8_t type, uint8_t param);
    void topOut();

    Bag          bag_;
    Board        board_{};
    // Parallel to board_: which piece locked into each cell. See snapshot.h.
    uint8_t      cellPiece_[BOARD_CELLS]{};
    SearchConfig cfg_{};
    Snapshot     snap_{};

    uint32_t seed_;
    float    pps_;

    PieceType current_ = PIECE_NONE;
    PieceType hold_    = PIECE_NONE;
    PieceType queue_[PREVIEW_LEN]{};

    bool     b2bActive_   = false;
    int      comboCount_  = 0;
    uint16_t b2bCount_    = 0;
    uint32_t piecesPlaced_ = 0;
    uint32_t linesCleared_ = 0;
    uint32_t attackSent_   = 0;
    bool     toppedOut_    = false;

    Placement plan_{};
    bool      planValid_   = false;
    int       plannedLines_ = 0;
    int8_t    pathX_[MAX_PATH_LEN + 1]{};
    int8_t    pathY_[MAX_PATH_LEN + 1]{};
    int8_t    pathR_[MAX_PATH_LEN + 1]{};
    int       pathSteps_ = 0;

    double pieceMs_       = 0.0;
    double pieceStartMs_  = 0.0;
    double lastNowMs_     = 0.0;
    bool   started_       = false;

    double   ppsWindowStartMs_ = 0.0;
    uint32_t ppsWindowPieces_  = 0;
};

} // namespace tb
