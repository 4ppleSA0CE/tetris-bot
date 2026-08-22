#include "core/eval.h"
#include "core/board.h"

namespace tb {

namespace {

// Out of bounds sideways and below the floor count as FILLED; above the board counts as EMPTY.
inline bool occ(const Board& b, int x, int y) {
    if (x < 0 || x >= BOARD_W) return true;
    if (y < 0) return true;
    if (y >= BOARD_H) return false;
    return ((b.rows[y] >> x) & 1u) != 0u;
}

} // namespace

Features extractFeatures(const Board& b) {
    Features f{};

    int h[BOARD_W];
    for (int c = 0; c < BOARD_W; ++c) h[c] = columnHeight(b, c);

    for (int c = 0; c < BOARD_W; ++c) if (h[c] > f.maxHeight) f.maxHeight = h[c];

    for (int c = 0; c < BOARD_W; ++c) {
        for (int y = 0; y < h[c] - 1; ++y) {
            if (!occ(b, c, y)) ++f.holes;
        }
    }

    return f;
}

} // namespace tb
