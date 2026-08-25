#pragma once
#include "core/types.h"

namespace tb {

bool collides(const Board& b, PieceType p, Rot r, int x, int y);

void lockPiece(Board& b, PieceType p, Rot r, int x, int y);

int clearLines(Board& b);

int dropY(const Board& b, PieceType p, Rot r, int x, int y);

void addGarbage(Board& b, int lines, int holeCol);

int columnHeight(const Board& b, int col);

bool isEmpty(const Board& b);

Board boardFromAscii(const char* const* rows, int nRows);

}
