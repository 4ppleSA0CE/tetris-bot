// tests/tests.cpp -- the entire test suite for tetris-bot.
// Plain assert(), one binary, no framework. Run with `make test`.

#ifdef NDEBUG
#error "tests must be built with assert() enabled -- NDEBUG must not be defined"
#endif

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "core/types.h"
#include "core/piece.h"
#include "core/board.h"

static int g_testCount = 0;

#define RUN(fn)                                \
    do {                                       \
        fn();                                  \
        ++g_testCount;                         \
        std::printf("  ok  %s\n", #fn);        \
    } while (0)

// ---------------------------------------------------------------- core/types.h

static void test_types_constants() {
    assert(tb::BOARD_W == 10);
    assert(tb::BOARD_H == 40);
    assert(tb::VISIBLE_H == 20);
    assert(tb::FULL_ROW == 0x3FF);
    assert(tb::PREVIEW_LEN == 5);
    assert(tb::NUM_PIECES == 7);
    assert(tb::PIECE_I == 0);
    assert(tb::PIECE_J == 1);
    assert(tb::PIECE_L == 2);
    assert(tb::PIECE_O == 3);
    assert(tb::PIECE_S == 4);
    assert(tb::PIECE_T == 5);
    assert(tb::PIECE_Z == 6);
    assert(tb::PIECE_NONE == -1);
    assert(tb::ROT_0 == 0 && tb::ROT_R == 1 && tb::ROT_2 == 2 && tb::ROT_L == 3);
    assert(tb::SPIN_NONE == 0 && tb::SPIN_MINI == 1 && tb::SPIN_FULL == 2);
    // The bitboard must be exactly 40 uint16_t and nothing else.
    assert(sizeof(tb::Board) == 80);
    tb::Board b{};
    for (int y = 0; y < tb::BOARD_H; ++y) assert(b.rows[y] == 0);
}

// ---------------------------------------------------------------- core/piece.h

static bool hasCell(const tb::Cell* cells, int dx, int dy) {
    for (int i = 0; i < 4; ++i)
        if (cells[i].dx == dx && cells[i].dy == dy) return true;
    return false;
}

// Bounding-box size for each piece: 4x4 for I, 2x2 for O, 3x3 for the rest.
static int boxSize(tb::PieceType p) {
    if (p == tb::PIECE_I) return 4;
    if (p == tb::PIECE_O) return 2;
    return 3;
}

static void test_piece_cells_spawn_shapes() {
    // Spawn (ROT_0) shapes, box coordinates, origin = box lower-left, dy up.
    const tb::Cell* i0 = tb::pieceCells(tb::PIECE_I, tb::ROT_0);
    assert(hasCell(i0, 0, 2) && hasCell(i0, 1, 2) && hasCell(i0, 2, 2) && hasCell(i0, 3, 2));

    const tb::Cell* j0 = tb::pieceCells(tb::PIECE_J, tb::ROT_0);
    assert(hasCell(j0, 0, 2) && hasCell(j0, 0, 1) && hasCell(j0, 1, 1) && hasCell(j0, 2, 1));

    const tb::Cell* l0 = tb::pieceCells(tb::PIECE_L, tb::ROT_0);
    assert(hasCell(l0, 2, 2) && hasCell(l0, 0, 1) && hasCell(l0, 1, 1) && hasCell(l0, 2, 1));

    const tb::Cell* o0 = tb::pieceCells(tb::PIECE_O, tb::ROT_0);
    assert(hasCell(o0, 0, 0) && hasCell(o0, 1, 0) && hasCell(o0, 0, 1) && hasCell(o0, 1, 1));

    const tb::Cell* s0 = tb::pieceCells(tb::PIECE_S, tb::ROT_0);
    assert(hasCell(s0, 1, 2) && hasCell(s0, 2, 2) && hasCell(s0, 0, 1) && hasCell(s0, 1, 1));

    const tb::Cell* t0 = tb::pieceCells(tb::PIECE_T, tb::ROT_0);
    assert(hasCell(t0, 1, 2) && hasCell(t0, 0, 1) && hasCell(t0, 1, 1) && hasCell(t0, 2, 1));

    const tb::Cell* z0 = tb::pieceCells(tb::PIECE_Z, tb::ROT_0);
    assert(hasCell(z0, 0, 2) && hasCell(z0, 1, 2) && hasCell(z0, 1, 1) && hasCell(z0, 2, 1));
}

static void test_piece_cells_vertical_shapes() {
    // The four states the kick research names explicitly, other than spawn.
    const tb::Cell* iR = tb::pieceCells(tb::PIECE_I, tb::ROT_R);
    assert(hasCell(iR, 2, 3) && hasCell(iR, 2, 2) && hasCell(iR, 2, 1) && hasCell(iR, 2, 0));

    const tb::Cell* tR = tb::pieceCells(tb::PIECE_T, tb::ROT_R);
    assert(hasCell(tR, 1, 2) && hasCell(tR, 1, 1) && hasCell(tR, 2, 1) && hasCell(tR, 1, 0));

    const tb::Cell* t2 = tb::pieceCells(tb::PIECE_T, tb::ROT_2);
    assert(hasCell(t2, 0, 1) && hasCell(t2, 1, 1) && hasCell(t2, 2, 1) && hasCell(t2, 1, 0));

    const tb::Cell* tL = tb::pieceCells(tb::PIECE_T, tb::ROT_L);
    assert(hasCell(tL, 1, 2) && hasCell(tL, 0, 1) && hasCell(tL, 1, 1) && hasCell(tL, 1, 0));
}

static void test_piece_cells_rotation_is_cw_in_box() {
    // A clockwise quarter-turn inside an n x n box maps (cx, cy) -> (cy, n-1-cx).
    // Applying it to state r must reproduce the cell set of state r+1, for every
    // piece and every state. This catches a single mistyped offset anywhere.
    for (int pi = 0; pi < tb::NUM_PIECES; ++pi) {
        const tb::PieceType p = static_cast<tb::PieceType>(pi);
        const int n = boxSize(p);
        for (int r = 0; r < 4; ++r) {
            const tb::Cell* from = tb::pieceCells(p, static_cast<tb::Rot>(r));
            const tb::Cell* to = tb::pieceCells(p, static_cast<tb::Rot>((r + 1) % 4));
            for (int i = 0; i < 4; ++i) {
                const int rx = from[i].dy;
                const int ry = (n - 1) - from[i].dx;
                assert(hasCell(to, rx, ry));
            }
        }
    }
}

static void test_piece_cells_fit_box_and_are_distinct() {
    for (int pi = 0; pi < tb::NUM_PIECES; ++pi) {
        const tb::PieceType p = static_cast<tb::PieceType>(pi);
        const int n = boxSize(p);
        for (int r = 0; r < 4; ++r) {
            const tb::Cell* c = tb::pieceCells(p, static_cast<tb::Rot>(r));
            for (int i = 0; i < 4; ++i) {
                assert(c[i].dx >= 0 && c[i].dx < n);
                assert(c[i].dy >= 0 && c[i].dy < n);
                for (int k = i + 1; k < 4; ++k)
                    assert(!(c[i].dx == c[k].dx && c[i].dy == c[k].dy));
            }
        }
    }
}

static void test_piece_cells_o_never_changes() {
    const tb::Cell* base = tb::pieceCells(tb::PIECE_O, tb::ROT_0);
    for (int r = 1; r < 4; ++r) {
        const tb::Cell* c = tb::pieceCells(tb::PIECE_O, static_cast<tb::Rot>(r));
        for (int i = 0; i < 4; ++i) assert(hasCell(base, c[i].dx, c[i].dy));
    }
}

// ---------------------------------------------------------------- core/board.h

static void test_boardFromAscii_maps_first_row_to_top() {
    // rows[0] is the TOP row. With 3 rows given, rows[2] becomes board y = 0.
    const char* rows[] = {
        "..........",   // y = 2
        "#........#",   // y = 1
        ".#########",   // y = 0
    };
    const tb::Board b = tb::boardFromAscii(rows, 3);
    assert(b.rows[0] == 0x3FE);   // bits 1..9 set, bit 0 clear
    assert(b.rows[1] == 0x201);   // bits 0 and 9 set
    assert(b.rows[2] == 0x000);
    assert(b.rows[3] == 0x000);
    assert(b.rows[tb::BOARD_H - 1] == 0x000);
}

static void test_boardFromAscii_full_row_equals_FULL_ROW() {
    const char* rows[] = { "##########" };
    const tb::Board b = tb::boardFromAscii(rows, 1);
    assert(b.rows[0] == tb::FULL_ROW);
}

static void test_collides_left_wall() {
    const tb::Board empty{};
    // O occupies columns x and x+1.
    assert(!tb::collides(empty, tb::PIECE_O, tb::ROT_0, 0, 0));
    assert(tb::collides(empty, tb::PIECE_O, tb::ROT_0, -1, 0));
    // I in ROT_0 occupies columns x .. x+3.
    assert(!tb::collides(empty, tb::PIECE_I, tb::ROT_0, 0, 0));
    assert(tb::collides(empty, tb::PIECE_I, tb::ROT_0, -1, 0));
}

static void test_collides_right_wall() {
    const tb::Board empty{};
    assert(!tb::collides(empty, tb::PIECE_O, tb::ROT_0, 8, 0));
    assert(tb::collides(empty, tb::PIECE_O, tb::ROT_0, 9, 0));
    assert(!tb::collides(empty, tb::PIECE_I, tb::ROT_0, 6, 0));
    assert(tb::collides(empty, tb::PIECE_I, tb::ROT_0, 7, 0));
}

static void test_collides_floor_and_ceiling() {
    const tb::Board empty{};
    // O has cells in its box bottom row, so y = -1 puts a cell below the floor.
    assert(!tb::collides(empty, tb::PIECE_O, tb::ROT_0, 4, 0));
    assert(tb::collides(empty, tb::PIECE_O, tb::ROT_0, 4, -1));
    // T in ROT_0 has NO cell in its box bottom row, so y = -1 is legal for it.
    assert(!tb::collides(empty, tb::PIECE_T, tb::ROT_0, 3, -1));
    assert(tb::collides(empty, tb::PIECE_T, tb::ROT_0, 3, -2));
    // Above the allocated 40 rows counts as occupied.
    assert(!tb::collides(empty, tb::PIECE_I, tb::ROT_0, 3, tb::BOARD_H - 3));
    assert(tb::collides(empty, tb::PIECE_I, tb::ROT_0, 3, tb::BOARD_H - 2));
}

static void test_collides_existing_stack() {
    const char* rows[] = {
        "....#.....",   // y = 1
        "##########",   // y = 0
    };
    const tb::Board b = tb::boardFromAscii(rows, 2);
    assert(tb::collides(b, tb::PIECE_O, tb::ROT_0, 4, 0));   // sits on the full row
    assert(tb::collides(b, tb::PIECE_O, tb::ROT_0, 4, 1));   // hits the lone cell at (4,1)
    assert(!tb::collides(b, tb::PIECE_O, tb::ROT_0, 6, 1));  // clear of both
    assert(!tb::collides(b, tb::PIECE_O, tb::ROT_0, 4, 2));  // clear above everything
}

int main() {
    RUN(test_types_constants);
    RUN(test_piece_cells_spawn_shapes);
    RUN(test_piece_cells_vertical_shapes);
    RUN(test_piece_cells_rotation_is_cw_in_box);
    RUN(test_piece_cells_fit_box_and_are_distinct);
    RUN(test_piece_cells_o_never_changes);
    RUN(test_boardFromAscii_maps_first_row_to_top);
    RUN(test_boardFromAscii_full_row_equals_FULL_ROW);
    RUN(test_collides_left_wall);
    RUN(test_collides_right_wall);
    RUN(test_collides_floor_and_ceiling);
    RUN(test_collides_existing_stack);
    std::printf("all %d tests passed\n", g_testCount);
    return 0;
}
