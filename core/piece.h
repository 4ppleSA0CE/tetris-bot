#pragma once
#include "core/types.h"

namespace tb {

struct Cell { int8_t dx, dy; };

const Cell* pieceCells(PieceType p, Rot r);

}
