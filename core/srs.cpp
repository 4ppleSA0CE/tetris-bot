#include "core/srs.h"

#include <cassert>
#include <iterator>

#include "core/board.h"
#include "core/srs_tables.h"

namespace tb {
namespace {

constexpr int8_t TRANSITION_INDEX[4][4] = {

    {     -1,   0,  -1,   7 },
    {      1,  -1,   2,  -1 },
    {     -1,   3,  -1,   4 },
    {      6,  -1,   5,  -1 },
};

}

bool tryRotate(const Board& b, PieceType p, Rot from, Rot to,
               int x, int y, int* outX, int* outY, uint8_t* outKickIndex) {
    if (from == to) return false;

    if (p == PIECE_O) {
        if (collides(b, p, to, x, y)) return false;
        *outX = x;
        *outY = y;
        *outKickIndex = 0;
        return true;
    }

    assert(from >= 0 && from < 4);
    assert(to >= 0 && to < 4);

    const int idx = TRANSITION_INDEX[static_cast<int>(from)][static_cast<int>(to)];
    if (idx < 0) {

        const int row = static_cast<int>(from);

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
    return false;
}

}
