#pragma once
#include "core/types.h"

namespace tb {

// True if any cell of piece `p` in rotation `r` placed at origin (x, y) is out
// of bounds or overlaps a filled cell. Walls (x < 0, x >= BOARD_W), the floor
// (y < 0) and the ceiling (y >= BOARD_H) all count as occupied.
bool collides(const Board& b, PieceType p, Rot r, int x, int y);

// Sets the four bits of the piece. Caller must have checked collides() first.
void lockPiece(Board& b, PieceType p, Rot r, int x, int y);

// Removes every full row and shifts everything above it down by the number of
// rows removed below it (naive gravity -- never sticky, never cascade).
// Returns the number of rows removed.
int clearLines(Board& b);

// Lowest legal y at or below `y`. Assumes (x, y) itself does not collide.
int dropY(const Board& b, PieceType p, Rot r, int x, int y);

// One above the highest filled cell in `col`; 0 when the column is empty.
int columnHeight(const Board& b, int col);

// True when no cell anywhere is filled. Used for perfect-clear detection.
bool isEmpty(const Board& b);

// Tests only. '#' = filled, '.' = empty, rows[0] = TOP row, each string
// BOARD_W chars. rows[nRows-1] becomes board row y = 0.
Board boardFromAscii(const char* const* rows, int nRows);

} // namespace tb
