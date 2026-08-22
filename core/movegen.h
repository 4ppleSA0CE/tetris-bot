#pragma once
#include <cstdint>

#include "core/types.h"
#include "core/board.h"

namespace tb {

constexpr int MAX_PLACEMENTS = 256;
constexpr int MAX_PATH_LEN   = 64;

// PRD 4.4: the BFS is seeded here. Origin is the bounding box's lower-left
// corner, so a 3-wide piece at SPAWN_X occupies columns 4, 5 and 6.
constexpr int SPAWN_X = 4;
constexpr int SPAWN_Y = 21;

// ---------------------------------------------------------------------------
// BFS state space.
//
// A state is (x, y, rot). Every piece cell offset is in [0, 3], so a legal
// state satisfies 0 <= x + dx <= 9 and 0 <= y + dy <= 39, which forces
// x into [-3, 9] and y into [-3, 39]. The window below is that range plus one
// cell of slack: every legal state is inside it, and every state outside it is
// provably illegal. mgInStateBounds() is called BEFORE mgStateIndex() on every
// candidate, so negative and oversized coordinates never reach the array.
// ---------------------------------------------------------------------------
constexpr int MG_X_MIN = -3;
constexpr int MG_X_MAX = 10;
constexpr int MG_Y_MIN = -3;
constexpr int MG_Y_MAX = 40;
constexpr int MG_XS    = MG_X_MAX - MG_X_MIN + 1;   // 14
constexpr int MG_YS    = MG_Y_MAX - MG_Y_MIN + 1;   // 44
constexpr int MG_STATES = MG_XS * MG_YS * 4;        // 2464

inline bool mgInStateBounds(int x, int y) {
    return x >= MG_X_MIN && x <= MG_X_MAX && y >= MG_Y_MIN && y <= MG_Y_MAX;
}

// Caller MUST have checked mgInStateBounds(x, y) first.
inline int mgStateIndex(int x, int y, Rot r) {
    return (((y - MG_Y_MIN) * MG_XS) + (x - MG_X_MIN)) * 4 + static_cast<int>(r);
}

inline void mgDecodeState(int idx, int* x, int* y, Rot* r) {
    *r = static_cast<Rot>(idx & 3);
    const int cell = idx >> 2;
    *x = (cell % MG_XS) + MG_X_MIN;
    *y = (cell / MG_XS) + MG_Y_MIN;
}

inline Rot rotCW (Rot r) { return static_cast<Rot>((static_cast<int>(r) + 1) & 3); }
inline Rot rotCCW(Rot r) { return static_cast<Rot>((static_cast<int>(r) + 3) & 3); }
inline Rot rot180(Rot r) { return static_cast<Rot>((static_cast<int>(r) + 2) & 3); }

inline bool isRotateAction(Action a) {
    return a == ACT_CW || a == ACT_CCW || a == ACT_180;
}

// ---------------------------------------------------------------------------
// A locked placement, plus the exact action sequence that reaches it. The path
// is not optional: PRD 4.4 makes it what the renderer animates, so the piece
// performs real slides, rotations and kicks rather than teleporting.
// ---------------------------------------------------------------------------
struct Placement {
    int8_t   x, y;             // piece origin = bounding box lower-left corner
    Rot      rot;
    SpinKind spin;             // SPIN_NONE unless piece is T and 4.6 holds
    bool     lastWasRotation;
    // kickIndex: 0..4, the SRS kick-test index of the final 90-degree rotation.
    // 255 means "not a 90-degree rotation": either the last action was a
    // translation or a soft drop (lastWasRotation == false), OR the last action
    // was ACT_180 (lastWasRotation == true). 180 is deliberately excluded from
    // the index because the mini -> full promotion in classifyTSpin is defined
    // only for the five-test 90-degree kick tables; index 4 of the six-test 180
    // table is a completely different offset and must never promote.
    uint8_t  kickIndex;
    uint8_t  pathLen;
    Action   path[MAX_PATH_LEN];
};

struct MoveList {
    int       count;
    Placement items[MAX_PLACEMENTS];
};

// Generates every locked placement reachable from spawn via
// left / right / cw / ccw / 180 / soft-drop. ACT_180 is hardcoded on (Q&A
// amendment to PRD 4.2). Writes at most MAX_PLACEMENTS entries; out->count is
// 0 when the spawn state itself collides (top-out).
//
// NOT REENTRANT: uses one file-static scratch arena. Call it from one thread at
// a time and finish with the MoveList before calling again.
void generateMoves(const Board& b, PieceType p, MoveList* out);

// PRD 4.6 classification. (x, y) is the T's piece origin, NOT its centre mino.
// Assumes the piece is T; callers must not invoke it for anything else.
// Exposed for tests.
SpinKind classifyTSpin(const Board& b, Rot r, int x, int y,
                       bool lastWasRotation, uint8_t kickIndex);

// Walls (x < 0, x > 9) and the floor (y < 0) count as OCCUPIED.
// Cells above the well (y >= BOARD_H) count as EMPTY. This is modern-guideline
// behaviour and it is what makes the 3-corner test give the right answer at the
// edges. Exposed for tests.
bool mgCellOccupied(const Board& b, int x, int y);

} // namespace tb
