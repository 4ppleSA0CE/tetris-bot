#include "core/board.h"

#include "core/piece.h"

#include <cassert>

namespace tb {

bool collides(const Board& b, PieceType p, Rot r, int x, int y) {
    const Cell* cells = pieceCells(p, r);
    for (int i = 0; i < 4; ++i) {
        const int cx = x + cells[i].dx;
        const int cy = y + cells[i].dy;
        if (cx < 0 || cx >= BOARD_W) return true;
        if (cy < 0 || cy >= BOARD_H) return true;
        if (b.rows[cy] & static_cast<uint16_t>(1u << cx)) return true;
    }
    return false;
}

void lockPiece(Board& b, PieceType p, Rot r, int x, int y) {

    assert(!collides(b, p, r, x, y));
    const Cell* cells = pieceCells(p, r);
    for (int i = 0; i < 4; ++i)
        b.rows[y + cells[i].dy] |= static_cast<uint16_t>(1u << (x + cells[i].dx));
}

int clearLines(Board& b) {

    int write = 0;
    for (int read = 0; read < BOARD_H; ++read) {
        if (b.rows[read] == FULL_ROW) continue;
        b.rows[write++] = b.rows[read];
    }
    const int removed = BOARD_H - write;
    for (int y = write; y < BOARD_H; ++y) b.rows[y] = 0;
    return removed;
}

void addGarbage(Board& b, int lines, int holeCol) {
    if (lines <= 0) return;
    if (lines > BOARD_H) lines = BOARD_H;
    for (int y = BOARD_H - 1; y >= lines; --y) b.rows[y] = b.rows[y - lines];
    const uint16_t row = static_cast<uint16_t>(FULL_ROW & ~(1u << holeCol));
    for (int y = 0; y < lines; ++y) b.rows[y] = row;
}

int dropY(const Board& b, PieceType p, Rot r, int x, int y) {

    assert(!collides(b, p, r, x, y));

    while (!collides(b, p, r, x, y - 1)) --y;
    return y;
}

int columnHeight(const Board& b, int col) {
    const uint16_t mask = static_cast<uint16_t>(1u << col);
    for (int y = BOARD_H - 1; y >= 0; --y)
        if (b.rows[y] & mask) return y + 1;
    return 0;
}

bool isEmpty(const Board& b) {
    for (int y = 0; y < BOARD_H; ++y)
        if (b.rows[y] != 0) return false;
    return true;
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

}
