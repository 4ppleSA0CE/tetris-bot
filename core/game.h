#pragma once
#include "core/types.h"
#include "core/movegen.h"
#include "core/search.h"
#include "core/rng.h"

namespace tb {

enum GameEventType : uint8_t {
    GEV_PIECE_LOCK = 0, GEV_LINE_CLEAR = 1, GEV_TETRIS = 2,
    GEV_TSPIN_MINI = 3, GEV_TSPIN_SINGLE = 4, GEV_TSPIN_DOUBLE = 5, GEV_TSPIN_TRIPLE = 6,
    GEV_B2B_EXTEND = 7, GEV_B2B_BREAK = 8, GEV_PERFECT_CLEAR = 9, GEV_TOPOUT = 10,
};

constexpr int MAX_GAME_EVENTS = 8;

struct GameEvent {
    uint8_t type;
    uint8_t param;
};

class Game {
public:
    Game(uint32_t seed, const SearchConfig& cfg);
    void reset(uint32_t seed);
    void stepPiece();

    void     queueGarbage(int lines)        { pendingGarbage_ += lines; }
    void     setMessiness(float m)          { messiness_ = m; }
    uint32_t garbageReceived()        const { return garbageReceived_; }
    int      pendingGarbage()         const { return pendingGarbage_; }

    bool         toppedOut()    const { return toppedOut_; }
    const Board& board()        const { return board_; }
    uint32_t     piecesPlaced() const { return piecesPlaced_; }
    uint32_t     linesCleared() const { return linesCleared_; }
    uint32_t     attackSent()   const { return attackSent_; }
    uint32_t     surgeSent()    const { return surgeSent_; }
    uint32_t     pcCount()      const { return pcCount_; }
    uint16_t     b2bCount()     const { return b2bCount_; }
    uint16_t     comboCount()   const { return comboCount_; }

    uint16_t         maxB2b()        const { return maxB2b_; }
    uint32_t         tSpinCount()    const { return tSpinCount_; }
    float            lastSearchMs()  const { return lastSearchMs_; }
    long             lastSearchNodes() const { return lastSearchNodes_; }
    long             lastSearchDupes() const { return lastSearchDupes_; }
    long             lastSearchBeamSlots() const { return lastSearchBeamSlots_; }
    PieceType        currentPiece()  const { return current_; }
    PieceType        holdPiece()     const { return hold_; }
    const PieceType* queue()         const { return queue_; }
    const Placement& lastPlacement() const { return lastPlacement_; }
    bool             lastUsedHold()  const { return lastUsedHold_; }
    SpinKind         lastSpin()      const { return lastPlacement_.spin; }
    int              eventCount()    const { return eventCount_; }
    GameEvent        eventAt(int i)  const { return events_[i]; }
    void             clearEvents()         { eventCount_ = 0; }

private:
    void pushEvent(uint8_t type, uint8_t param);
    void shiftQueue();

    Board        board_;
    Bag          bag_;
    SearchConfig cfg_;
    PieceType    current_;
    PieceType    hold_;
    PieceType    queue_[PREVIEW_LEN];
    Placement    lastPlacement_;
    bool         lastUsedHold_;
    bool         toppedOut_;
    uint32_t     piecesPlaced_;
    uint32_t     linesCleared_;
    uint32_t     attackSent_;
    uint32_t     surgeSent_;
    uint32_t     tSpinCount_;
    uint32_t     pcCount_;
    uint16_t     b2bCount_;
    uint16_t     comboCount_;
    uint16_t     maxB2b_;
    float        lastSearchMs_;
    long         lastSearchNodes_ = 0;
    long         lastSearchDupes_ = 0;
    long         lastSearchBeamSlots_ = 0;
    GameEvent    events_[MAX_GAME_EVENTS];
    int          eventCount_;
    int          pendingGarbage_;
    uint32_t     garbageReceived_;
    uint32_t     garbageRng_;
    float        messiness_ = 0.05f;
};

}
