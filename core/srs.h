#pragma once
#include <cstdint>

#include "core/types.h"

namespace tb {

// Attempts to rotate piece `p` at origin (x, y) from rotation `from` to `to`.
// Returns true on success and writes the accepted position + kick index.
//
// kickIndex is the index of the offset that succeeded: 0..4 for a 90-degree
// turn, 0..5 for a 180 of J/L/S/T/Z, always 0 for I on a 180 and for O.
// Index 4 of a 90-degree turn is the "1 by 2" kick that promotes a T-spin mini
// to a full T-spin, so it must be recorded on every successful rotation.
//
// On failure nothing is written through outX / outY / outKickIndex and the
// caller keeps its original (x, y, rotation). `from == to` is a failure.
bool tryRotate(const Board& b, PieceType p, Rot from, Rot to,
               int x, int y, int* outX, int* outY, uint8_t* outKickIndex);

} // namespace tb
