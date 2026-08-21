#include "core/piece.h"

namespace tb {
namespace {

// [piece][rotation][cell]. Piece order follows PieceType: I J L O S T Z.
// Rotation order follows Rot: 0 R 2 L, where R is one clockwise turn from spawn.
// Coordinates are (dx, dy) inside the piece's bounding box, origin = lower-left,
// dy up. Verified: rotating state r by (cx, cy) -> (cy, n-1-cx) yields state r+1.
constexpr Cell CELLS[NUM_PIECES][4][4] = {
    // PIECE_I -- 4x4 box
    {
        {{0, 2}, {1, 2}, {2, 2}, {3, 2}},   // ROT_0
        {{2, 3}, {2, 2}, {2, 1}, {2, 0}},   // ROT_R
        {{0, 1}, {1, 1}, {2, 1}, {3, 1}},   // ROT_2
        {{1, 3}, {1, 2}, {1, 1}, {1, 0}},   // ROT_L
    },
    // PIECE_J -- 3x3 box
    {
        {{0, 2}, {0, 1}, {1, 1}, {2, 1}},   // ROT_0
        {{1, 2}, {2, 2}, {1, 1}, {1, 0}},   // ROT_R
        {{0, 1}, {1, 1}, {2, 1}, {2, 0}},   // ROT_2
        {{1, 2}, {1, 1}, {0, 0}, {1, 0}},   // ROT_L
    },
    // PIECE_L -- 3x3 box
    {
        {{2, 2}, {0, 1}, {1, 1}, {2, 1}},   // ROT_0
        {{1, 2}, {1, 1}, {1, 0}, {2, 0}},   // ROT_R
        {{0, 1}, {1, 1}, {2, 1}, {0, 0}},   // ROT_2
        {{0, 2}, {1, 2}, {1, 1}, {1, 0}},   // ROT_L
    },
    // PIECE_O -- 2x2 box, identical in all four states, never kicks
    {
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},   // ROT_0
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},   // ROT_R
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},   // ROT_2
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},   // ROT_L
    },
    // PIECE_S -- 3x3 box
    {
        {{1, 2}, {2, 2}, {0, 1}, {1, 1}},   // ROT_0
        {{1, 2}, {1, 1}, {2, 1}, {2, 0}},   // ROT_R
        {{1, 1}, {2, 1}, {0, 0}, {1, 0}},   // ROT_2
        {{0, 2}, {0, 1}, {1, 1}, {1, 0}},   // ROT_L
    },
    // PIECE_T -- 3x3 box
    {
        {{1, 2}, {0, 1}, {1, 1}, {2, 1}},   // ROT_0
        {{1, 2}, {1, 1}, {2, 1}, {1, 0}},   // ROT_R
        {{0, 1}, {1, 1}, {2, 1}, {1, 0}},   // ROT_2
        {{1, 2}, {0, 1}, {1, 1}, {1, 0}},   // ROT_L
    },
    // PIECE_Z -- 3x3 box
    {
        {{0, 2}, {1, 2}, {1, 1}, {2, 1}},   // ROT_0
        {{2, 2}, {1, 1}, {2, 1}, {1, 0}},   // ROT_R
        {{0, 1}, {1, 1}, {1, 0}, {2, 0}},   // ROT_2
        {{1, 2}, {0, 1}, {1, 1}, {0, 0}},   // ROT_L
    },
};

} // namespace

const Cell* pieceCells(PieceType p, Rot r) {
    return CELLS[static_cast<int>(p)][static_cast<int>(r)];
}

} // namespace tb
