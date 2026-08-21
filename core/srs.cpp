#include "core/srs.h"

#include "core/board.h"
#include "core/srs_tables.h"

namespace tb {
namespace {

// Maps (from, to) onto the first dimension of KICKS_JLSTZ / KICKS_I:
//   [0] = 0->R  [1] = R->0  [2] = R->2  [3] = 2->R
//   [4] = 2->L  [5] = L->2  [6] = L->0  [7] = 0->L
// -1 means "not a 90-degree turn": either from == to, or a 180.
constexpr int8_t TRANSITION_INDEX[4][4] = {
    //  to: 0    R    2    L
    {     -1,   0,  -1,   7 },   // from 0
    {      1,  -1,   2,  -1 },   // from R
    {     -1,   3,  -1,   4 },   // from 2
    {      6,  -1,   5,  -1 },   // from L
};

} // namespace

bool tryRotate(const Board& b, PieceType p, Rot from, Rot to,
               int x, int y, int* outX, int* outY, uint8_t* outKickIndex) {
    if (from == to) return false;

    // O never kicks: all four of its rotation states are the same four cells,
    // so the turn is a no-op that succeeds in place. It is routed around both
    // kick tables deliberately.
    if (p == PIECE_O) {
        if (collides(b, p, to, x, y)) return false;
        *outX = x;
        *outY = y;
        *outKickIndex = 0;
        return true;
    }

    const int idx = TRANSITION_INDEX[static_cast<int>(from)][static_cast<int>(to)];
    if (idx < 0) return false;   // 180 transitions are added in Task 8

    const Kick* tests = (p == PIECE_I) ? KICKS_I[idx] : KICKS_JLSTZ[idx];
    for (int k = 0; k < 5; ++k) {
        const int nx = x + tests[k].dx;
        const int ny = y + tests[k].dy;
        if (!collides(b, p, to, nx, ny)) {
            *outX = nx;
            *outY = ny;
            *outKickIndex = static_cast<uint8_t>(k);
            return true;
        }
    }
    return false;   // all tests collided -- rotation refused
}

} // namespace tb
