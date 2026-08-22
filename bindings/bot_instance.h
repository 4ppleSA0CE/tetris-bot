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

// --- animation tempo (PRD section 7.2) -------------------------------------
// The last TAIL_FRACTION of every placement's path is the "interesting" part.
// Normally it takes its proportional share of the piece budget; when the
// placement is a spin it is stretched to DILATION_MS of wall clock instead.
constexpr float TAIL_FRACTION = 0.25f;
constexpr float DILATION_MS   = 200.0f;

// PRD section 13 asks whether tempo dilation should cover tetrises as well as
// spins, and says to decide it by eye. SHIPPED VALUE: false, spins only.
//
// The mechanical half of that question is settled and needs no further work:
// recomputeTempo() already reads this, plannedLines_ is already computed before
// the tempo is set, and tests/animation.mjs measures the dilated and undilated
// paths separately - so flipping it is a one-line change with no follow-on edits.
//
// The aesthetic half is genuinely a judgement and is deliberately NOT recorded
// here as decided. To settle it:
//     sed -i '' 's/DILATE_TETRIS = false/DILATE_TETRIS = true/' bindings/bot_instance.h
//     ./build.sh && npx vite --config demo/vite.config.ts
// then watch one tetris land at 5 PPS and one at 15 PPS and keep whichever reads
// better. The argument for leaving it false is that dilation exists to make a spin
// stand out from everything around it, and dilating the tetris too spends that
// contrast; the argument for true is that a four-row clear is the biggest thing
// that happens and currently goes by at routine speed.
constexpr bool  DILATE_TETRIS = false;

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
    void recomputeTempo();     // headMs_ / tailMs_ from pps_ and the pending spin
    void lockCurrent();        // apply plan_, fire events, advance the queue
    void writeActive(double nowMs);
    void writeStats();
    void pushEvent(uint8_t type, uint8_t param);
    void topOut();

    Bag          bag_;
    Board        board_{};
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

    double headMs_        = 0.0;
    double tailMs_        = 0.0;
    double pieceStartMs_  = 0.0;
    double lastNowMs_     = 0.0;
    bool   started_       = false;

    double   ppsWindowStartMs_ = 0.0;
    uint32_t ppsWindowPieces_  = 0;
};

} // namespace tb
