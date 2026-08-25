#pragma once
#include <cstdint>
#include <cstddef>

namespace tb {

constexpr int BOARD_CELLS = 40 * 10;

constexpr uint8_t CELL_EMPTY = 0xFF;

enum EventType : uint8_t {
    EV_PIECE_LOCK = 0, EV_LINE_CLEAR = 1, EV_TETRIS = 2,
    EV_TSPIN_MINI = 3, EV_TSPIN_SINGLE = 4, EV_TSPIN_DOUBLE = 5, EV_TSPIN_TRIPLE = 6,
    EV_B2B_EXTEND = 7, EV_B2B_BREAK = 8, EV_PERFECT_CLEAR = 9, EV_TOPOUT = 10,
};

struct Event {
    uint8_t  type;
    uint8_t  param;
    uint16_t frame;
};

struct Snapshot {
    uint32_t frame;
    uint16_t rows[40];
    int8_t   activePiece;
    int8_t   activeRot;
    int8_t   activeX;
    int8_t   activeY;
    int8_t   ghostY;
    uint8_t  pendingSpin;

    uint8_t  pathProgress;
    int8_t   holdPiece;
    int8_t   queue[5];
    uint8_t  eventCount;
    Event    events[8];
    uint32_t piecesPlaced;
    uint32_t linesCleared;
    uint32_t attackSent;
    uint16_t b2bCount;
    uint16_t comboCount;
    float    pps;
    uint8_t  state;

    uint8_t  cellPiece[BOARD_CELLS];

    uint8_t  pendingGarbage;
};

constexpr int SNAPSHOT_MAX_EVENTS = 8;

static_assert(sizeof(Event) == 4,               "Event layout changed");
static_assert(alignof(Event) == 2,              "Event alignment changed");
static_assert(sizeof(Snapshot) == 556,          "Snapshot layout changed");
static_assert(alignof(Snapshot) == 4,           "Snapshot alignment changed");
static_assert(offsetof(Snapshot, rows) == 4,    "Snapshot.rows moved");
static_assert(offsetof(Snapshot, pendingSpin) == 89,  "Snapshot.pendingSpin moved");
static_assert(offsetof(Snapshot, pathProgress) == 90, "Snapshot.pathProgress moved");
static_assert(offsetof(Snapshot, events) == 98, "Snapshot.events moved");
static_assert(offsetof(Snapshot, state) == 152, "Snapshot.state moved");
static_assert(offsetof(Snapshot, cellPiece) == 153, "Snapshot.cellPiece moved");
static_assert(offsetof(Snapshot, pendingGarbage) == 553, "Snapshot.pendingGarbage moved");

}
