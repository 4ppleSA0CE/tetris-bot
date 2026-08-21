#include "core/board.h"

#include "core/piece.h"

namespace tb {

bool collides(const Board& b, PieceType p, Rot r, int x, int y) {
    const Cell* cells = pieceCells(p, r);
    for (int i = 0; i < 4; ++i) {
        const int cx = x + cells[i].dx;
        const int cy = y + cells[i].dy;
        if (cx < 0 || cx >= BOARD_W) return true;   // left / right wall
        if (cy < 0 || cy >= BOARD_H) return true;   // floor / ceiling
        if (b.rows[cy] & static_cast<uint16_t>(1u << cx)) return true;
    }
    return false;
}

Board boardFromAscii(const char* const* rows, int nRows) {
    Board b{};
    for (int i = 0; i < nRows; ++i) {
        const int y = nRows - 1 - i;
        if (y < 0 || y >= BOARD_H) continue;
        for (int x = 0; x < BOARD_W; ++x)
            if (rows[i][x] == '#') b.rows[y] |= static_cast<uint16_t>(1u << x);
    }
    return b;
}

} // namespace tb
