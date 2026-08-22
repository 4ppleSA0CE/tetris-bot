#pragma once
#include <cstdint>
#include <cstddef>

namespace tb {

// Contract: bindings/snapshot.h (PRD section 5.1, AMENDED).
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

// Written into by BotInstance, read by TypeScript through zero-copy views.
// FIELD ORDER IS THE CONTRACT. Do not reorder, do not insert, do not pack.
// Natural alignment already packs this to 156 bytes; #pragma pack would only
// make the DataView reads unaligned and slower on wasm32.
struct Snapshot {
    uint32_t frame;
    uint16_t rows[40];       // board bitmask, row 0 = bottom, bit i = column i
    int8_t   activePiece;    // 0-6, -1 = none
    int8_t   activeRot;      // 0-3
    int8_t   activeX;
    int8_t   activeY;
    int8_t   ghostY;
    uint8_t  pendingSpin;    // 0 none, 1 mini, 2 full - known before the piece locks
    // 0-255 along the current movement path. EXPORTED FOR HOSTS, and deliberately
    // not read by renderers/canvas-mono.ts: the tempo dilation of PRD 7.2 happens
    // in C++ by stretching how long each path state is displayed, and the renderer
    // sub-interpolates on its own frame clock. A host that wants to drive its own
    // effect off the placement's progress reads this; the shipped renderer does not.
    uint8_t  pathProgress;
    int8_t   holdPiece;      // -1 = empty
    int8_t   queue[5];
    uint8_t  eventCount;     // events written this tick
    Event    events[8];
    uint32_t piecesPlaced;
    uint32_t linesCleared;
    uint32_t attackSent;
    uint16_t b2bCount;
    uint16_t comboCount;
    float    pps;            // measured, not configured
    uint8_t  state;          // 0 idle, 1 playing, 2 topped out
};

constexpr int SNAPSHOT_MAX_EVENTS = 8;

// Tripwires. TypeScript never depends on these numbers - it reads the layout at
// runtime - but an accidental layout change should be loud at compile time.
static_assert(sizeof(Event) == 4,               "Event layout changed");
static_assert(alignof(Event) == 2,              "Event alignment changed");
static_assert(sizeof(Snapshot) == 156,          "Snapshot layout changed");
static_assert(alignof(Snapshot) == 4,           "Snapshot alignment changed");
static_assert(offsetof(Snapshot, rows) == 4,    "Snapshot.rows moved");
static_assert(offsetof(Snapshot, pendingSpin) == 89,  "Snapshot.pendingSpin moved");
static_assert(offsetof(Snapshot, pathProgress) == 90, "Snapshot.pathProgress moved");
static_assert(offsetof(Snapshot, events) == 98, "Snapshot.events moved");
static_assert(offsetof(Snapshot, state) == 152, "Snapshot.state moved");

} // namespace tb
