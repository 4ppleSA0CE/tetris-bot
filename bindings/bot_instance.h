#pragma once
#include <cstdint>

#include "bindings/snapshot.h"
#include "core/types.h"
#include "core/rng.h"
#include "core/eval.h"
#include "core/movegen.h"
#include "core/search.h"

namespace tb {

float* bindingsWeightSlot(Weights& w, int index);

class BotInstance {
public:
    BotInstance(uint32_t seed, float pps, int searchDepth, int beamWidth);

    void tick(double nowMs);
    void setPPS(float pps);
    void setWeight(int index, float value);

    void setTimeBudget(float ms);
    void setPcConfig(const PcConfig& pc);
    void setNodeBudget(long n);
    void reset(uint32_t seed);

    void queueGarbage(int lines);

    const Snapshot* snapshotPtr() const { return &snap_; }
    uint32_t seed() const { return seed_; }

private:
    void plan();
    void buildPathStates();
    void recomputeTempo();
    void lockCurrent();
    void writeActive(double nowMs);
    void writeStats();
    void pushEvent(uint8_t type, uint8_t param);
    void topOut();

    Bag          bag_;
    Board        board_{};

    uint8_t      cellPiece_[BOARD_CELLS]{};
    SearchConfig cfg_{};
    Snapshot     snap_{};

    uint32_t seed_;
    float    pps_;

    PieceType current_ = PIECE_NONE;
    PieceType hold_    = PIECE_NONE;
    PieceType queue_[PREVIEW_LEN]{};

    int      comboCount_  = 0;
    uint16_t b2bCount_    = 0;
    uint32_t piecesPlaced_ = 0;
    uint32_t linesCleared_ = 0;
    uint32_t attackSent_   = 0;
    int      pendingGarbage_ = 0;
    uint32_t garbageRng_     = 0;

    PieceType prePlanHold_    = PIECE_NONE;
    PieceType prePlanCurrent_ = PIECE_NONE;
    PieceType prePlanQueue_[PREVIEW_LEN]{};
    Bag       prePlanBag_{0};
    bool     toppedOut_    = false;

    Placement plan_{};
    bool      planValid_   = false;
    int       plannedLines_ = 0;
    int8_t    pathX_[MAX_PATH_LEN + 1]{};
    int8_t    pathY_[MAX_PATH_LEN + 1]{};
    int8_t    pathR_[MAX_PATH_LEN + 1]{};
    int8_t    pathFloor_[MAX_PATH_LEN + 1]{};
    int       pathSteps_ = 0;

    double pieceMs_       = 0.0;
    double pieceStartMs_  = 0.0;
    double lastNowMs_     = 0.0;
    bool   started_       = false;

    double   ppsWindowStartMs_ = 0.0;
    uint32_t ppsWindowPieces_  = 0;
};

}
