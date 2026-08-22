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

    for (int c = 0; c < BOARD_W; ++c) {
        bool sawEmpty = false;
        for (int y = 0; y < h[c]; ++y) {
            if (!occ(b, c, y))      sawEmpty = true;
            else if (sawEmpty)      ++f.coveredCells;
        }
    }

    for (int c = 0; c + 1 < BOARD_W; ++c) {
        int d = h[c] - h[c + 1];
        f.bumpiness += (d < 0) ? -d : d;
    }

    {
        int e = f.maxHeight - HEIGHT_PENALTY_THRESHOLD;
        if (e < 0) e = 0;
        f.heightPenalty = e * e;   // squared: this is the cliff above row 12, not a slope
    }

    for (int y = 0; y < f.maxHeight; ++y) {
        bool prev = true;                       // left wall counts as filled
        for (int x = 0; x < BOARD_W; ++x) {
            bool cur = occ(b, x, y);
            if (cur != prev) ++f.rowTransitions;
            prev = cur;
        }
        if (!prev) ++f.rowTransitions;          // right wall counts as filled
    }

    for (int c = 0; c < BOARD_W; ++c) {
        bool prev = true;                       // the floor counts as filled
        for (int y = 0; y < f.maxHeight; ++y) {
            bool cur = occ(b, c, y);
            if (cur != prev) ++f.columnTransitions;
            prev = cur;
        }
    }

    return f;
}

} // namespace tb
