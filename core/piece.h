#pragma once
#include "core/types.h"

namespace tb {

// 4 cells per (piece, rotation). Offsets are (dx, dy) from the piece origin,
// dy increasing upward to match Board's row 0 = bottom.
//
// ORIGIN CONVENTION -- everything in this project depends on it:
//   The origin (x, y) is the LOWER-LEFT CORNER OF THE PIECE'S BOUNDING BOX.
//   Box size is 3x3 for J/L/S/T/Z, 4x4 for I, 2x2 for O.
//
// This is the convention the SRS kick tables in core/srs_tables.h are written
// in, so a kick offset is added straight to (x, y) with no conversion and no
// sign flip.
//
// Consequence worth knowing: J/L/S/T/Z in ROT_0 have no cells in their box's
// bottom row (every offset has dy >= 1), so y = -1 is a legal non-colliding
// position for them. The 180 floor-kick test relies on this.
//
// The T-spin corner test (a later milestone) is conventionally written against
// the T's CENTER MINO. The conversion is a constant: center = origin + (1, 1),
// so the T's four diagonal corner cells are origin + (0,0), (2,0), (0,2), (2,2).
struct Cell { int8_t dx, dy; };

// Returns a pointer to 4 Cells. `p` must be a real piece (0..6), not PIECE_NONE.
const Cell* pieceCells(PieceType p, Rot r);

} // namespace tb
