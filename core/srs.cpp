#include "core/srs.h"

#include <cassert>
#include <iterator>

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

    // `p` and `to` are validated downstream by collides() -> pieceCells(), but
    // `from` is only ever used as an array index -- here, and by KICKS_180[from]
    // for 180s -- so nothing else would catch it being out of range.
    assert(from >= 0 && from < 4);
    assert(to >= 0 && to < 4);

    const int idx = TRANSITION_INDEX[static_cast<int>(from)][static_cast<int>(to)];
    if (idx < 0) {
        // A 180. KICKS_180 rows are ordered 0->2, R->L, 2->0, L->R, which is
        // exactly the numeric value of `from`. J/L/S/T/Z get 6 tests; I gets a
        // single {0,0} test, i.e. the I piece does not kick on a 180.
        //
        // Index by `from`, NOT by `idx` -- `idx` is in scope and is -1 here.
        const int row = static_cast<int>(from);

        // Pointer and count are taken in ONE branch so they cannot drift apart.
        // KICKS_180 is [4][6] and KICKS_180_I is [4][1]; once either decays to a
        // Kick* the width is unrecoverable, so writing the literal 6 for both --
        // or copying the hardcoded 5 from the 90-degree loop below -- is a
        // silent out-of-bounds read that compiles clean and feeds garbage into
        // the recorded kick index, which milestone 2 uses for T-spin promotion.
        const Kick* tests180 = nullptr;
        int count180 = 0;
        if (p == PIECE_I) {
            tests180 = KICKS_180_I[row];
            count180 = static_cast<int>(std::size(KICKS_180_I[row]));
        } else {
            tests180 = KICKS_180[row];
            count180 = static_cast<int>(std::size(KICKS_180[row]));
        }

        for (int k = 0; k < count180; ++k) {
            const int nx = x + tests180[k].dx;
            const int ny = y + tests180[k].dy;
            if (!collides(b, p, to, nx, ny)) {
                *outX = nx;
                *outY = ny;
                *outKickIndex = static_cast<uint8_t>(k);
                return true;
            }
        }
        return false;
    }

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
