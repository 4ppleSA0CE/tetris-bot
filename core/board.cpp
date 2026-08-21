#include "core/board.h"

#include "core/piece.h"

#include <cassert>

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

void lockPiece(Board& b, PieceType p, Rot r, int x, int y) {
    // The contract is that the caller already checked collides(). Violating it
    // writes out of bounds in b.rows[] below, which corrupts memory rather than
    // just setting a wrong bit. Free in release; live in tb_tests, which -UNDEBUG.
    assert(!collides(b, p, r, x, y));
    const Cell* cells = pieceCells(p, r);
    for (int i = 0; i < 4; ++i)
        b.rows[y + cells[i].dy] |= static_cast<uint16_t>(1u << (x + cells[i].dx));
}

int clearLines(Board& b) {
    // Compact surviving rows toward the bottom, then zero the vacated top rows.
    // This is naive gravity: a row falls by exactly the number of cleared rows
    // below it, and floating blocks are allowed to stay floating.
    int write = 0;
    for (int read = 0; read < BOARD_H; ++read) {
        if (b.rows[read] == FULL_ROW) continue;
        b.rows[write++] = b.rows[read];
    }
    const int removed = BOARD_H - write;
    for (int y = write; y < BOARD_H; ++y) b.rows[y] = 0;
    return removed;
}

int dropY(const Board& b, PieceType p, Rot r, int x, int y) {
    // Precondition: (x, y) must not already collide. Violating it silently
    // returns the caller's own y -- and the search evaluates many candidates
    // without ever locking them, so lockPiece's assert would never see it.
    assert(!collides(b, p, r, x, y));
    // Terminates: collides() reports the floor as occupied, so the walk stops.
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

} // namespace tb
