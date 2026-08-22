// core/movegen.cpp -- BFS move generation and T-spin classification (PRD 4.4, 4.6).
#include "core/movegen.h"

#include <cstring>

#include "core/piece.h"
#include "core/srs.h"

namespace tb {

// The T's centre mino, as an offset from the piece origin. The origin is the
// bounding box's lower-left corner and the centre is the box's middle cell, so
// this is (1, 1). Task 1's test_mg_t_center_is_origin_plus_one_one asserts it.
constexpr int T_CENTER_DX = 1;
constexpr int T_CENTER_DY = 1;

bool mgCellOccupied(const Board& b, int x, int y) {
    if (x < 0 || x >= BOARD_W) return true;    // walls
    if (y < 0) return true;                    // floor
    if (y >= BOARD_H) return false;            // above the well is empty
    return ((b.rows[y] >> x) & 1u) != 0u;
}

SpinKind classifyTSpin(const Board& b, Rot r, int x, int y,
                       bool lastWasRotation, uint8_t kickIndex) {
    // 1. last successful action must have been a rotation
    if (!lastWasRotation) return SPIN_NONE;

    const int cx = x + T_CENTER_DX;
    const int cy = y + T_CENTER_DY;

    // The four cells diagonally adjacent to the centre. Same four in every
    // rotation; rotation only decides which two are "front".
    //   index 0 = (-1,+1)  1 = (+1,+1)  2 = (-1,-1)  3 = (+1,-1)
    static const int8_t CORNER_DX[4] = { -1, +1, -1, +1 };
    static const int8_t CORNER_DY[4] = { +1, +1, -1, -1 };
    // Front pair per rotation: the two corners on the nub's side.
    static const uint8_t FRONT[4][2] = {
        { 0, 1 },   // ROT_0, nub up    -> (-1,+1) (+1,+1)
        { 1, 3 },   // ROT_R, nub right -> (+1,+1) (+1,-1)
        { 2, 3 },   // ROT_2, nub down  -> (-1,-1) (+1,-1)
        { 0, 2 },   // ROT_L, nub left  -> (-1,+1) (-1,-1)
    };

    bool filled[4];
    int occupied = 0;
    for (int i = 0; i < 4; ++i) {
        filled[i] = mgCellOccupied(b, cx + CORNER_DX[i], cy + CORNER_DY[i]);
        if (filled[i]) ++occupied;
    }

    // 2. HARD GATE. Fewer than three corners is not a mini, it is nothing.
    if (occupied < 3) return SPIN_NONE;

    // 3. Both front corners -> proper T-spin. (With occupied >= 3, front is
    //    always 1 or 2, so this and the fall-through are exhaustive.)
    const int ri = static_cast<int>(r) & 3;
    const int front = (filled[FRONT[ri][0]] ? 1 : 0) + (filled[FRONT[ri][1]] ? 1 : 0);
    if (front == 2) return SPIN_FULL;

    // 4. The T-Spin Triple wall-kick upgrade: the last of the five SRS tests is
    //    always a "1 by 2" displacement, and reaching a slot with it is a proper
    //    spin regardless of the corner count. kickIndex 255 (translation, or a
    //    180 rotation) never promotes.
    if (kickIndex == 4) return SPIN_FULL;

    // 5.
    return SPIN_MINI;
}

} // namespace tb
