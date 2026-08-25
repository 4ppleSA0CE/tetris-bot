#pragma once
#include <cstdint>

#include "core/types.h"

namespace tb {

bool tryRotate(const Board& b, PieceType p, Rot from, Rot to,
               int x, int y, int* outX, int* outY, uint8_t* outKickIndex);

}
