// tests/tests.cpp -- the entire test suite for tetris-bot.
// Plain assert(), one binary, no framework. Run with `make test`.

#ifdef NDEBUG
#error "tests must be built with assert() enabled -- NDEBUG must not be defined"
#endif

#include <cassert>
#include <cstdint>
#include <cstddef>
#include <string>
#include <cstdio>

#include "core/types.h"
#include "core/piece.h"
#include "core/board.h"
#include "core/srs_tables.h"
#include "core/srs.h"
#include "core/rng.h"
#include "core/attack.h"
#include "core/movegen.h"
#include "core/eval.h"
#include "core/search.h"
#include "core/game.h"
#include "bindings/snapshot.h"
#include "bindings/bot_instance.h"
#include <cmath>
#include <chrono>

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

// ------------------------------------------------- core/board.h: lock + clear

static int popcount16(uint16_t v) {
    int n = 0;
    for (int i = 0; i < 16; ++i) n += (v >> i) & 1;
    return n;
}

static void test_lockPiece_sets_exactly_four_bits() {
    tb::Board b{};
    tb::lockPiece(b, tb::PIECE_T, tb::ROT_0, 3, 0);
    // T ROT_0 cells are (1,2)(0,1)(1,1)(2,1) -> (4,2) and (3,1)(4,1)(5,1).
    assert(b.rows[2] == 0x010);
    assert(b.rows[1] == 0x038);
    assert(b.rows[0] == 0x000);
    int total = 0;
    for (int y = 0; y < tb::BOARD_H; ++y) total += popcount16(b.rows[y]);
    assert(total == 4);
}

static void test_clearLines_removes_none_when_no_row_is_full() {
    const char* rows[] = {
        "#########.",
        ".#########",
    };
    tb::Board b = tb::boardFromAscii(rows, 2);
    assert(tb::clearLines(b) == 0);
    assert(b.rows[0] == 0x3FE);
    assert(b.rows[1] == 0x1FF);
}

static void test_clearLines_single_row_shifts_above_down_by_one() {
    const char* rows[] = {
        "#.........",   // y = 2
        "##########",   // y = 1  <- full, removed
        "..#.......",   // y = 0
    };
    tb::Board b = tb::boardFromAscii(rows, 3);
    assert(tb::clearLines(b) == 1);
    assert(b.rows[0] == 0x004);   // "..#......." untouched, still the bottom row
    assert(b.rows[1] == 0x001);   // "#........." fell from y = 2 to y = 1
    assert(b.rows[2] == 0x000);   // vacated and zeroed
    assert(b.rows[tb::BOARD_H - 1] == 0x000);
}

static void test_clearLines_two_non_adjacent_rows() {
    const char* rows[] = {
        ".....#....",   // y = 4
        "##########",   // y = 3  <- full, removed
        "....#.....",   // y = 2
        "##########",   // y = 1  <- full, removed
        "#.........",   // y = 0
    };
    tb::Board b = tb::boardFromAscii(rows, 5);
    assert(tb::clearLines(b) == 2);
    assert(b.rows[0] == 0x001);   // "#........."  fell 0
    assert(b.rows[1] == 0x010);   // "....#....."  fell 1
    assert(b.rows[2] == 0x020);   // ".....#...."  fell 2
    assert(b.rows[3] == 0x000);
    assert(b.rows[4] == 0x000);
}

static void test_clearLines_four_rows_is_a_tetris() {
    const char* rows[] = {
        "##########",
        "##########",
        "##########",
        "##########",
    };
    tb::Board b = tb::boardFromAscii(rows, 4);
    assert(tb::clearLines(b) == 4);
    for (int y = 0; y < tb::BOARD_H; ++y) assert(b.rows[y] == 0);
}

// -------------------------------------- core/board.h: drop, heights, emptiness

static void test_board_add_garbage() {
    const char* rows[] = { "#.........", "##........" };
    tb::Board b = tb::boardFromAscii(rows, 2);
    b.rows[tb::BOARD_H - 1] = 0x1;                       // will be pushed off the top
    tb::addGarbage(b, 2, 3);
    assert(b.rows[0] == (tb::FULL_ROW & ~(1u << 3)));
    assert(b.rows[1] == (tb::FULL_ROW & ~(1u << 3)));
    assert(b.rows[2] == 0x3);                            // was y=0
    assert(b.rows[3] == 0x1);                            // was y=1
    assert(b.rows[4] == 0);
    assert(b.rows[tb::BOARD_H - 1] == 0);
    tb::Board c = b;
    tb::addGarbage(c, 0, 5);                             // zero lines is a no-op
    for (int y = 0; y < tb::BOARD_H; ++y) assert(c.rows[y] == b.rows[y]);
}

static void test_dropY_falls_to_the_floor_on_an_empty_board() {
    const tb::Board empty{};
    // O has cells in its box bottom row, so it rests at y = 0.
    assert(tb::dropY(empty, tb::PIECE_O, tb::ROT_0, 4, 21) == 0);
    // T in ROT_0 has none, so its box origin rests one below the floor.
    assert(tb::dropY(empty, tb::PIECE_T, tb::ROT_0, 3, 21) == -1);
}

static void test_dropY_lands_on_a_stack() {
    const char* rows[] = {
        "#.........",   // y = 2
        "#.........",   // y = 1
        "#.........",   // y = 0
    };
    const tb::Board b = tb::boardFromAscii(rows, 3);
    // O spans columns 0 and 1; column 0 is filled to y = 2, so it rests at y = 3.
    assert(tb::dropY(b, tb::PIECE_O, tb::ROT_0, 0, 21) == 3);
    // Clear of the tower, it falls all the way.
    assert(tb::dropY(b, tb::PIECE_O, tb::ROT_0, 4, 21) == 0);
}

static void test_dropY_is_idempotent_at_rest() {
    const tb::Board empty{};
    const int y = tb::dropY(empty, tb::PIECE_O, tb::ROT_0, 4, 21);
    assert(tb::dropY(empty, tb::PIECE_O, tb::ROT_0, 4, y) == y);
}

static void test_columnHeight() {
    const char* rows[] = {
        "#.........",   // y = 2
        "#.........",   // y = 1
        "#.#.......",   // y = 0
    };
    const tb::Board b = tb::boardFromAscii(rows, 3);
    assert(tb::columnHeight(b, 0) == 3);
    assert(tb::columnHeight(b, 1) == 0);
    assert(tb::columnHeight(b, 2) == 1);
    assert(tb::columnHeight(b, 9) == 0);
    const tb::Board empty{};
    for (int c = 0; c < tb::BOARD_W; ++c) assert(tb::columnHeight(empty, c) == 0);
}

static void test_columnHeight_counts_over_a_hole() {
    const char* rows[] = {
        "#.........",   // y = 1
        "..........",   // y = 0   <- hole underneath
    };
    const tb::Board b = tb::boardFromAscii(rows, 2);
    assert(tb::columnHeight(b, 0) == 2);
}

static void test_isEmpty() {
    tb::Board b{};
    assert(tb::isEmpty(b));
    tb::lockPiece(b, tb::PIECE_O, tb::ROT_0, 4, 0);
    assert(!tb::isEmpty(b));
    (void)tb::clearLines(b);
    assert(!tb::isEmpty(b));   // an O alone never fills a row
    b = tb::Board{};
    assert(tb::isEmpty(b));
}

// ---------------------------------------------------------- core/srs_tables.h

static bool kickEq(tb::Kick a, int dx, int dy) { return a.dx == dx && a.dy == dy; }

static void test_kick_tables_first_test_is_identity() {
    for (int t = 0; t < 8; ++t) {
        assert(kickEq(tb::KICKS_JLSTZ[t][0], 0, 0));
        assert(kickEq(tb::KICKS_I[t][0], 0, 0));
    }
    for (int t = 0; t < 4; ++t) {
        assert(kickEq(tb::KICKS_180[t][0], 0, 0));
        assert(kickEq(tb::KICKS_180_I[t][0], 0, 0));
    }
}

// Both kick tables collapse to 4 distinct rows, each appearing twice -- but the
// two tables pair DIFFERENT transitions, and that difference is the whole point.
//
// Verified against https://harddrop.com/wiki/SRS and
// https://tetris.wiki/Super_Rotation_System. Kicks are derived as
// kick(A->B) = offsetA - offsetB from per-state offset vectors.
//
//   JLSTZ: states 0 and 2 have identical ALL-ZERO offsets, so any transition
//          into or out of R (or L) is the same regardless of the other end.
//          Pairs: [0]0->R == [3]2->R, [1]R->0 == [2]R->2,
//                 [4]2->L == [7]0->L, [5]L->2 == [6]L->0
//
//   I:     no offset vector is zero -- the I piece's rotation pivot is not at
//          its geometric center -- so that collapse does not apply. Instead a
//          cross-sum symmetry (offset0 + offset2 == offsetR + offsetL) pairs
//          OPPOSITE transitions.
//          Pairs: [0]0->R == [5]L->2, [1]R->0 == [4]2->L,
//                 [2]R->2 == [7]0->L, [3]2->R == [6]L->0
//
// Asserting merely "4 distinct rows" would pass against the single most likely
// transcription bug: copying JLSTZ's pairing onto the I table. So this checks
// all 28 unordered row pairs and requires an EXACT match to the expected
// structure -- every listed pair identical, every other pair different.
static void assertKickDuplicateStructure(const tb::Kick table[][5],
                                         const int pairs[][2], int nPairs) {
    bool shouldMatch[8][8] = {};
    for (int i = 0; i < nPairs; ++i) {
        shouldMatch[pairs[i][0]][pairs[i][1]] = true;
        shouldMatch[pairs[i][1]][pairs[i][0]] = true;
    }
    for (int a = 0; a < 8; ++a) {
        for (int b = a + 1; b < 8; ++b) {
            bool same = true;
            for (int k = 0; k < 5; ++k)
                if (!kickEq(table[a][k], table[b][k].dx, table[b][k].dy)) same = false;
            assert(same == shouldMatch[a][b]);
        }
    }
}

static void test_kick_table_jlstz_duplicate_structure() {
    static const int pairs[4][2] = {{0, 3}, {1, 2}, {4, 7}, {5, 6}};
    assertKickDuplicateStructure(tb::KICKS_JLSTZ, pairs, 4);
}

static void test_kick_table_i_duplicate_structure() {
    static const int pairs[4][2] = {{0, 5}, {1, 4}, {2, 7}, {3, 6}};
    assertKickDuplicateStructure(tb::KICKS_I, pairs, 4);
}

static void test_kick_table_spot_checks() {
    // 0->R (index 0)
    assert(kickEq(tb::KICKS_JLSTZ[0][4], -1, -2));
    // 0->L (index 7) -- the offset the T-spin-triple case turns on
    assert(kickEq(tb::KICKS_JLSTZ[7][4], 1, -2));
    // 2->L (index 4)
    assert(kickEq(tb::KICKS_JLSTZ[4][4], 1, -2));
    // L->2 (index 5)
    assert(kickEq(tb::KICKS_JLSTZ[5][4], -1, 2));
    // I 0->R (index 0): test 3 and test 4, in this order. Swapping them is the
    // real published-library bug this test exists to catch.
    assert(kickEq(tb::KICKS_I[0][3], -2, -1));
    assert(kickEq(tb::KICKS_I[0][4], 1, 2));
    // I R->2 (index 2): test 4 is (2,-1), NOT (2,1).
    assert(kickEq(tb::KICKS_I[2][4], 2, -1));
    // 180: 0->2 lifts one row on its second test.
    assert(kickEq(tb::KICKS_180[0][1], 0, 1));
    assert(kickEq(tb::KICKS_180[2][1], 0, -1));
}

static void test_kick_180_every_offset_is_within_one_column_and_two_rows() {
    for (int t = 0; t < 4; ++t) {
        for (int k = 0; k < 6; ++k) {
            assert(tb::KICKS_180[t][k].dx >= -1 && tb::KICKS_180[t][k].dx <= 1);
            assert(tb::KICKS_180[t][k].dy >= -1 && tb::KICKS_180[t][k].dy <= 2);
        }
    }
}

// ----------------------------------------------------------------- core/srs.h

// Assert that the candidate position for one kick test collides, i.e. that this
// test is skipped. `to` is the rotation being rotated INTO.
static void expectKickTestCollides(const tb::Board& b, tb::PieceType p, tb::Rot to,
                                   int x, int y, int dx, int dy) {
    assert(tb::collides(b, p, to, x + dx, y + dy));
}

static void test_kick_case1_tspin_triple() {
    // T, 0 -> L (table row 7), from (3,2). Tests 0..3 collide; test 4 (+1,-2)
    // lands the piece at (4,0), grounded, clearing rows 0, 1 and 2.
    const char* rows[] = {
        ".....#####",   // y = 4
        "......####",   // y = 3
        "#####.####",   // y = 2
        "####..####",   // y = 1
        "#####.####",   // y = 0
    };
    tb::Board b = tb::boardFromAscii(rows, 5);
    assert(!tb::collides(b, tb::PIECE_T, tb::ROT_0, 3, 2));
    expectKickTestCollides(b, tb::PIECE_T, tb::ROT_L, 3, 2, 0, 0);
    expectKickTestCollides(b, tb::PIECE_T, tb::ROT_L, 3, 2, 1, 0);
    expectKickTestCollides(b, tb::PIECE_T, tb::ROT_L, 3, 2, 1, 1);
    expectKickTestCollides(b, tb::PIECE_T, tb::ROT_L, 3, 2, 0, -2);

    int ox = -99, oy = -99;
    uint8_t k = 255;
    assert(tb::tryRotate(b, tb::PIECE_T, tb::ROT_0, tb::ROT_L, 3, 2, &ox, &oy, &k));
    assert(ox == 4);
    assert(oy == 0);
    assert(k == 4);

    assert(tb::collides(b, tb::PIECE_T, tb::ROT_L, ox, oy - 1));   // grounded
    tb::lockPiece(b, tb::PIECE_T, tb::ROT_L, ox, oy);
    assert(tb::clearLines(b) == 3);
}

static void test_kick_case2_s_piece() {
    // S, 0 -> R (table row 0), from (4,2). Tests 0..3 collide; test 4 (-1,-2)
    // lands the piece at (3,0), grounded, clearing rows 0 and 1.
    const char* rows[] = {
        "#####.....",   // y = 4
        "####......",   // y = 3
        "####.####.",   // y = 2
        "####..####",   // y = 1
        "#####.####",   // y = 0
    };
    tb::Board b = tb::boardFromAscii(rows, 5);
    assert(!tb::collides(b, tb::PIECE_S, tb::ROT_0, 4, 2));
    expectKickTestCollides(b, tb::PIECE_S, tb::ROT_R, 4, 2, 0, 0);
    expectKickTestCollides(b, tb::PIECE_S, tb::ROT_R, 4, 2, -1, 0);
    expectKickTestCollides(b, tb::PIECE_S, tb::ROT_R, 4, 2, -1, 1);
    expectKickTestCollides(b, tb::PIECE_S, tb::ROT_R, 4, 2, 0, -2);

    int ox = -99, oy = -99;
    uint8_t k = 255;
    assert(tb::tryRotate(b, tb::PIECE_S, tb::ROT_0, tb::ROT_R, 4, 2, &ox, &oy, &k));
    assert(ox == 3);
    assert(oy == 0);
    assert(k == 4);

    assert(tb::collides(b, tb::PIECE_S, tb::ROT_R, ox, oy - 1));   // grounded
    tb::lockPiece(b, tb::PIECE_S, tb::ROT_R, ox, oy);
    assert(tb::clearLines(b) == 2);
}

static void test_kick_case3_z_piece_mirrors_case2() {
    // Z, 0 -> L (table row 7), from (3,2). Exact mirror of the S case: if one
    // passes and the other fails, the sign error is isolated to one table row.
    const char* rows[] = {
        ".....#####",   // y = 4
        "......####",   // y = 3
        ".####.####",   // y = 2
        "####..####",   // y = 1
        "####.#####",   // y = 0
    };
    tb::Board b = tb::boardFromAscii(rows, 5);
    assert(!tb::collides(b, tb::PIECE_Z, tb::ROT_0, 3, 2));
    expectKickTestCollides(b, tb::PIECE_Z, tb::ROT_L, 3, 2, 0, 0);
    expectKickTestCollides(b, tb::PIECE_Z, tb::ROT_L, 3, 2, 1, 0);
    expectKickTestCollides(b, tb::PIECE_Z, tb::ROT_L, 3, 2, 1, 1);
    expectKickTestCollides(b, tb::PIECE_Z, tb::ROT_L, 3, 2, 0, -2);

    int ox = -99, oy = -99;
    uint8_t k = 255;
    assert(tb::tryRotate(b, tb::PIECE_Z, tb::ROT_0, tb::ROT_L, 3, 2, &ox, &oy, &k));
    assert(ox == 4);
    assert(oy == 0);
    assert(k == 4);

    assert(tb::collides(b, tb::PIECE_Z, tb::ROT_L, ox, oy - 1));   // grounded
    tb::lockPiece(b, tb::PIECE_Z, tb::ROT_L, ox, oy);
    assert(tb::clearLines(b) == 2);
}

static void test_kick_case4_i_piece_asserts_test_order() {
    // I, 0 -> R (table row 0), from (4,2). Tests 0..2 collide; test 3 (-2,-1)
    // wins at (2,1). Test 4 (+1,+2) would ALSO be legal -- so this case asserts
    // ORDER, not membership. Scanning the row backwards returns (5,4) and fails.
    const char* rows[] = {
        "....#.....",   // y = 5
        "..........",   // y = 4
        "####.#####",   // y = 3
        "####.#####",   // y = 2
        "####.#####",   // y = 1
        "#########.",   // y = 0
    };
    tb::Board b = tb::boardFromAscii(rows, 6);
    assert(!tb::collides(b, tb::PIECE_I, tb::ROT_0, 4, 2));
    expectKickTestCollides(b, tb::PIECE_I, tb::ROT_R, 4, 2, 0, 0);
    expectKickTestCollides(b, tb::PIECE_I, tb::ROT_R, 4, 2, -2, 0);
    expectKickTestCollides(b, tb::PIECE_I, tb::ROT_R, 4, 2, 1, 0);
    assert(!tb::collides(b, tb::PIECE_I, tb::ROT_R, 4 + 1, 2 + 2));   // test 4 is legal

    int ox = -99, oy = -99;
    uint8_t k = 255;
    assert(tb::tryRotate(b, tb::PIECE_I, tb::ROT_0, tb::ROT_R, 4, 2, &ox, &oy, &k));
    assert(ox == 2);
    assert(oy == 1);
    assert(k == 3);

    assert(tb::collides(b, tb::PIECE_I, tb::ROT_R, ox, oy - 1));   // grounded
    tb::lockPiece(b, tb::PIECE_I, tb::ROT_R, ox, oy);
    assert(tb::clearLines(b) == 3);
}

static void test_rotate_o_never_kicks() {
    const tb::Board empty{};
    for (int from = 0; from < 4; ++from) {
        for (int to = 0; to < 4; ++to) {
            if (from == to) continue;
            int ox = -99, oy = -99;
            uint8_t k = 255;
            const bool ok = tb::tryRotate(empty, tb::PIECE_O,
                                          static_cast<tb::Rot>(from),
                                          static_cast<tb::Rot>(to),
                                          4, 3, &ox, &oy, &k);
            assert(ok);
            assert(ox == 4);   // never moves
            assert(oy == 3);
            assert(k == 0);
        }
    }
}

static void test_rotate_rejected_when_every_test_collides() {
    // A T wedged into a slot exactly its own shape. All five 0->R tests hit
    // something, so the rotation is refused and the outputs stay untouched.
    const char* rows[] = {
        "##.#######",   // y = 3
        "#...######",   // y = 2
        "##########",   // y = 1
        "##########",   // y = 0
    };
    const tb::Board b = tb::boardFromAscii(rows, 4);
    assert(!tb::collides(b, tb::PIECE_T, tb::ROT_0, 1, 1));
    expectKickTestCollides(b, tb::PIECE_T, tb::ROT_R, 1, 1, 0, 0);
    expectKickTestCollides(b, tb::PIECE_T, tb::ROT_R, 1, 1, -1, 0);
    expectKickTestCollides(b, tb::PIECE_T, tb::ROT_R, 1, 1, -1, 1);
    expectKickTestCollides(b, tb::PIECE_T, tb::ROT_R, 1, 1, 0, -2);
    expectKickTestCollides(b, tb::PIECE_T, tb::ROT_R, 1, 1, -1, -2);

    int ox = -99, oy = -99;
    uint8_t k = 255;
    assert(!tb::tryRotate(b, tb::PIECE_T, tb::ROT_0, tb::ROT_R, 1, 1, &ox, &oy, &k));
    assert(ox == -99);
    assert(oy == -99);
    assert(k == 255);
}

static void test_rotate_to_the_same_rotation_is_rejected() {
    const tb::Board empty{};
    int ox = -99, oy = -99;
    uint8_t k = 255;
    assert(!tb::tryRotate(empty, tb::PIECE_T, tb::ROT_0, tb::ROT_0, 4, 3, &ox, &oy, &k));
    assert(ox == -99 && oy == -99 && k == 255);
}

// -------------------------------------------------- core/srs.h: 180 rotation

static void test_kick_case5_180_floor_kick() {
    // Empty board. A T rests flat on the floor at box origin (3,-1) -- legal,
    // because T in ROT_0 has no cells in its box's bottom row, so its cells are
    // (4,1),(3,0),(4,0),(5,0). A 180 in place would push the nub to y = -1, so
    // KICKS_180 test 1 (0,+1) must lift it one row.
    const tb::Board empty{};
    assert(!tb::collides(empty, tb::PIECE_T, tb::ROT_0, 3, -1));
    expectKickTestCollides(empty, tb::PIECE_T, tb::ROT_2, 3, -1, 0, 0);

    int ox = -99, oy = -99;
    uint8_t k = 255;
    assert(tb::tryRotate(empty, tb::PIECE_T, tb::ROT_0, tb::ROT_2, 3, -1, &ox, &oy, &k));
    assert(ox == 3);
    assert(oy == 0);
    assert(k == 1);

    // Grounded, and the resulting cells are (3,1),(4,1),(5,1),(4,0).
    assert(tb::collides(empty, tb::PIECE_T, tb::ROT_2, ox, oy - 1));
    tb::Board b = empty;
    tb::lockPiece(b, tb::PIECE_T, tb::ROT_2, ox, oy);
    assert(b.rows[1] == 0x038);
    assert(b.rows[0] == 0x010);
    assert(tb::clearLines(b) == 0);
}

static void test_180_in_open_space_uses_test_zero() {
    const tb::Board empty{};
    int ox = -99, oy = -99;
    uint8_t k = 255;
    assert(tb::tryRotate(empty, tb::PIECE_T, tb::ROT_0, tb::ROT_2, 3, 5, &ox, &oy, &k));
    assert(ox == 3 && oy == 5 && k == 0);

    ox = -99; oy = -99; k = 255;
    assert(tb::tryRotate(empty, tb::PIECE_S, tb::ROT_R, tb::ROT_L, 3, 5, &ox, &oy, &k));
    assert(ox == 3 && oy == 5 && k == 0);

    ox = -99; oy = -99; k = 255;
    assert(tb::tryRotate(empty, tb::PIECE_Z, tb::ROT_2, tb::ROT_0, 3, 5, &ox, &oy, &k));
    assert(ox == 3 && oy == 5 && k == 0);

    ox = -99; oy = -99; k = 255;
    assert(tb::tryRotate(empty, tb::PIECE_J, tb::ROT_L, tb::ROT_R, 3, 5, &ox, &oy, &k));
    assert(ox == 3 && oy == 5 && k == 0);
}

static void test_180_i_piece_has_exactly_one_test() {
    const tb::Board empty{};
    int ox = -99, oy = -99;
    uint8_t k = 255;
    assert(tb::tryRotate(empty, tb::PIECE_I, tb::ROT_0, tb::ROT_2, 3, 5, &ox, &oy, &k));
    assert(ox == 3 && oy == 5 && k == 0);
}

static void test_180_i_piece_is_rejected_when_its_only_test_collides() {
    // I in ROT_0 sits at y+2; ROT_2 puts it at y+1, which is a full row here.
    // The I 180 table has no second test, so the rotation must be refused --
    // an implementation that reuses the JLSTZ 180 table would kick and pass.
    const char* rows[] = {
        "..........",   // y = 2
        "##########",   // y = 1
        "..........",   // y = 0
    };
    const tb::Board b = tb::boardFromAscii(rows, 3);
    assert(!tb::collides(b, tb::PIECE_I, tb::ROT_0, 3, 0));
    assert(tb::collides(b, tb::PIECE_I, tb::ROT_2, 3, 0));

    int ox = -99, oy = -99;
    uint8_t k = 255;
    assert(!tb::tryRotate(b, tb::PIECE_I, tb::ROT_0, tb::ROT_2, 3, 0, &ox, &oy, &k));
    assert(ox == -99 && oy == -99 && k == 255);
}

// ----------------------------------------------------------------- core/rng.h

static void test_xorshift32_is_deterministic_and_never_zero() {
    uint32_t a = 12345;
    uint32_t b = 12345;
    for (int i = 0; i < 1000; ++i) {
        const uint32_t va = tb::xorshift32(a);
        const uint32_t vb = tb::xorshift32(b);
        assert(va == vb);
        assert(va != 0);
        assert(a == va);   // the state is advanced in place
    }
    uint32_t c = 99;
    uint32_t first = tb::xorshift32(c);
    uint32_t second = tb::xorshift32(c);
    assert(first != second);
}

static void test_bag_every_seven_pieces_is_a_permutation() {
    tb::Bag bag(20260821u);
    for (int b = 0; b < 100; ++b) {
        int seen[tb::NUM_PIECES] = {0, 0, 0, 0, 0, 0, 0};
        for (int i = 0; i < 7; ++i) {
            const tb::PieceType p = bag.next();
            assert(p >= 0);
            assert(p < tb::NUM_PIECES);
            ++seen[static_cast<int>(p)];
        }
        for (int i = 0; i < tb::NUM_PIECES; ++i) assert(seen[i] == 1);
    }
}

static void test_bag_same_seed_same_sequence() {
    tb::Bag a(7u);
    tb::Bag b(7u);
    for (int i = 0; i < 700; ++i) assert(a.next() == b.next());
}

static void test_bag_different_seeds_diverge() {
    tb::Bag a(7u);
    tb::Bag b(8u);
    bool differs = false;
    for (int i = 0; i < 700; ++i)
        if (a.next() != b.next()) differs = true;
    assert(differs);
}

static void test_bag_reset_replays_the_same_sequence() {
    tb::Bag bag(4242u);
    tb::PieceType first[70];
    for (int i = 0; i < 70; ++i) first[i] = bag.next();
    bag.reset(4242u);
    for (int i = 0; i < 70; ++i) assert(bag.next() == first[i]);
    bag.reset(4243u);
    bool differs = false;
    for (int i = 0; i < 70; ++i)
        if (bag.next() != first[i]) differs = true;
    assert(differs);
}

static void test_bag_seed_zero_is_remapped_and_still_shuffles() {
    // A raw xorshift32 seeded with 0 is a fixed point -- it returns 0 forever.
    // Every Fisher-Yates index then becomes 0, and the shuffle degenerates into
    // the SAME fixed rotation (1,2,3,4,5,6,0) on every single bag.
    //
    // Note what that rotation is NOT: it is not piece order, and it is a valid
    // permutation. So asserting "some bag is not in piece order", or asserting
    // each bag is a permutation, both PASS against the broken implementation.
    // The property that actually separates them is that the broken one emits
    // the IDENTICAL bag every time, while a correctly remapped seed produces
    // ~96 distinct orderings per 100 bags.
    //
    // Seed 0 must be remapped to 0x9E3779B9.
    tb::Bag zero(0u);

    tb::PieceType firstBag[7];
    int firstSeen[tb::NUM_PIECES] = {0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 7; ++i) {
        firstBag[i] = zero.next();
        ++firstSeen[static_cast<int>(firstBag[i])];
    }
    for (int i = 0; i < tb::NUM_PIECES; ++i) assert(firstSeen[i] == 1);

    bool sawDifferentBag = false;
    for (int b = 1; b < 100; ++b) {
        int seen[tb::NUM_PIECES] = {0, 0, 0, 0, 0, 0, 0};
        for (int i = 0; i < 7; ++i) {
            const tb::PieceType p = zero.next();
            ++seen[static_cast<int>(p)];
            if (p != firstBag[i]) sawDifferentBag = true;
        }
        for (int i = 0; i < tb::NUM_PIECES; ++i) assert(seen[i] == 1);
    }
    assert(sawDifferentBag);
}

// -------------------------------------------------------------- core/attack.h

static tb::ClearInfo ci(int lines, tb::SpinKind spin, bool perfectClear) {
    tb::ClearInfo c;
    c.lines = static_cast<uint8_t>(lines);
    c.spin = spin;
    c.perfectClear = perfectClear;
    return c;
}

static void test_attack_prd_table_without_b2b_or_combo() {
    // PRD section 4.7, every row, b2b inactive and no combo.
    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, false), false, 0) == 0);   // single
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), false, 0) == 1);   // double
    assert(tb::computeAttack(ci(3, tb::SPIN_NONE, false), false, 0) == 2);   // triple
    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, false), false, 0) == 4);   // tetris
    assert(tb::computeAttack(ci(1, tb::SPIN_MINI, false), false, 0) == 0);   // mini single
    assert(tb::computeAttack(ci(2, tb::SPIN_MINI, false), false, 0) == 1);   // mini double
    assert(tb::computeAttack(ci(3, tb::SPIN_MINI, false), false, 0) == 2);   // mini triple
    assert(tb::computeAttack(ci(4, tb::SPIN_MINI, false), false, 0) == 4);   // mini quad (I-spin)
    assert(tb::computeAttack(ci(1, tb::SPIN_FULL, false), false, 0) == 2);   // t-spin single
    assert(tb::computeAttack(ci(2, tb::SPIN_FULL, false), false, 0) == 4);   // t-spin double
    assert(tb::computeAttack(ci(3, tb::SPIN_FULL, false), false, 0) == 6);   // t-spin triple
    assert(tb::computeAttack(ci(4, tb::SPIN_FULL, false), false, 0) == 10);  // spin quad
}

static void test_attack_b2b_bonus_is_applied() {
    // +1, and only when the clear is itself b2b-maintaining AND the chain was
    // already live before this clear.
    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, false), true, 0) == 5);   // tetris
    assert(tb::computeAttack(ci(1, tb::SPIN_FULL, false), true, 0) == 3);   // tss
    assert(tb::computeAttack(ci(2, tb::SPIN_FULL, false), true, 0) == 5);   // tsd
    assert(tb::computeAttack(ci(3, tb::SPIN_FULL, false), true, 0) == 7);   // tst
    assert(tb::computeAttack(ci(1, tb::SPIN_MINI, false), true, 0) == 1);   // mini single
    assert(tb::computeAttack(ci(2, tb::SPIN_MINI, false), true, 0) == 2);   // mini double
}

static void test_attack_b2b_bonus_is_not_applied() {
    // An easy clear gets no bonus even with the chain live -- it breaks it.
    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, false), true, 0) == 0);   // single
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), true, 0) == 1);   // double
    assert(tb::computeAttack(ci(3, tb::SPIN_NONE, false), true, 0) == 2);   // triple
    // And a difficult clear gets no bonus when the chain was not live.
    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, false), false, 0) == 4);
    assert(tb::computeAttack(ci(2, tb::SPIN_FULL, false), false, 0) == 4);
}

static void test_b2bMaintaining_rules() {
    assert(tb::b2bMaintaining(ci(4, tb::SPIN_NONE, false)));    // tetris
    assert(tb::b2bMaintaining(ci(1, tb::SPIN_FULL, false)));
    assert(tb::b2bMaintaining(ci(2, tb::SPIN_FULL, false)));
    assert(tb::b2bMaintaining(ci(3, tb::SPIN_FULL, false)));
    assert(!tb::b2bMaintaining(ci(1, tb::SPIN_NONE, false)));   // single
    assert(!tb::b2bMaintaining(ci(2, tb::SPIN_NONE, false)));   // double
    assert(!tb::b2bMaintaining(ci(3, tb::SPIN_NONE, false)));   // triple
    // Zero-line placements are neutral, never maintaining -- the caller must
    // leave the flag alone rather than setting or clearing it.
    assert(!tb::b2bMaintaining(ci(0, tb::SPIN_NONE, false)));
    assert(!tb::b2bMaintaining(ci(0, tb::SPIN_MINI, false)));
    assert(!tb::b2bMaintaining(ci(0, tb::SPIN_FULL, false)));
}

static void test_b2b_is_maintained_by_a_mini_that_clears_lines() {
    // AMENDED RULE. A T-spin mini that clears lines is "difficult": it extends
    // an existing chain and starts a new one. Gate on
    // (lines > 0 && spin != SPIN_NONE), never on spin == SPIN_FULL.
    assert(tb::b2bMaintaining(ci(1, tb::SPIN_MINI, false)));
    assert(tb::b2bMaintaining(ci(2, tb::SPIN_MINI, false)));
    // A mini clearing nothing stays neutral.
    assert(!tb::b2bMaintaining(ci(0, tb::SPIN_MINI, false)));
}

// g = (base + b2b) * (1 + 0.25c), c = prior consecutive clears; c >= 2: at least ln(1 + 1.25c);
// rounded down. Expected values worked by hand from the formula.
static void test_attack_combo_multiplier() {
    const int dbl[13] = {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4};
    for (int c = 0; c < 13; ++c) {
        assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), false, c) == dbl[c]);
    }
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), false, 20) == 6);
    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, false), false, 1) == 5);
    assert(tb::computeAttack(ci(2, tb::SPIN_FULL, false), true, 3) == 8);    // (4+1)*1.75
    assert(tb::computeAttack(ci(2, tb::SPIN_FULL, false), true, 4) == 10);
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), false, -1) == 1);
}

// ln(3.5)=1.25 -> 1 at c=2, ln(8.5)=2.14 -> 2 at c=6, ln(21)=3.04 -> 3 at c=16.
static void test_attack_combo_log_floor_for_zero_base() {
    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, false), false, 0) == 0);
    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, false), false, 1) == 0);
    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, false), false, 2) == 1);
    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, false), false, 5) == 1);
    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, false), false, 6) == 2);
    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, false), false, 15) == 2);
    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, false), false, 16) == 3);
    assert(tb::computeAttack(ci(1, tb::SPIN_MINI, false), false, 2) == 1);
}

static void test_attack_combo_multiplier_is_unbounded() {
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), false, 50) == 13);
    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, false), false, 100) == 104);
}

// All Clear: +5 flat after rounding, and it is a difficult clear.
static void test_attack_all_clear_adds_five_and_holds_b2b() {
    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, true), false, 0) == 9);
    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, true), false, 0) == 5);
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, true), true, 0) == 1 + 1 + 5);
    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, true), true, 5) == 11 + 5);       // (4+1)*2.25 = 11.25
    assert(tb::b2bMaintaining(ci(1, tb::SPIN_NONE, true)));
    assert(tb::b2bMaintaining(ci(2, tb::SPIN_NONE, true)));
    assert(!tb::b2bMaintaining(ci(2, tb::SPIN_NONE, false)));
}

// TETR.IO B2B Charging: from an internal count of 5 (displayed "B2B x4") a chain holds a
// Surge of count-1 lines, released in full by the clear that breaks it, never combo-scaled.
static void test_surge_charge_by_chain_length() {
    for (int c = 0; c <= 4; ++c) assert(tb::surgeCharge(c) == 0);
    assert(tb::surgeCharge(5) == 4);
    assert(tb::surgeCharge(9) == 8);
    assert(tb::surgeCharge(31) == 30);
}

static void test_attack_surge_is_sent_on_break() {
    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, false), 5, 0) == 4);
    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, false), 4, 0) == 0);
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), 9, 3) == 1 + 8);      // 1.75 -> 1, plus 8
    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, false), 9, 0) == 4 + 1);      // held, not broken
    assert(tb::computeAttack(ci(1, tb::SPIN_MINI, false), 9, 0) == 0 + 1);
    assert(tb::computeAttack(ci(0, tb::SPIN_NONE, false), 9, 0) == 0);
}

static void test_attack_is_zero_when_no_lines_clear() {
    assert(tb::computeAttack(ci(0, tb::SPIN_NONE, false), true, 8) == 0);
    assert(tb::computeAttack(ci(0, tb::SPIN_MINI, false), true, 8) == 0);
    assert(tb::computeAttack(ci(0, tb::SPIN_FULL, false), true, 8) == 0);
}

// ------------------------------------------------- movegen assumption guards

// The T's centre mino is the one cell present in all four rotation states.
// Every T-spin corner offset in this milestone is measured from it, and the
// piece origin is the bounding box's lower-left corner, so it sits at (+1,+1).
static void test_mg_t_center_is_origin_plus_one_one() {
    for (int r = 0; r < 4; ++r) {
        const tb::Cell* c = tb::pieceCells(tb::PIECE_T, static_cast<tb::Rot>(r));
        bool found = false;
        for (int i = 0; i < 4; ++i)
            if (c[i].dx == 1 && c[i].dy == 1) found = true;
        assert(found);   // see remediation note in the plan if this fires
    }
    // And the nub -- the cell that is NOT shared -- points the way the
    // front-corner table in classifyTSpin says it does.
    const tb::Cell* t0 = tb::pieceCells(tb::PIECE_T, tb::ROT_0);
    const tb::Cell* tR = tb::pieceCells(tb::PIECE_T, tb::ROT_R);
    const tb::Cell* t2 = tb::pieceCells(tb::PIECE_T, tb::ROT_2);
    const tb::Cell* tL = tb::pieceCells(tb::PIECE_T, tb::ROT_L);
    bool nub0 = false, nubR = false, nub2 = false, nubL = false;
    for (int i = 0; i < 4; ++i) {
        if (t0[i].dx == 1 && t0[i].dy == 2) nub0 = true;   // up
        if (tR[i].dx == 2 && tR[i].dy == 1) nubR = true;   // right
        if (t2[i].dx == 1 && t2[i].dy == 0) nub2 = true;   // down
        if (tL[i].dx == 0 && tL[i].dy == 1) nubL = true;   // left
    }
    assert(nub0 && nubR && nub2 && nubL);
}

// The BFS indexes a flat array by (x, y, rot) and rejects out-of-window states
// before indexing. The window is derived from this bound, so assert the bound.
static void test_mg_cell_offsets_fit_zero_to_three() {
    for (int pi = 0; pi < tb::NUM_PIECES; ++pi) {
        for (int r = 0; r < 4; ++r) {
            const tb::Cell* c = tb::pieceCells(static_cast<tb::PieceType>(pi),
                                              static_cast<tb::Rot>(r));
            for (int i = 0; i < 4; ++i) {
                assert(c[i].dx >= 0 && c[i].dx <= 3);
                assert(c[i].dy >= 0 && c[i].dy <= 3);
            }
        }
    }
}

// ACT_180 is hardcoded on (Q&A amendment to PRD 4.2), so tryRotate must accept
// a 0 <-> 2 and R <-> L transition and use KICKS_180. This is the floor-kick
// case from the kick research, converted to the box-origin convention:
// a T flat on the floor at (3, -1) cannot 180 in place -- its nub would land at
// y = -1 -- so test 1 of the 180 table, (0, +1), must lift it to (3, 0).
static void test_mg_tryRotate_supports_180() {
    const tb::Board empty{};
    int nx = -99, ny = -99;
    uint8_t k = 200;
    assert(tb::tryRotate(empty, tb::PIECE_T, tb::ROT_0, tb::ROT_2, 3, -1,
                         &nx, &ny, &k));
    assert(nx == 3);
    assert(ny == 0);
    assert(k == 1);
}

// O never kicks and its four states are the same four cells, so rotating O
// always succeeds in place. The BFS relies on this: O reaches all four rotation
// states, which is why its placement count is 36 and not 9.
static void test_mg_tryRotate_o_always_succeeds() {
    const tb::Board empty{};
    for (int r = 0; r < 4; ++r) {
        const tb::Rot from = static_cast<tb::Rot>(r);
        const tb::Rot to = static_cast<tb::Rot>((r + 1) & 3);
        int nx = -99, ny = -99;
        uint8_t k = 200;
        assert(tb::tryRotate(empty, tb::PIECE_O, from, to, 4, 5, &nx, &ny, &k));
        assert(nx == 4 && ny == 5);
    }
}

// ---------------------------------------------------------------- core/movegen.h

static void test_mg_state_index_roundtrips() {
    // Every state in the window must map to a distinct index in [0, MG_STATES)
    // and decode back to itself.
    static bool seen[tb::MG_STATES];
    for (int i = 0; i < tb::MG_STATES; ++i) seen[i] = false;
    int n = 0;
    for (int y = tb::MG_Y_MIN; y <= tb::MG_Y_MAX; ++y) {
        for (int x = tb::MG_X_MIN; x <= tb::MG_X_MAX; ++x) {
            for (int r = 0; r < 4; ++r) {
                const int idx = tb::mgStateIndex(x, y, static_cast<tb::Rot>(r));
                assert(idx >= 0 && idx < tb::MG_STATES);
                assert(!seen[idx]);
                seen[idx] = true;
                ++n;
                int dx = -99, dy = -99;
                tb::Rot dr = tb::ROT_0;
                tb::mgDecodeState(idx, &dx, &dy, &dr);
                assert(dx == x && dy == y && static_cast<int>(dr) == r);
            }
        }
    }
    assert(n == tb::MG_STATES);
}

static void test_mg_state_bounds_reject_out_of_window() {
    assert(tb::mgInStateBounds(0, 0));
    assert(tb::mgInStateBounds(tb::MG_X_MIN, tb::MG_Y_MIN));
    assert(tb::mgInStateBounds(tb::MG_X_MAX, tb::MG_Y_MAX));
    assert(!tb::mgInStateBounds(tb::MG_X_MIN - 1, 0));
    assert(!tb::mgInStateBounds(tb::MG_X_MAX + 1, 0));
    assert(!tb::mgInStateBounds(0, tb::MG_Y_MIN - 1));
    assert(!tb::mgInStateBounds(0, tb::MG_Y_MAX + 1));
    assert(!tb::mgInStateBounds(-100, -100));
    assert(!tb::mgInStateBounds(1000, 1000));
    // The spawn state must be inside the window, or nothing works at all.
    assert(tb::mgInStateBounds(tb::SPAWN_X, tb::SPAWN_Y));
}

static void test_mg_rotation_helpers() {
    assert(tb::rotCW(tb::ROT_0) == tb::ROT_R);
    assert(tb::rotCW(tb::ROT_R) == tb::ROT_2);
    assert(tb::rotCW(tb::ROT_2) == tb::ROT_L);
    assert(tb::rotCW(tb::ROT_L) == tb::ROT_0);
    assert(tb::rotCCW(tb::ROT_0) == tb::ROT_L);
    assert(tb::rotCCW(tb::ROT_L) == tb::ROT_2);
    assert(tb::rotCCW(tb::ROT_2) == tb::ROT_R);
    assert(tb::rotCCW(tb::ROT_R) == tb::ROT_0);
    assert(tb::rot180(tb::ROT_0) == tb::ROT_2);
    assert(tb::rot180(tb::ROT_R) == tb::ROT_L);
    assert(tb::rot180(tb::ROT_2) == tb::ROT_0);
    assert(tb::rot180(tb::ROT_L) == tb::ROT_R);
    assert(tb::isRotateAction(tb::ACT_CW));
    assert(tb::isRotateAction(tb::ACT_CCW));
    assert(tb::isRotateAction(tb::ACT_180));
    assert(!tb::isRotateAction(tb::ACT_LEFT));
    assert(!tb::isRotateAction(tb::ACT_RIGHT));
    assert(!tb::isRotateAction(tb::ACT_SOFT_DROP));
    assert(!tb::isRotateAction(tb::ACT_NONE));
}

// -------------------------------------------------------- movegen: T-spin rules

static void test_mg_cell_occupied_semantics() {
    tb::Board b{};
    b.rows[3] = static_cast<uint16_t>(1u << 7);
    assert(tb::mgCellOccupied(b, 7, 3));            // a filled cell
    assert(!tb::mgCellOccupied(b, 6, 3));           // an empty cell
    assert(tb::mgCellOccupied(b, -1, 5));           // left wall
    assert(tb::mgCellOccupied(b, -100, 5));         // far left wall
    assert(tb::mgCellOccupied(b, 10, 5));           // right wall
    assert(tb::mgCellOccupied(b, 999, 5));          // far right wall
    assert(tb::mgCellOccupied(b, 4, -1));           // floor
    assert(tb::mgCellOccupied(b, 4, -50));          // below the floor
    assert(!tb::mgCellOccupied(b, 4, tb::BOARD_H)); // above the well is EMPTY
    assert(!tb::mgCellOccupied(b, 4, 5000));        // still empty
}

// The 3-corner gate. Two corners is NOT a mini, it is nothing at all -- and it
// stays nothing even when both of them happen to be the front pair.
static void test_mg_classify_three_corner_gate() {
    const char* rows[] = {
        "..........",   // y = 2
        "..........",   // y = 1
        "...#.#....",   // y = 0   filled at x = 3 and x = 5
    };
    const tb::Board b = tb::boardFromAscii(rows, 3);
    // Origin (3, 0) puts the T's centre at (4, 1). Corners (3,2) (5,2) (3,0)
    // (5,0): only the bottom two are filled -> occupied == 2.
    assert(tb::classifyTSpin(b, tb::ROT_0, 3, 0, true, 0) == tb::SPIN_NONE);
    // Same two cells are ROT_2's FRONT pair. Still nothing: the gate is first.
    assert(tb::classifyTSpin(b, tb::ROT_2, 3, 0, true, 0) == tb::SPIN_NONE);
    // The gate runs BEFORE the kick-4 promotion. Hoisting the promotion above
    // it is a one-line edit that every other assert in this file tolerates, so
    // pin the ordering here: two corners plus the promoting kick index is still
    // nothing at all.
    assert(tb::classifyTSpin(b, tb::ROT_0, 3, 0, true, 4) == tb::SPIN_NONE);
    // Zero corners on an empty board.
    const tb::Board empty{};
    assert(tb::classifyTSpin(empty, tb::ROT_0, 4, 0, true, 0) == tb::SPIN_NONE);
}

static void test_mg_classify_requires_rotation_last() {
    const char* rows[] = {
        "..........",   // y = 3
        "...#......",   // y = 2
        "###...####",   // y = 1
        ".###.#####",   // y = 0
    };
    const tb::Board b = tb::boardFromAscii(rows, 4);
    // Origin (3, 0) ROT_2 is the T-spin single. Rotation-last -> full spin.
    assert(tb::classifyTSpin(b, tb::ROT_2, 3, 0, true, 0) == tb::SPIN_FULL);
    // Identical geometry, but the piece slid or fell into place -> no spin.
    assert(tb::classifyTSpin(b, tb::ROT_2, 3, 0, false, 255) == tb::SPIN_NONE);
}

// Walls and the floor count as occupied. Both fixtures sit at EXACTLY three
// corners, one supplied by a wall and one by the floor, so if either stopped
// counting the hard gate would drop that case to SPIN_NONE. Both positions are
// collision-free: this function is only ever called on legal placements, and a
// fixture the piece cannot physically occupy would break the moment someone
// adds the precondition assert this codebase adds everywhere else.
static void test_mg_classify_counts_walls_and_floor() {
    // Left wall. ROT_R origin (-1, 0) puts the centre at (0, 1). Corners:
    // (-1,2) wall, (1,2) empty, (-1,0) wall, (1,0) filled -> occupied == 3.
    // ROT_R's front pair is (1,2) and (1,0) -> front == 1 -> MINI.
    const char* wallRows[] = {
        "..........",   // y = 2
        "..........",   // y = 1
        ".#........",   // y = 0
    };
    const tb::Board wall = tb::boardFromAscii(wallRows, 3);
    assert(tb::classifyTSpin(wall, tb::ROT_R, -1, 0, true, 0) == tb::SPIN_MINI);

    // Floor. ROT_0 origin (0, -1) puts the centre at (1, 0) -- ROT_0 is the one
    // rotation with no bottom-row cell, so y = -1 is legal. Corners: (0,1)
    // filled, (2,1) empty, (0,-1) floor, (2,-1) floor -> occupied == 3.
    // ROT_0's front pair is (0,1) and (2,1) -> front == 1 -> MINI.
    const char* floorRows[] = {
        "..........",   // y = 2
        "#.........",   // y = 1
        "..........",   // y = 0
    };
    const tb::Board floorBoard = tb::boardFromAscii(floorRows, 3);
    assert(tb::classifyTSpin(floorBoard, tb::ROT_0, 0, -1, true, 0) == tb::SPIN_MINI);
}

// One board, one centre, two rotations, two different answers. If the
// front-corner table is transposed or off by one these cannot both pass.
static void test_mg_classify_mini_vs_full_same_centre() {
    const char* rows[] = {
        "..........",   // y = 3
        ".......#..",   // y = 2
        "#####...##",   // y = 1
        ".#####.###",   // y = 0
    };
    const tb::Board b = tb::boardFromAscii(rows, 4);
    // Origin (5, 0) puts the centre at (6, 1). Corners (5,2)'.' (7,2)'#'
    // (5,0)'#' (7,0)'#' -> occupied == 3.
    // ROT_0 front pair = (5,2) and (7,2) -> front == 1 -> MINI.
    assert(tb::classifyTSpin(b, tb::ROT_0, 5, 0, true, 0) == tb::SPIN_MINI);
    // ROT_2 front pair = (5,0) and (7,0) -> front == 2 -> FULL.
    assert(tb::classifyTSpin(b, tb::ROT_2, 5, 0, true, 0) == tb::SPIN_FULL);
    // Promotion is one-directional. This position is already FULL on its own
    // corners; arriving by the fifth kick must leave it FULL, not bump it to
    // some tier above. (The kick-4 test cannot check this -- every position on
    // its board has front == 1, so a kickIndex of 4 there exercises the
    // promotion branch rather than this one.)
    assert(tb::classifyTSpin(b, tb::ROT_2, 5, 0, true, 4) == tb::SPIN_FULL);
    // ROT_L front pair = (5,2) and (5,0) -> front == 1 -> MINI. Without this the
    // whole ROT_L row of the FRONT table is unexercised. Kills the three wrong
    // pairs that read the +1 column: {1,2}, {1,3} and {2,3}.
    assert(tb::classifyTSpin(b, tb::ROT_L, 5, 0, true, 0) == tb::SPIN_MINI);
}

// The other half of the ROT_L check. With exactly three corners filled the front
// pair has one empty member, so a single board can never separate the correct
// pair from all five wrong ones -- whichever corner is empty, some wrong pair
// contains it and scores the same. This board empties the corner the previous
// one fills, and kills the remaining two: {0,1} and {0,3}.
static void test_mg_classify_rot_l_front_pair() {
    const char* rows[] = {
        "...#.#....",   // y = 2
        "..........",   // y = 1
        ".....#....",   // y = 0
    };
    const tb::Board b = tb::boardFromAscii(rows, 3);
    // Origin (3, 0) puts the centre at (4, 1). Corners (3,2)'#' (5,2)'#'
    // (3,0)'.' (5,0)'#' -> occupied == 3.
    // ROT_L front pair = (3,2) and (3,0) -> front == 1 -> MINI.
    assert(tb::classifyTSpin(b, tb::ROT_L, 3, 0, true, 0) == tb::SPIN_MINI);
}

// The promotion branch, isolated: same board, same position, only kickIndex
// differs. This is the STSD case and it is where the bug will be if there is one.
static void test_mg_classify_kick_index_four_promotes() {
    const char* rows[] = {
        "..........",   // y = 5
        "##........",   // y = 4
        "#.........",   // y = 3
        "#.########",   // y = 2
        "#..#######",   // y = 1
        "#..#######",   // y = 0
    };
    const tb::Board b = tb::boardFromAscii(rows, 6);
    // Origin (0, 0) puts the centre at (1, 1). Corners (0,2)'#' (2,2)'#'
    // (0,0)'#' (2,0)'.' -> occupied == 3.
    // ROT_R front pair = (2,2)'#' and (2,0)'.' -> front == 1.
    assert(tb::classifyTSpin(b, tb::ROT_R, 0, 0, true, 0) == tb::SPIN_MINI);
    assert(tb::classifyTSpin(b, tb::ROT_R, 0, 0, true, 3) == tb::SPIN_MINI);
    assert(tb::classifyTSpin(b, tb::ROT_R, 0, 0, true, 4) == tb::SPIN_FULL);
    // 255 means the last rotation was a 180. The five-test index does not
    // apply, so it must never promote.
    assert(tb::classifyTSpin(b, tb::ROT_R, 0, 0, true, 255) == tb::SPIN_MINI);
}

// ------------------------------------------------------- movegen: BFS invariants

// On an empty board every resting state is on the floor, so the placement count
// is exactly the number of (rotation, x) pairs where the piece fits between the
// walls, summed over the four rotations:
//   I  7 + 10 +  7 + 10 = 34      O  9 + 9 + 9 + 9 = 36  (O's four rotation
//   J  8 +  9 +  8 +  9 = 34         states are the same four cells but are
//   L  34   S  34   T  34   Z  34    distinct (x,y,rot) keys, so all four count)
static void test_mg_empty_board_placement_counts() {
    const tb::Board empty{};
    const int expected[tb::NUM_PIECES] = { 34, 34, 34, 36, 34, 34, 34 };
    static tb::MoveList ml;
    for (int pi = 0; pi < tb::NUM_PIECES; ++pi) {
        const tb::PieceType p = static_cast<tb::PieceType>(pi);
        tb::generateMoves(empty, p, &ml);
        if (ml.count != expected[pi])
            std::printf("piece %d: got %d placements, expected %d\n",
                        pi, ml.count, expected[pi]);
        assert(ml.count == expected[pi]);
    }
}

// Two invariants that must hold for EVERY placement of EVERY piece on EVERY
// board: the placement is a legal position, and it cannot fall any further.
static void test_mg_every_placement_is_legal_and_grounded() {
    const char* midRows[] = {
        "..........",
        "..........",
        "#...####..",
        "##..#####.",
        "###.#####.",
        "####.####.",
        "#####.####",
        "######.###",
        "#######.##",
        "########.#",
    };
    const tb::Board boards[2] = { tb::Board{}, tb::boardFromAscii(midRows, 10) };
    static tb::MoveList ml;
    for (int bi = 0; bi < 2; ++bi) {
        for (int pi = 0; pi < tb::NUM_PIECES; ++pi) {
            const tb::PieceType p = static_cast<tb::PieceType>(pi);
            tb::generateMoves(boards[bi], p, &ml);
            assert(ml.count > 0);
            assert(ml.count <= tb::MAX_PLACEMENTS);
            for (int i = 0; i < ml.count; ++i) {
                const tb::Placement& pl = ml.items[i];
                assert(tb::mgInStateBounds(pl.x, pl.y));
                assert(!tb::collides(boards[bi], p, pl.rot, pl.x, pl.y));
                assert(tb::collides(boards[bi], p, pl.rot, pl.x, pl.y - 1));
                assert(pl.pathLen <= tb::MAX_PATH_LEN);
                // All-mini+: any piece may carry a MINI flag, only T can be FULL.
                if (pl.spin == tb::SPIN_FULL) assert(p == tb::PIECE_T);
            }
        }
    }
}

// The rotation-last tie-break exists to keep a spin flag; a rotation that earns no spin
// must not be kept. On an empty board nothing can spin, so no path ends in a rotation.
static void test_mg_no_pointless_final_rotation() {
    static tb::MoveList ml;
    for (int pi = 0; pi < tb::NUM_PIECES; ++pi) {
        const tb::PieceType p = static_cast<tb::PieceType>(pi);
        tb::generateMoves(tb::Board{}, p, &ml);
        for (int i = 0; i < ml.count; ++i) {
            const tb::Placement& pl = ml.items[i];
            assert(pl.spin == tb::SPIN_NONE);
            assert(pl.pathLen > 0);
            assert(!tb::isRotateAction(pl.path[pl.pathLen - 1]));
            assert(!pl.lastWasRotation);
        }
    }
}

// A board whose spawn row is blocked yields no moves rather than garbage.
static void test_mg_topped_out_board_yields_nothing() {
    tb::Board b{};
    for (int y = 0; y < tb::BOARD_H; ++y) b.rows[y] = tb::FULL_ROW;
    static tb::MoveList ml;
    tb::generateMoves(b, tb::PIECE_T, &ml);
    assert(ml.count == 0);
}

// ---------------------------------------------------- movegen: path replay

// Replays a placement's path from spawn using the same primitives the engine
// uses. Returns false if any action in the path is illegal.
static bool mgReplayPath(const tb::Board& b, tb::PieceType p,
                         const tb::Placement& pl,
                         int* outX, int* outY, tb::Rot* outRot,
                         uint8_t* outKick, bool* outLastRot) {
    int x = tb::SPAWN_X, y = tb::SPAWN_Y;
    tb::Rot r = tb::ROT_0;
    uint8_t kick = 255;
    bool lastRot = false;
    if (tb::collides(b, p, r, x, y)) return false;

    for (int i = 0; i < pl.pathLen; ++i) {
        int nx = x, ny = y;
        tb::Rot nr = r;
        uint8_t nk = 255;
        switch (pl.path[i]) {
            case tb::ACT_LEFT:
                nx = x - 1;
                if (tb::collides(b, p, nr, nx, ny)) return false;
                lastRot = false;
                break;
            case tb::ACT_RIGHT:
                nx = x + 1;
                if (tb::collides(b, p, nr, nx, ny)) return false;
                lastRot = false;
                break;
            case tb::ACT_SOFT_DROP:
                ny = y - 1;
                if (tb::collides(b, p, nr, nx, ny)) return false;
                lastRot = false;
                break;
            case tb::ACT_CW:
                nr = tb::rotCW(r);
                if (!tb::tryRotate(b, p, r, nr, x, y, &nx, &ny, &nk)) return false;
                lastRot = true;
                break;
            case tb::ACT_CCW:
                nr = tb::rotCCW(r);
                if (!tb::tryRotate(b, p, r, nr, x, y, &nx, &ny, &nk)) return false;
                lastRot = true;
                break;
            case tb::ACT_180:
                nr = tb::rot180(r);
                if (!tb::tryRotate(b, p, r, nr, x, y, &nx, &ny, &nk)) return false;
                nk = 255;   // 180 never reports a five-test kick index
                lastRot = true;
                break;
            default:
                return false;
        }
        x = nx; y = ny; r = nr; kick = nk;
    }
    *outX = x; *outY = y; *outRot = r; *outKick = kick; *outLastRot = lastRot;
    return true;
}

static void test_mg_every_path_replays_to_its_placement() {
    const char* midRows[] = {
        "..........",
        "..........",
        "#...####..",
        "##..#####.",
        "###.#####.",
        "####.####.",
        "#####.####",
        "######.###",
        "#######.##",
        "########.#",
    };
    const char* tstRows[] = {
        "..........",
        "##........",
        "#.........",
        "#.########",
        "#..#######",
        "#.########",
    };
    const char* tsdRows[] = {
        "..........",
        "...#......",
        "###...####",
        "####.#####",
    };
    const tb::Board boards[4] = {
        tb::Board{},
        tb::boardFromAscii(midRows, 10),
        tb::boardFromAscii(tstRows, 6),
        tb::boardFromAscii(tsdRows, 4),
    };
    static tb::MoveList ml;
    int checked = 0;
    for (int bi = 0; bi < 4; ++bi) {
        for (int pi = 0; pi < tb::NUM_PIECES; ++pi) {
            const tb::PieceType p = static_cast<tb::PieceType>(pi);
            tb::generateMoves(boards[bi], p, &ml);
            for (int i = 0; i < ml.count; ++i) {
                const tb::Placement& pl = ml.items[i];
                int rx = 0, ry = 0;
                tb::Rot rr = tb::ROT_0;
                uint8_t rk = 200;
                bool rlast = false;
                const bool ok = mgReplayPath(boards[bi], p, pl,
                                             &rx, &ry, &rr, &rk, &rlast);
                if (!ok || rx != pl.x || ry != pl.y || rr != pl.rot ||
                    rlast != pl.lastWasRotation || rk != pl.kickIndex) {
                    std::printf("replay mismatch: board %d piece %d "
                                "claimed (x=%d y=%d rot=%d lastRot=%d kick=%u len=%u) "
                                "replayed ok=%d (x=%d y=%d rot=%d lastRot=%d kick=%u)\n",
                                bi, pi, pl.x, pl.y, static_cast<int>(pl.rot),
                                pl.lastWasRotation ? 1 : 0,
                                static_cast<unsigned>(pl.kickIndex),
                                static_cast<unsigned>(pl.pathLen),
                                ok ? 1 : 0, rx, ry, static_cast<int>(rr),
                                rlast ? 1 : 0, static_cast<unsigned>(rk));
                }
                assert(ok);
                assert(rx == pl.x);
                assert(ry == pl.y);
                assert(rr == pl.rot);
                assert(rlast == pl.lastWasRotation);
                assert(rk == pl.kickIndex);
                ++checked;
            }
        }
    }
    // 4 boards x 7 pieces, every one of which has placements.
    assert(checked > 700);
}

// ------------------------------------------------- movegen: rotation tie-break

// A flat floor with a step at x = 2. The T placement at origin (2, 0) ROT_0 is grounded
// on that step and reachable two ways: translation-last in 23 actions (2 lefts + 21
// soft drops), or rotation-last in 25 (rotate at spawn, 2 lefts, 21 drops, rotate
// back). The corners are nowhere near three, so the rotation earns nothing and the
// shortest path must win - it ends in a drop, which the renderer turns into a hard drop.
static void test_mg_shortest_path_wins_when_rotation_earns_no_spin() {
    const char* rows[] = {
        "..........",   // y = 3
        "..........",   // y = 2
        "..........",   // y = 1
        "###.....##",   // y = 0
    };
    const tb::Board b = tb::boardFromAscii(rows, 4);
    static tb::MoveList ml;
    tb::generateMoves(b, tb::PIECE_T, &ml);

    const tb::Placement* found = nullptr;
    for (int i = 0; i < ml.count; ++i) {
        const tb::Placement& pl = ml.items[i];
        if (pl.x == 2 && pl.y == 0 && pl.rot == tb::ROT_0) found = &pl;
    }
    assert(found != nullptr);
    assert(!found->lastWasRotation);
    assert(found->pathLen == 23);
    assert(found->path[found->pathLen - 1] == tb::ACT_SOFT_DROP);
    assert(found->spin == tb::SPIN_NONE);
}

// ------------------------------------------------------ movegen: T-spin fixtures

// Locks a placement onto a copy of the board and returns the lines it clears.
static int mgLinesFor(const tb::Board& b, tb::PieceType p, const tb::Placement& pl) {
    tb::Board c = b;
    tb::lockPiece(c, p, pl.rot, pl.x, pl.y);
    return tb::clearLines(c);
}

// Finds the placement at an exact (x, y, rot), or nullptr.
static const tb::Placement* mgFind(const tb::MoveList& ml, int x, int y, tb::Rot r) {
    for (int i = 0; i < ml.count; ++i) {
        const tb::Placement& pl = ml.items[i];
        if (pl.x == x && pl.y == y && pl.rot == r) return &pl;
    }
    return nullptr;
}

static int mgCountSpins(const tb::MoveList& ml) {
    int n = 0;
    for (int i = 0; i < ml.count; ++i)
        if (ml.items[i].spin != tb::SPIN_NONE) ++n;
    return n;
}

static void test_mg_tspin_single_fixture() {
    const char* rows[] = {
        "..........",   // y = 3
        "...#......",   // y = 2   the overhang that makes it a spin
        "###...####",   // y = 1   gap at x = 3,4,5
        ".###.#####",   // y = 0   gap at x = 0 and x = 4
    };
    const tb::Board b = tb::boardFromAscii(rows, 4);
    static tb::MoveList ml;
    tb::generateMoves(b, tb::PIECE_T, &ml);

    assert(mgCountSpins(ml) >= 1);
    const tb::Placement* pl = mgFind(ml, 3, 0, tb::ROT_2);
    assert(pl != nullptr);
    assert(pl->lastWasRotation);
    assert(pl->spin != tb::SPIN_NONE);
    assert(pl->spin == tb::SPIN_FULL);
    assert(pl->kickIndex == 0);
    assert(mgLinesFor(b, tb::PIECE_T, *pl) == 1);
    // Row y = 1 completes; row y = 0 keeps its hole at x = 0.
}

static void test_mg_tspin_double_fixture() {
    const char* rows[] = {
        "..........",   // y = 3
        "...#......",   // y = 2
        "###...####",   // y = 1   gap at x = 3,4,5
        "####.#####",   // y = 0   gap at x = 4 only -- the one cell that
    };                  //         separates this fixture from the single
    const tb::Board b = tb::boardFromAscii(rows, 4);
    static tb::MoveList ml;
    tb::generateMoves(b, tb::PIECE_T, &ml);

    assert(mgCountSpins(ml) >= 1);
    const tb::Placement* pl = mgFind(ml, 3, 0, tb::ROT_2);
    assert(pl != nullptr);
    assert(pl->lastWasRotation);
    assert(pl->spin == tb::SPIN_FULL);
    assert(pl->kickIndex == 0);
    assert(mgLinesFor(b, tb::PIECE_T, *pl) == 2);
}

// --------------------------------------------- movegen: TST and the kick-4 promotion

static void test_mg_tspin_triple_fixture() {
    const char* rows[] = {
        "..........",   // y = 5
        "##........",   // y = 4   the jut at (1,4) is what defeats kick tests 1 and 2
        "#.........",   // y = 3
        "#.########",   // y = 2
        "#..#######",   // y = 1
        "#.########",   // y = 0
    };
    const tb::Board b = tb::boardFromAscii(rows, 6);
    static tb::MoveList ml;
    tb::generateMoves(b, tb::PIECE_T, &ml);

    assert(mgCountSpins(ml) >= 1);
    const tb::Placement* pl = mgFind(ml, 0, 0, tb::ROT_R);
    assert(pl != nullptr);
    assert(pl->lastWasRotation);
    assert(pl->spin != tb::SPIN_NONE);
    assert(pl->spin == tb::SPIN_FULL);
    // The ONLY way into this state is the last SRS test. Nothing else reaches it.
    assert(pl->kickIndex == 4);
    assert(mgLinesFor(b, tb::PIECE_T, *pl) == 3);
    // All four corners are filled here, so this fixture would classify as FULL
    // even without the promotion rule -- it exercises the kick-4 code path, not
    // the promotion branch. The next test covers the branch.
    assert(tb::classifyTSpin(b, tb::ROT_R, 0, 0, true, 0) == tb::SPIN_FULL);
}

static void test_mg_tspin_kick4_promotion_fixture() {
    const char* rows[] = {
        "..........",   // y = 5
        "##........",   // y = 4
        "#.........",   // y = 3
        "#.########",   // y = 2
        "#..#######",   // y = 1
        "#..#######",   // y = 0   one cell different from the TST fixture
    };
    const tb::Board b = tb::boardFromAscii(rows, 6);
    static tb::MoveList ml;
    tb::generateMoves(b, tb::PIECE_T, &ml);

    assert(mgCountSpins(ml) >= 1);
    const tb::Placement* pl = mgFind(ml, 0, 0, tb::ROT_R);
    assert(pl != nullptr);
    assert(pl->lastWasRotation);
    assert(pl->kickIndex == 4);
    assert(mgLinesFor(b, tb::PIECE_T, *pl) == 2);
    // Corner test alone: 3 corners, front == 1 -> Mini.
    assert(tb::classifyTSpin(b, tb::ROT_R, 0, 0, true, 0) == tb::SPIN_MINI);
    // With the real kick index it is promoted to a proper T-spin double.
    // That is a 400-point Mini becoming a 1200-point spin, which is the whole
    // reason the STSD is worth setting up.
    assert(pl->spin == tb::SPIN_FULL);
}

// ------------------------------------- movegen: mini fixture and negative control

static void test_mg_tspin_mini_fixture() {
    const char* rows[] = {
        "..........",   // y = 3
        ".......#..",   // y = 2   the overhang at (7,2) blocks the ROT_R descent
        "#####...##",   // y = 1   gap at x = 5,6,7
        ".#####.###",   // y = 0   gaps at x = 0 and x = 6
    };
    const tb::Board b = tb::boardFromAscii(rows, 4);
    static tb::MoveList ml;
    tb::generateMoves(b, tb::PIECE_T, &ml);

    // The T falls in ROT_L down the column and rotates CW into ROT_0.
    const tb::Placement* mini = mgFind(ml, 5, 0, tb::ROT_0);
    assert(mini != nullptr);
    assert(mini->lastWasRotation);
    assert(mini->kickIndex == 0);
    assert(mini->spin == tb::SPIN_MINI);
    assert(mgLinesFor(b, tb::PIECE_T, *mini) == 1);

    // Same board, same centre, rotate CCW instead: front corners become the two
    // filled ones and it is a full T-spin single. One board, two answers.
    const tb::Placement* full = mgFind(ml, 5, 0, tb::ROT_2);
    assert(full != nullptr);
    assert(full->lastWasRotation);
    assert(full->kickIndex == 0);
    assert(full->spin == tb::SPIN_FULL);
    assert(mgLinesFor(b, tb::PIECE_T, *full) == 1);

    assert(mgCountSpins(ml) >= 2);
}

static void test_mg_negative_control_no_spins_anywhere() {
    const char* rows[] = {
        "..........",   // y = 2
        "##......##",   // y = 1
        "####..####",   // y = 0
    };
    const tb::Board b = tb::boardFromAscii(rows, 3);
    static tb::MoveList ml;
    tb::generateMoves(b, tb::PIECE_T, &ml);

    assert(ml.count > 0);
    for (int i = 0; i < ml.count; ++i) {
        const tb::Placement& pl = ml.items[i];
        if (pl.spin != tb::SPIN_NONE)
            std::printf("false spin: x=%d y=%d rot=%d spin=%d kick=%u\n",
                        pl.x, pl.y, static_cast<int>(pl.rot),
                        static_cast<int>(pl.spin),
                        static_cast<unsigned>(pl.kickIndex));
        assert(pl.spin == tb::SPIN_NONE);
        assert(mgLinesFor(b, tb::PIECE_T, pl) == 0);
    }
    assert(mgCountSpins(ml) == 0);
}

// ------------------------------------------------ movegen: dedup and invariants

static void test_mg_no_duplicate_placement_keys() {
    const char* midRows[] = {
        "..........",
        "..........",
        "#...####..",
        "##..#####.",
        "###.#####.",
        "####.####.",
        "#####.####",
        "######.###",
        "#######.##",
        "########.#",
    };
    const char* tstRows[] = {
        "..........",
        "##........",
        "#.........",
        "#.########",
        "#..#######",
        "#.########",
    };
    const tb::Board boards[3] = {
        tb::Board{},
        tb::boardFromAscii(midRows, 10),
        tb::boardFromAscii(tstRows, 6),
    };
    static bool seen[tb::MG_STATES];
    static tb::MoveList ml;
    for (int bi = 0; bi < 3; ++bi) {
        for (int pi = 0; pi < tb::NUM_PIECES; ++pi) {
            const tb::PieceType p = static_cast<tb::PieceType>(pi);
            tb::generateMoves(boards[bi], p, &ml);
            for (int i = 0; i < tb::MG_STATES; ++i) seen[i] = false;
            for (int i = 0; i < ml.count; ++i) {
                const tb::Placement& pl = ml.items[i];
                assert(tb::mgInStateBounds(pl.x, pl.y));
                const int key = tb::mgStateIndex(pl.x, pl.y, pl.rot);
                if (seen[key])
                    std::printf("duplicate key: board %d piece %d x=%d y=%d rot=%d\n",
                                bi, pi, pl.x, pl.y, static_cast<int>(pl.rot));
                assert(!seen[key]);
                seen[key] = true;
            }
        }
    }
}

static void test_mg_placement_fields_are_consistent() {
    const char* midRows[] = {
        "..........",
        "..........",
        "#...####..",
        "##..#####.",
        "###.#####.",
        "####.####.",
        "#####.####",
        "######.###",
        "#######.##",
        "########.#",
    };
    const tb::Board boards[2] = { tb::Board{}, tb::boardFromAscii(midRows, 10) };
    static tb::MoveList ml;
    for (int bi = 0; bi < 2; ++bi) {
        for (int pi = 0; pi < tb::NUM_PIECES; ++pi) {
            const tb::PieceType p = static_cast<tb::PieceType>(pi);
            tb::generateMoves(boards[bi], p, &ml);
            assert(ml.count > 0 && ml.count <= tb::MAX_PLACEMENTS);
            for (int i = 0; i < ml.count; ++i) {
                const tb::Placement& pl = ml.items[i];
                // kickIndex is a five-test SRS index, or the 255 sentinel.
                assert(pl.kickIndex <= 4 || pl.kickIndex == 255);
                // A non-255 index means the final action WAS a 90-degree rotation.
                if (pl.kickIndex != 255) assert(pl.lastWasRotation);
                // A non-rotation arrival never carries an index.
                if (!pl.lastWasRotation) assert(pl.kickIndex == 255);
                // A rotation-last path really does end in a rotation.
                if (pl.lastWasRotation) {
                    assert(pl.pathLen > 0);
                    assert(tb::isRotateAction(pl.path[pl.pathLen - 1]));
                }
                // All-spin: any piece but I and O can be a spin, and every spin
                // still requires the last action to have been a rotation.
                if (pl.spin != tb::SPIN_NONE) {
                    assert(p != tb::PIECE_I && p != tb::PIECE_O);
                    assert(pl.lastWasRotation);
                }
                // A non-T spin is always FULL; MINI is a T-only concept.
                if (pl.spin == tb::SPIN_MINI) assert(p == tb::PIECE_T);
                // An empty path means the piece never moved from spawn.
                if (pl.pathLen == 0) {
                    assert(pl.x == tb::SPAWN_X && pl.y == tb::SPAWN_Y &&
                           pl.rot == tb::ROT_0);
                }
                assert(pl.pathLen <= tb::MAX_PATH_LEN);
            }
        }
    }
}

// ---- Plan 3: evaluator -----------------------------------------------------

static tb::Board fixA() {
    const char* rows[] = {
        "..........",
        "#.........",
        "##........",
        "###.......",
        "####......",
        "#####.....",
    };
    return tb::boardFromAscii(rows, 6);
}

static void test_eval_ascii_convention() {
    const char* rows[] = { "##########", ".........." };
    tb::Board b = tb::boardFromAscii(rows, 2);
    // rows[nRows-1] is the BOTTOM row (y == 0); rows[0] is the top of the block.
    assert(b.rows[0] == 0);
    assert(b.rows[1] == tb::FULL_ROW);
    assert(b.rows[2] == 0);
}

static void test_eval_heights() {
    tb::Board empty{};
    tb::Features fe = tb::extractFeatures(empty);
    assert(fe.maxHeight == 0);
    assert(fe.holes == 0);
    assert(fe.coveredCells == 0);
    assert(fe.bumpiness == 0);
    assert(fe.heightPenalty == 0);
    assert(fe.rowTransitions == 0);
    assert(fe.columnTransitions == 0);
    assert(fe.wellDepth == 0);
    assert(fe.tSlotCount == 0);

    tb::Board a = fixA();
    tb::Features fa = tb::extractFeatures(a);
    assert(fa.maxHeight == 5);
    assert(tb::columnHeight(a, 0) == 5);
    assert(tb::columnHeight(a, 4) == 1);
    assert(tb::columnHeight(a, 9) == 0);
}

static tb::Board fixB() {
    const char* rows[] = {
        "..........",
        "####......",
        "#.##......",
        "####......",
        "#.#.......",
    };
    return tb::boardFromAscii(rows, 5);
}

static void test_eval_holes() {
    tb::Features f = tb::extractFeatures(fixB());
    // column 1 is empty at y=0 and y=2 under a height of 4 -> 2 holes
    // column 3 is empty at y=0 under a height of 4          -> 1 hole
    assert(f.holes == 3);
    assert(tb::extractFeatures(fixA()).holes == 0);
}

static void test_eval_covered_cells() {
    tb::Features f = tb::extractFeatures(fixB());
    // column 1: filled cells at y=1 and y=3 both sit above an empty cell -> 2
    // column 3: filled cells at y=1, y=2, y=3 all sit above the empty y=0 -> 3
    assert(f.coveredCells == 5);
    assert(tb::extractFeatures(fixA()).coveredCells == 0);
}

static void test_eval_bumpiness() {
    // FIX_A heights 5,4,3,2,1,0,0,0,0,0 -> 1+1+1+1+1+0+0+0+0 == 5
    assert(tb::extractFeatures(fixA()).bumpiness == 5);
    // FIX_B heights 4,4,4,4,0,0,0,0,0,0 -> 0+0+0+4+0+0+0+0+0 == 4
    assert(tb::extractFeatures(fixB()).bumpiness == 4);
}

static tb::Board fixWell() {
    const char* rows[14];
    for (int i = 0; i < 14; ++i) rows[i] = "#########.";
    return tb::boardFromAscii(rows, 14);
}

static void test_eval_height_penalty() {
    // FIX_A maxHeight 5, under the threshold of 12 -> 0
    assert(tb::extractFeatures(fixA()).heightPenalty == 0);
    // FIX_WELL maxHeight 14, e = 14 - 12 = 2, e*e == 4
    tb::Features fw = tb::extractFeatures(fixWell());
    assert(fw.maxHeight == 14);
    assert(fw.heightPenalty == 4);
}

static void test_eval_row_transitions() {
    // FIX_A rows y=0..4 are "#####.....", "####......", "###.......", "##........", "#........."
    // each is wall->filled (0) ... filled->empty (1) ... empty->wall (2) == 2 per row, 5 rows.
    assert(tb::extractFeatures(fixA()).rowTransitions == 10);
    // FIX_B: y=0 "#.#......."  -> 4
    //        y=1 "####......"  -> 2
    //        y=2 "#.##......"  -> 4
    //        y=3 "####......"  -> 2
    assert(tb::extractFeatures(fixB()).rowTransitions == 12);
}

static void test_eval_column_transitions() {
    // FIX_A, scanned y=0..4:
    //   col0 11111 -> 0, col1 11110 -> 1, col2 11100 -> 1, col3 11000 -> 1, col4 10000 -> 1,
    //   col5 00000 -> 1 (floor->empty), cols 6..9 -> 1 each
    assert(tb::extractFeatures(fixA()).columnTransitions == 9);
    // FIX_B, scanned y=0..3:
    //   col0 1111 -> 0, col1 0101 -> 4, col2 1111 -> 0, col3 0111 -> 2,
    //   col4 0000 -> 1, cols 5..9 -> 1 each
    assert(tb::extractFeatures(fixB()).columnTransitions == 12);
}

static void test_eval_well_depth() {
    // FIX_A is a staircase: every column's neighbour on one side is lower, so no run starts.
    assert(tb::extractFeatures(fixA()).wellDepth == 0);
    // FIX_WELL: column 9 is empty for y=0..13 with column 8 filled and the right wall.
    // d == 14 -> 14*15/2 == 105. No other column starts a run.
    tb::Features fw = tb::extractFeatures(fixWell());
    assert(fw.wellDepth == 105);
    assert(fw.bumpiness == 14);
    assert(fw.holes == 0);
}

static void test_eval_rows_with_holes() {
    // fixB: holes at y=0 (two of them) and y=2 -> two distinct rows
    assert(tb::extractFeatures(fixB()).rowsWithHoles == 2);
    assert(tb::extractFeatures(fixA()).rowsWithHoles == 0);
    const char* rows[] = { "####......", "#..#......", "#.##......" };
    tb::Features f = tb::extractFeatures(tb::boardFromAscii(rows, 3));
    assert(f.holes == 3);
    assert(f.rowsWithHoles == 2);
    // rows above 15 are their own rows, not aliases of the bottom sixteen
    tb::Board tall{};
    tall.rows[18] = 0x1; tall.rows[2] = 0x1;   // holes at (0,0),(0,1),(0,3..17)
    assert(tb::extractFeatures(tall).rowsWithHoles == 17);
}

static void test_eval_overhangs() {
    // A hole is an overhang when a neighbouring column is open down to that row, so a
    // piece can slide in sideways. Enclosed cavities are not.
    const char* open[] = { "##........", "#........." };
    tb::Features f = tb::extractFeatures(tb::boardFromAscii(open, 2));
    assert(f.holes == 1);
    assert(f.overhangs == 1);
    const char* closed[] = { "###.......", "#.#......." };
    f = tb::extractFeatures(tb::boardFromAscii(closed, 2));
    assert(f.holes == 1);
    assert(f.overhangs == 0);
    // neighbour empty but roofed: not reachable from above, not an overhang
    const char* roofed[] = { "####......", "#..#......", "#.##......" };
    f = tb::extractFeatures(tb::boardFromAscii(roofed, 3));
    assert(f.overhangs == 0);
    // fixB: only the hole at (3,0) has an open neighbour (column 4 is empty)
    assert(tb::extractFeatures(fixB()).overhangs == 1);
    assert(tb::extractFeatures(tb::Board{}).overhangs == 0);
}

static void test_eval_t_slots() {
    // A clear T-slot: nub column 4 at y=0, bar at y=1 across columns 3..5,
    // overhang at (3,2). Diagonals filled: (3,0) (5,0) (3,2) -> 3 of 4.
    // Column 4 is open all the way up, so S4 passes.
    const char* clear[] = {
        "..........",
        "####......",
        "###...####",
        "####.#####",
    };
    tb::Board bClear = tb::boardFromAscii(clear, 4);
    assert(tb::countTSlots(bClear) == 1);
    assert(tb::extractFeatures(bClear).tSlotCount == 1);

    // Near miss: identical notch, overhang at (3,2) REMOVED. Only 2 diagonals filled,
    // so S3 fails. This is a plain gap, not a spin slot, and must not count.
    const char* nearMiss[] = {
        "..........",
        "###.......",
        "###...####",
        "####.#####",
    };
    assert(tb::countTSlots(tb::boardFromAscii(nearMiss, 4)) == 0);

    // Covered: identical notch, but (4,2) and (5,2) are filled so the slot is roofed.
    // All four diagonals are filled (S3 passes) but S4 fails: no column of 3,4,5 is open
    // down to y=1. A T can never get in there, so it must not count.
    const char* covered[] = {
        "..........",
        "######....",
        "###...####",
        "####.#####",
    };
    assert(tb::countTSlots(tb::boardFromAscii(covered, 4)) == 0);

    // Boards with no overhangs at all are structurally incapable of holding a T-slot.
    assert(tb::countTSlots(fixA()) == 0);
    assert(tb::countTSlots(fixB()) == 0);
    assert(tb::countTSlots(tb::Board{}) == 0);
}

static void test_eval_dot_product() {
    // All-ones weights turn evaluate() into a plain sum of the features + b2b.
    tb::Weights ones;
    ones.holes = 1.0f; ones.coveredCells = 1.0f; ones.bumpiness = 1.0f;
    ones.maxHeight = 1.0f; ones.heightPenalty = 1.0f; ones.rowTransitions = 1.0f;
    ones.columnTransitions = 1.0f; ones.wellDepth = 1.0f; ones.tSlotCount = 1.0f;
    ones.b2bActive = 1.0f; ones.b2bCharge = 1.0f; ones.attackDealt = 1.0f;
    ones.rowsWithHoles = 1.0f; ones.overhangs = 1.0f;
    ones.plainClear = 1.0f; ones.wastedT = 1.0f;   // move terms: applied by the search, not here

    // FIX_B: holes 3 + covered 5 + bumpiness 4 + maxHeight 4 + heightPenalty 0
    //      + rowTransitions 12 + columnTransitions 12 + wellDepth 0 + tSlots 0
    //      + rowsWithHoles 2 + overhangs 1 == 43
    tb::Board b = fixB();
    assert(std::fabs(tb::evaluate(b, ones, 0) - 43.0f) < 1e-3f);
    // b2bActive adds one unit of its weight; b2bCharge adds surgeCharge(count) units;
    // attackDealt is NOT applied here
    assert(std::fabs(tb::evaluate(b, ones, 1) - 44.0f) < 1e-3f);
    assert(std::fabs(tb::evaluate(b, ones, 4) - 44.0f) < 1e-3f);
    assert(std::fabs(tb::evaluate(b, ones, 5) - 48.0f) < 1e-3f);

    // A single non-zero weight isolates a single feature.
    tb::Weights onlyHoles{};
    onlyHoles.holes = -2.0f;
    assert(std::fabs(tb::evaluate(b, onlyHoles, 0) - (-6.0f)) < 1e-3f);

    // Defaults: an empty board has every feature at 0, so it evaluates to exactly 0.
    tb::Weights d = tb::defaultWeights();
    assert(std::fabs(tb::evaluate(tb::Board{}, d, 0)) < 1e-6f);
    // Sign guards for the plan-6 CE-tuned vector. These are documentation, not laws of
    // nature: the tuner owns the values, and this block exists so the next tuning run is
    // forced to update the story in weights.h when a sign flips.
    assert(d.holes < 0.0f && d.coveredCells < 0.0f);
    assert(d.rowTransitions < 0.0f && d.columnTransitions < 0.0f);
    assert(d.tSlotCount > 0.0f && d.b2bActive > 0.0f && d.b2bCharge > 0.0f && d.attackDealt > 0.0f);
    // overhangs is a partial refund of holes (side-reachable ones are cheap to fix), so it is
    // positive but never outweighs holes itself.
    assert(d.rowsWithHoles < 0.0f);
    assert(d.overhangs > 0.0f && d.overhangs < -d.holes);
    assert(d.plainClear < 0.0f && d.wastedT < 0.0f);
    // The tuner flipped bumpiness (~+2: rowTransitions carries tidiness now) and wellDepth
    // (+3.5: an open well is worth paying for - Cold Clear ships the same sign).
    assert(d.bumpiness > 0.0f && d.wellDepth > 0.0f);

    // maxHeight is positive on purpose and asserted rather than dropped -- an accidental
    // flip back to negative must still fail here. With every health term negative, nothing
    // rewards building: the measured all-negative vector played a 2.68-row pancake with zero
    // tetrises. The pair only makes sense together: maxHeight is the bounded build reward,
    // heightPenalty is the cliff above row 12 that bounds it.
    assert(d.maxHeight > 0.0f);
    assert(d.heightPenalty < 0.0f);
}

static void test_weight_by_name() {
    tb::Weights w = tb::defaultWeights();

    assert(tb::setWeightByName(w, "tSlotCount", 240.0f));
    assert(std::fabs(w.tSlotCount - 240.0f) < 1e-3f);
    assert(tb::setWeightByName(w, "maxHeight", -14.0f));
    assert(std::fabs(w.maxHeight - (-14.0f)) < 1e-3f);
    assert(tb::setWeightByName(w, "attackDealt", 0.0f));
    assert(std::fabs(w.attackDealt) < 1e-6f);
    // untouched fields keep their defaults
    assert(std::fabs(w.holes - tb::defaultWeights().holes) < 1e-3f);

    // unknown names are rejected, not silently ignored
    assert(!tb::setWeightByName(w, "maxheight", 1.0f));
    assert(!tb::setWeightByName(w, "", 1.0f));
    assert(!tb::setWeightByName(w, "tSlotCounts", 1.0f));

    // the name table is exhaustive and every listed name is settable
    assert(tb::weightNameCount() == 16);
    for (int i = 0; i < tb::weightNameCount(); ++i) {
        assert(tb::setWeightByName(w, tb::weightName(i), 1.0f));
    }
    assert(std::fabs(w.holes - 1.0f) < 1e-3f);
    assert(std::fabs(w.wellDepth - 1.0f) < 1e-3f);
    assert(std::fabs(w.b2bActive - 1.0f) < 1e-3f);
    assert(std::fabs(w.b2bCharge - 1.0f) < 1e-3f);

    // out-of-range indices return the empty string, never a null pointer
    assert(tb::weightName(-1)[0] == '\0');
    assert(tb::weightName(tb::weightNameCount())[0] == '\0');

    // weightValue reads back through the same table
    tb::Weights v = tb::defaultWeights();
    assert(tb::setWeightByName(v, "wastedT", -77.0f));
    assert(std::fabs(tb::weightValue(v, 15) - (-77.0f)) < 1e-6f);
    assert(std::fabs(tb::weightValue(v, 0) - tb::defaultWeights().holes) < 1e-6f);
    assert(tb::weightValue(v, -1) == 0.0f && tb::weightValue(v, 16) == 0.0f);
}

// ---- Plan 3: search --------------------------------------------------------

// Wall-clock helper for the search budget tests.
static double msSince(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - t0).count();
}

static void test_search_empty_board() {
    tb::Board b{};
    tb::PieceType queue[tb::PREVIEW_LEN] = {
        tb::PIECE_I, tb::PIECE_O, tb::PIECE_T, tb::PIECE_L, tb::PIECE_J
    };
    tb::SearchConfig cfg;
    assert(cfg.depth == 5);
    assert(cfg.beamWidth == 100);
    assert(std::fabs(cfg.gamma - 0.95f) < 1e-6f);
    // 4.8, not 5.0, and the difference is load-bearing. PRD 4.5 requires the search to
    // COMPLETE inside 5 ms, but the loop breaks when `elapsed > timeBudgetMs` -- always
    // just after the budget, never before. A 5.0 budget therefore puts p99 at 5.045 ms,
    // failing the requirement by construction rather than by being slow. Aiming 0.2 ms
    // low absorbs the overshoot: measured p99 4.850 ms. Kept as an assertion because the
    // WASM layer in milestone 4 inherits this default and the frame budget depends on it.
    assert(std::fabs(cfg.timeBudgetMs - 4.5f) < 1e-6f);

    tb::SearchResult r = tb::search(b, tb::PIECE_T, tb::PIECE_NONE, queue,
                                    tb::PREVIEW_LEN, false, 0, cfg);
    assert(r.valid);
    // the returned placement must be legal and resting on something
    tb::PieceType placed = r.useHold ? queue[0] : tb::PIECE_T;
    assert(!tb::collides(b, placed, r.placement.rot, r.placement.x, r.placement.y));
    assert(tb::collides(b, placed, r.placement.rot, r.placement.x,
                        (int)r.placement.y - 1));
    // and it must be a state the BFS could have produced. NOT `y >= 0`: the origin is the
    // bounding box's lower-left corner, so a piece whose box has an empty bottom row rests
    // BELOW the floor line -- a T in ROT_0 rests at y == -1 and an I in ROT_0 at y == -2
    // (plan 1's convention, plan 2's window MG_Y_MIN == -3). The two assertions above already
    // prove the piece's own cells are on the board and grounded.
    assert(tb::mgInStateBounds(r.placement.x, r.placement.y));
}

static void test_search_is_anytime() {
    // A one-microsecond budget must still produce a legal move: the anytime property is
    // "always has an answer", not "returns a good answer fast".
    tb::Board b{};
    tb::PieceType queue[tb::PREVIEW_LEN] = {
        tb::PIECE_S, tb::PIECE_Z, tb::PIECE_L, tb::PIECE_J, tb::PIECE_T
    };
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 0.001f;
    tb::SearchResult r = tb::search(b, tb::PIECE_T, tb::PIECE_NONE, queue,
                                    tb::PREVIEW_LEN, false, 0, cfg);
    assert(r.valid);
    assert(!tb::collides(b, tb::PIECE_T, r.placement.rot, r.placement.x, r.placement.y));
}

static void test_search_uses_full_depth() {
    // A fixture where no row can ever be completed, so the ONLY thing the evaluator can see is
    // how tall the stack got.
    //   rows 0..3: columns 0..8 filled, column 9 empty  -> four permanently dead cells
    //   rows 4..5: column 9 filled only                 -> the two-row roof that seals them in
    // Rows 0..3 can never be completed: column 9 is unreachable under that roof. Rows 4 and 5
    // already hold column 9, so they need columns 0..8 -- NINE cells. Every piece here is an
    // O, and an O covers exactly two horizontally adjacent columns, so the cells any set of
    // O's contributes to one row is a union of disjoint dominoes and its size is always EVEN.
    // Nine is odd, so rows 4 and 5 can never be completed either. Row 6 and above need all
    // ten columns; columns 0..7 stand at height 4, so an O reaches row 6 there only as the
    // SECOND O on that column pair, making row 6 cost 2*4 + 1 = 9 O's against a queue of 5.
    // (Verified by exhaustive enumeration: zero clears are reachable within five O's.)
    tb::Board b{};
    for (int y = 0; y < 4; ++y) b.rows[y] = (uint16_t)0x1FF;   // columns 0..8
    b.rows[4] = (uint16_t)0x200;                               // column 9 only
    b.rows[5] = (uint16_t)0x200;                               // column 9 only

    tb::PieceType queue[tb::PREVIEW_LEN] = {
        tb::PIECE_O, tb::PIECE_O, tb::PIECE_O, tb::PIECE_O, tb::PIECE_O
    };

    // Every weight zeroed except maxHeight = -1, so a node's score is exactly minus the max
    // column height of its own board. No attack term, no clears, nothing else moves.
    tb::SearchConfig one;
    one.timeBudgetMs = 1e9f;          // no wall-clock cut-off: this test must be deterministic
    one.depth        = 1;
    one.weights      = tb::Weights{};
    one.weights.maxHeight = -1.0f;

    tb::SearchConfig five = one;
    five.depth = 5;

    tb::SearchResult r1 = tb::search(b, tb::PIECE_O, tb::PIECE_NONE, queue,
                                     tb::PREVIEW_LEN, false, 0, one);
    tb::SearchResult r5 = tb::search(b, tb::PIECE_O, tb::PIECE_NONE, queue,
                                     tb::PREVIEW_LEN, false, 0, five);
    assert(r1.valid && r5.valid);

    // One O on this board reaches max height 6 (rows 4-5 on any column pair inside 0..8), which
    // is also the roof's own height, and cannot do better.
    assert(std::fabs(r1.score - (-6.0f)) < 1e-3f);

    // Five O's cannot stay at 6: four of them tile columns 0..7 at rows 4-5, and that leaves
    // column 8 at height 4 with both of its neighbours at 6, so the fifth O rests at row 6
    // whichever pair it takes and the board ends up 8 tall. The deepest completed level's best
    // score is therefore at most -7. A search that returns a maximum taken ACROSS depths
    // reports -6 here, because a depth-0 child beats every deeper one, and it has silently
    // discarded its own lookahead.
    assert(r5.score <= -7.0f + 1e-3f);
    assert(r5.score < r1.score);
}

static void test_search_prefers_hold_when_better() {
    // Rows 0..3 are filled across columns 0..8 with column 9 open: a 4-deep tetris well.
    // The held I clears four lines. The current S clears nothing and can only make the
    // board worse. The search must come back with useHold == true.
    tb::Board b{};
    for (int y = 0; y < 4; ++y) b.rows[y] = (uint16_t)(tb::FULL_ROW & ~(1u << 9));

    tb::PieceType queue[tb::PREVIEW_LEN] = {
        tb::PIECE_O, tb::PIECE_L, tb::PIECE_J, tb::PIECE_Z, tb::PIECE_O
    };
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;   // no wall-clock cut-off, so this test is deterministic

    tb::SearchResult r = tb::search(b, tb::PIECE_S, tb::PIECE_I, queue,
                                    tb::PREVIEW_LEN, false, 0, cfg);
    assert(r.valid);
    assert(r.useHold);
    // and the move it picked really is the I filling the well: locking it clears four rows.
    // (Asserted through the board, not through placement.x, because the piece origin offset
    // for a vertical I is plan 1's business and this test must not encode it.)
    assert(!tb::collides(b, tb::PIECE_I, r.placement.rot, r.placement.x, r.placement.y));
    tb::Board after = b;
    tb::lockPiece(after, tb::PIECE_I, r.placement.rot, r.placement.x, r.placement.y);
    assert(tb::clearLines(after) == 4);

    // With hold empty, the same win is available one piece deeper: the search should take
    // the queue's first piece instead of the S and bank the S in hold.
    tb::PieceType queue2[tb::PREVIEW_LEN] = {
        tb::PIECE_I, tb::PIECE_O, tb::PIECE_L, tb::PIECE_J, tb::PIECE_Z
    };
    tb::SearchResult r2 = tb::search(b, tb::PIECE_S, tb::PIECE_NONE, queue2,
                                     tb::PREVIEW_LEN, false, 0, cfg);
    assert(r2.valid);
    assert(r2.useHold);
    tb::Board after2 = b;
    tb::lockPiece(after2, tb::PIECE_I, r2.placement.rot, r2.placement.x, r2.placement.y);
    assert(tb::clearLines(after2) == 4);
}

static void test_search_plain_clear_penalty() {
    // Bottom row missing only column 9: a vertical I there is a plain (non-B2B) single.
    tb::Board b{};
    b.rows[0] = (uint16_t)(tb::FULL_ROW & ~(1u << 9));
    static tb::MoveList ml;
    tb::generateMoves(b, tb::PIECE_I, &ml);
    bool canClear = false;
    for (int i = 0; i < ml.count; ++i) {
        tb::Board c = b;
        tb::lockPiece(c, tb::PIECE_I, ml.items[i].rot, ml.items[i].x, ml.items[i].y);
        if (tb::clearLines(c) == 1) canClear = true;
    }
    assert(canClear);

    tb::PieceType queue[tb::PREVIEW_LEN] = {
        tb::PIECE_O, tb::PIECE_O, tb::PIECE_O, tb::PIECE_O, tb::PIECE_O
    };
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;
    cfg.depth = 1;
    cfg.weights.plainClear = -1.0e6f;
    tb::SearchResult r = tb::search(b, tb::PIECE_I, tb::PIECE_NONE, queue,
                                    tb::PREVIEW_LEN, 0, 0, cfg);
    assert(r.valid);
    const tb::PieceType placed = r.useHold ? queue[0] : tb::PIECE_I;
    tb::Board after = b;
    tb::lockPiece(after, placed, r.placement.rot, r.placement.x, r.placement.y);
    assert(tb::clearLines(after) == 0);
}

static void test_search_wasted_t() {
    // A flat T on an empty board is a wasted T. With the penalty huge the search banks it
    // in hold and places the I instead.
    tb::Board b{};
    tb::PieceType queue[tb::PREVIEW_LEN] = {
        tb::PIECE_I, tb::PIECE_O, tb::PIECE_L, tb::PIECE_J, tb::PIECE_Z
    };
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;
    cfg.depth = 1;
    cfg.weights.wastedT = -1.0e6f;
    tb::SearchResult r = tb::search(b, tb::PIECE_T, tb::PIECE_NONE, queue,
                                    tb::PREVIEW_LEN, 0, 0, cfg);
    assert(r.valid);
    assert(r.useHold);
}

static void test_search_node_budget() {
    // A node budget cuts the search off deterministically: two runs agree exactly, the count
    // never exceeds the budget by more than one clock-check interval, and depth 0 always
    // completes even when the budget is tiny.
    tb::Board b{};
    for (int y = 0; y < 3; ++y) b.rows[y] = (uint16_t)(tb::FULL_ROW & ~(1u << (y + 2)));
    tb::PieceType queue[tb::PREVIEW_LEN] = {
        tb::PIECE_L, tb::PIECE_S, tb::PIECE_I, tb::PIECE_Z, tb::PIECE_J
    };
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;
    cfg.nodeBudget   = 1500;
    tb::SearchResult a = tb::search(b, tb::PIECE_T, tb::PIECE_O, queue, tb::PREVIEW_LEN, 0, 0, cfg);
    tb::SearchResult c = tb::search(b, tb::PIECE_T, tb::PIECE_O, queue, tb::PREVIEW_LEN, 0, 0, cfg);
    assert(a.valid && c.valid);
    assert(a.nodes == c.nodes);
    assert(a.nodes >= 1500 && a.nodes < 1500 + 64);
    assert(a.placement.x == c.placement.x && a.placement.y == c.placement.y &&
           a.placement.rot == c.placement.rot && a.useHold == c.useHold);
    // unlimited: more nodes, and the count is reported
    cfg.nodeBudget = 0;
    tb::SearchResult u = tb::search(b, tb::PIECE_T, tb::PIECE_O, queue, tb::PREVIEW_LEN, 0, 0, cfg);
    assert(u.nodes > a.nodes);
    // a budget below the root sweep still returns the complete root level
    cfg.nodeBudget = 1;
    tb::SearchResult r = tb::search(b, tb::PIECE_T, tb::PIECE_O, queue, tb::PREVIEW_LEN, 0, 0, cfg);
    assert(r.valid && r.nodes > 1);
}

static void test_search_topout_semantics() {
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;   // deterministic
    tb::PieceType queueO[tb::PREVIEW_LEN] = {
        tb::PIECE_O, tb::PIECE_O, tb::PIECE_O, tb::PIECE_O, tb::PIECE_O
    };

    // (1) A completely full board: the piece cannot spawn, so there is genuinely no legal
    //     placement and valid must be false.
    tb::Board full{};
    for (int y = 0; y < 26; ++y) full.rows[y] = tb::FULL_ROW;
    tb::SearchResult rFull = tb::search(full, tb::PIECE_T, tb::PIECE_NONE, queueO,
                                        tb::PREVIEW_LEN, false, 0, cfg);
    assert(!rFull.valid);

    // (2) Columns 0..7 filled to height 20, columns 8 and 9 open to the floor.
    //     Exactly one O placement stays inside the visible field: the one in the 2-wide well.
    //     Every other O rests on top of the stack and puts cells at rows 20 and 21.
    //     All weights are zeroed so the ONLY thing that can steer the choice is the
    //     above-field penalty.
    tb::Board b{};
    for (int y = 0; y < 20; ++y) b.rows[y] = (uint16_t)0x0FF;   // columns 0..7
    tb::SearchConfig zero;
    zero.timeBudgetMs = 1e9f;
    zero.depth = 1;
    zero.weights = tb::Weights{};   // every weight, including attackDealt, is 0

    tb::SearchResult r = tb::search(b, tb::PIECE_O, tb::PIECE_NONE, queueO,
                                    tb::PREVIEW_LEN, false, 0, zero);
    assert(r.valid);   // an awful board is not a topped-out board
    tb::Board after = b;
    tb::lockPiece(after, tb::PIECE_O, r.placement.rot, r.placement.x, r.placement.y);
    int maxRow = -1;
    for (int y = tb::BOARD_H - 1; y >= 0; --y) { if (after.rows[y] != 0) { maxRow = y; break; } }
    assert(maxRow < tb::VISIBLE_H);
}

static void test_search_time_budget() {
    // A deliberately expensive board: ten rows, each missing a different column. Movegen
    // produces a large candidate set and every child has holes to evaluate.
    tb::Board b{};
    for (int y = 0; y < 10; ++y) b.rows[y] = (uint16_t)(tb::FULL_ROW & ~(1u << y));
    tb::PieceType queue[tb::PREVIEW_LEN] = {
        tb::PIECE_S, tb::PIECE_Z, tb::PIECE_L, tb::PIECE_J, tb::PIECE_I
    };

    tb::SearchConfig cfg;                 // depth 5, width 100, budget 5 ms
    auto t0 = std::chrono::steady_clock::now();
    tb::SearchResult r = tb::search(b, tb::PIECE_T, tb::PIECE_O, queue,
                                    tb::PREVIEW_LEN, true, 0, cfg);
    double ms = msSince(t0);
    assert(r.valid);
    // Overrun is bounded by up to 63 more scored children plus one generateMoves call, so a
    // 5x ceiling is the honest assertion for a test binary. The real p99 check lives in the
    // CLI acceptance run at -O3.
    assert(ms < 25.0);

    // A 1 ms budget must cut the search short but still hand back a legal move.
    cfg.timeBudgetMs = 1.0f;
    t0 = std::chrono::steady_clock::now();
    tb::SearchResult r1 = tb::search(b, tb::PIECE_T, tb::PIECE_O, queue,
                                     tb::PREVIEW_LEN, true, 0, cfg);
    double ms1 = msSince(t0);
    assert(r1.valid);
    assert(ms1 < 10.0);
    assert(ms1 <= ms + 1.0);   // a smaller budget never costs more time
    assert(!tb::collides(b, r1.useHold ? tb::PIECE_O : tb::PIECE_T,
                         r1.placement.rot, r1.placement.x, r1.placement.y));
    // Not an "ok" line -- RUN() prints that. These are the two numbers worth reading.
    std::printf("      budget: full=%.2fms cut=%.2fms\n", ms, ms1);
}

// ---- Plan 3: game ----------------------------------------------------------

static void test_game_steps() {
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;
    tb::Game g(42u, cfg);

    assert(g.piecesPlaced() == 0u);
    assert(g.linesCleared() == 0u);
    assert(g.attackSent() == 0u);
    assert(g.b2bCount() == 0);
    assert(g.comboCount() == 0);
    assert(!g.toppedOut());
    assert(g.holdPiece() == tb::PIECE_NONE);
    assert(g.currentPiece() >= 0 && g.currentPiece() < tb::NUM_PIECES);
    for (int i = 0; i < tb::PREVIEW_LEN; ++i) {
        assert(g.queue()[i] >= 0 && g.queue()[i] < tb::NUM_PIECES);
    }

    for (int i = 0; i < 40; ++i) {
        g.stepPiece();
        assert(!g.toppedOut());
        assert(g.piecesPlaced() == (uint32_t)(i + 1));
        assert(g.lastSearchMs() >= 0.0f);
        // every event this step is a known type
        for (int e = 0; e < g.eventCount(); ++e) {
            assert(g.eventAt(e).type <= tb::GEV_TOPOUT);
        }
        // the board never exceeds the visible field while the game is alive
        for (int y = tb::VISIBLE_H; y < tb::BOARD_H; ++y) assert(g.board().rows[y] == 0);
    }
    assert(g.linesCleared() > 0u);
    // The hold slot is empty only until the first time the search takes its hold branch, and
    // over 40 pieces of seven different types it must take it at least once. If this fires,
    // hold branching is present (Task 13 proves that) but never wins, which plays exactly like
    // not having hold at all. Investigate the search, do not weaken this line.
    assert(g.holdPiece() != tb::PIECE_NONE);

    // reset clears everything but the config
    g.reset(7u);
    assert(g.piecesPlaced() == 0u);
    assert(g.linesCleared() == 0u);
    assert(g.attackSent() == 0u);
    assert(g.maxB2b() == 0);
    assert(g.tSpinCount() == 0u);
    assert(g.holdPiece() == tb::PIECE_NONE);
    assert(tb::isEmpty(g.board()));
}

// Every GEV_B2B_BREAK carries the Surge it released, Game::surgeSent() is their sum, and a
// deterministic 1000-piece run on seed 42 releases at least one.
static void test_game_surge_accounting() {
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;
    // Chain-indifferent weights: the plan-6 vector protects its chain so well that seed 42
    // releases no Surge inside 1000 pieces. This test is about the ACCOUNTING (the event
    // param and surgeSent() sums), so play a bot with no reason to hold the chain.
    cfg.weights.plainClear = 0.0f;
    cfg.weights.wastedT    = 0.0f;
    cfg.weights.b2bActive  = 0.0f;
    cfg.weights.b2bCharge  = 0.0f;
    tb::Game g(42u, cfg);

    uint32_t breaks = 0, surges = 0, sum = 0;
    uint16_t prevB2b = 0;
    for (int i = 0; i < 1000 && !g.toppedOut(); ++i) {
        g.stepPiece();
        for (int e = 0; e < g.eventCount(); ++e) {
            if (g.eventAt(e).type != tb::GEV_B2B_BREAK) continue;
            ++breaks;
            const unsigned p = g.eventAt(e).param;
            assert(p == (unsigned)tb::surgeCharge(prevB2b));
            sum += p;
            if (p > 0) ++surges;
        }
        assert(g.surgeSent() == sum);
        prevB2b = g.b2bCount();
    }
    std::printf("      surge: %u breaks, %u surges, %u lines\n", breaks, surges, sum);
    assert(breaks > 0);
    assert(surges > 0);
}

// Required by the plan's obligation 1 (three-way back-to-back). b2bMaintaining() returns false
// both for "cleared nothing" and for "cleared, but broke the chain" -- it cannot tell them apart,
// so Game must:
//     if (lines == 0)              leave b2bActive EXACTLY as it was
//     else if (b2bMaintaining(ci)) b2bActive = true
//     else                         b2bActive = false
// Writing the obvious `b2bActive = b2bMaintaining(ci)` instead zeroes the chain on every
// non-clearing placement. Building a T-slot takes several of those, so the search would learn
// that setting up a T-spin destroys back-to-back and stop doing it -- and it would look like a
// weight-tuning problem, not a bug.
//
// b2bActive_ is private, but b2bCount_ is a faithful proxy for it: b2bCount_ is set to >= 1
// exactly where b2bActive_ is set true, to 0 exactly where it is set false, and left untouched
// exactly where b2bActive_ is. So `b2bCount() > 0` == b2bActive_ at every observation point.
//
// The budget is unbounded so the run is a fixed, deterministic sequence rather than a
// wall-clock-dependent one: the counts asserted below are pinned facts about seed 42, not
// probabilities.
static void test_game_b2b_survives_non_clearing_pieces() {
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;
    tb::Game g(42u, cfg);

    int      longestQuietRunMidChain = 0;  // consecutive non-clearing placements with a live chain
    int      quietRun   = 0;
    int      easyBreaks = 0;               // easy clears seen ending a live chain
    uint32_t prevLines  = 0;
    uint16_t prevB2b    = 0;

    for (int i = 0; i < 250 && !g.toppedOut(); ++i) {
        g.stepPiece();
        const uint32_t lines = g.linesCleared() - prevLines;
        const uint16_t b2b   = g.b2bCount();

        if (lines == 0) {
            // THE ASSERTION THIS TEST EXISTS FOR: a placement that clears nothing is neutral.
            assert(b2b == prevB2b);
            if (prevB2b > 0) {
                ++quietRun;
                if (quietRun > longestQuietRunMidChain) longestQuietRunMidChain = quietRun;
            }
        } else {
            quietRun = 0;
            // A non-spin clear of fewer than four lines is an easy clear: it must break a live
            // chain. This is the other half of the three-way rule. An All Clear is the
            // exception - it counts as difficult at any line count (plan 5) - and the
            // plan-6 vector actually reaches one inside these 250 pieces.
            bool pc = false;
            for (int e = 0; e < g.eventCount(); ++e)
                if (g.eventAt(e).type == tb::GEV_PERFECT_CLEAR) pc = true;
            if (prevB2b > 0 && g.lastSpin() == tb::SPIN_NONE && lines < 4 && !pc) {
                assert(b2b == 0);
                ++easyBreaks;
            }
        }
        prevLines = g.linesCleared();
        prevB2b   = b2b;
    }

    // The quiet half of the rule must actually have been exercised, or the loop above
    // asserted nothing. "Several" non-clearing pieces mid-chain is the case the bug destroys.
    assert(longestQuietRunMidChain >= 3);

    // The break half needs a bot that is WILLING to break a chain, which the plan-6 vector
    // is paid not to do (plainClear). Replay with chain-indifferent weights: the same rule,
    // exercised by a bot with no reason to protect its b2b.
    tb::SearchConfig cfg2;
    cfg2.timeBudgetMs = 1e9f;
    cfg2.weights.plainClear = 0.0f;
    cfg2.weights.wastedT    = 0.0f;
    cfg2.weights.b2bActive  = 0.0f;
    cfg2.weights.b2bCharge  = 0.0f;
    tb::Game h(42u, cfg2);
    prevLines = 0;
    prevB2b   = 0;
    easyBreaks = 0;
    for (int i = 0; i < 250 && !h.toppedOut(); ++i) {
        h.stepPiece();
        const uint32_t lines = h.linesCleared() - prevLines;
        bool pc = false;
        for (int e = 0; e < h.eventCount(); ++e)
            if (h.eventAt(e).type == tb::GEV_PERFECT_CLEAR) pc = true;
        if (lines > 0 && prevB2b > 0 && h.lastSpin() == tb::SPIN_NONE && lines < 4 && !pc) {
            assert(h.b2bCount() == 0);
            ++easyBreaks;
        }
        prevLines = h.linesCleared();
        prevB2b   = h.b2bCount();
    }
    assert(easyBreaks > 0);
}

static int countBits(uint16_t r) {
    int n = 0;
    for (int x = 0; x < tb::BOARD_W; ++x) n += (r >> x) & 1;
    return n;
}

static void test_game_garbage() {
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;
    tb::Game g(42u, cfg);
    g.setMessiness(0.0f);
    assert(g.garbageReceived() == 0u);

    // Queued garbage enters after the next lock: two nine-cell rows sharing one hole.
    g.queueGarbage(2);
    g.stepPiece();
    assert(g.garbageReceived() == 2u);
    assert(countBits(g.board().rows[0]) == 9);
    assert(g.board().rows[0] == g.board().rows[1]);
    assert(countBits(g.board().rows[2]) + countBits(g.board().rows[3]) +
           countBits(g.board().rows[4]) + countBits(g.board().rows[5]) == 4);   // the piece

    // Same seed, same hole columns.
    tb::Game h(42u, cfg);
    h.setMessiness(0.0f);
    h.queueGarbage(2);
    h.stepPiece();
    assert(h.board().rows[0] == g.board().rows[0]);

    // Attack cancels pending garbage first. Find the first attacking piece on a clean run,
    // then replay with (attack + 1) lines queued in front of it: exactly one row enters.
    tb::Game a(7u, cfg);
    int k = -1, atk = 0;
    for (int i = 0; i < 300 && k < 0; ++i) {
        const uint32_t before = a.attackSent();
        a.stepPiece();
        if (a.attackSent() > before) { k = i; atk = (int)(a.attackSent() - before); }
    }
    assert(k >= 0);
    tb::Game b(7u, cfg);
    b.setMessiness(0.0f);
    for (int i = 0; i < k; ++i) b.stepPiece();
    b.queueGarbage(atk + 1);
    b.stepPiece();
    assert(b.garbageReceived() == 1u);
    assert(countBits(b.board().rows[0]) == 9);

    // reset() drops whatever is pending.
    b.queueGarbage(5);
    b.reset(7u);
    b.stepPiece();
    assert(b.garbageReceived() == 0u);
    assert(countBits(b.board().rows[0]) <= 4);
}

static void test_game_determinism() {
    // The anytime cut-off is wall-clock driven, so identical replay is only guaranteed when
    // the budget is never reached. That is what the huge timeBudgetMs is for.
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;

    tb::Game a(1234u, cfg);
    tb::Game b(1234u, cfg);
    for (int i = 0; i < 200; ++i) {
        a.stepPiece();
        b.stepPiece();
        assert(a.toppedOut() == b.toppedOut());
        if (a.toppedOut()) break;
        const tb::Placement& pa = a.lastPlacement();
        const tb::Placement& pb = b.lastPlacement();
        assert(pa.x == pb.x);
        assert(pa.y == pb.y);
        assert(pa.rot == pb.rot);
        assert(pa.spin == pb.spin);
        assert(pa.pathLen == pb.pathLen);
        assert(a.lastUsedHold() == b.lastUsedHold());
        assert(a.currentPiece() == b.currentPiece());
        assert(a.holdPiece() == b.holdPiece());
        for (int y = 0; y < tb::BOARD_H; ++y) assert(a.board().rows[y] == b.board().rows[y]);
    }
    assert(a.piecesPlaced() == b.piecesPlaced());
    assert(a.linesCleared() == b.linesCleared());
    assert(a.attackSent() == b.attackSent());
    assert(a.tSpinCount() == b.tSpinCount());
    assert(a.maxB2b() == b.maxB2b());

    // A different seed must actually produce a different run, or the bag is not being seeded.
    tb::Game c(99u, cfg);
    for (int i = 0; i < 200; ++i) c.stepPiece();
    assert(c.attackSent() != a.attackSent() || c.linesCleared() != a.linesCleared());
}

static void test_thousand_piece_run() {
    tb::SearchConfig cfg;          // stock defaults: depth 5, width 100, 5 ms budget
    tb::Game g(42u, cfg);
    for (int i = 0; i < 1000; ++i) {
        g.stepPiece();
        if (g.toppedOut()) {
            std::printf("TOPPED OUT after %u pieces\n", g.piecesPlaced());
            assert(false);
        }
    }
    assert(g.piecesPlaced() == 1000u);
    // 1000 pieces is 4000 cells, i.e. 400 rows of material. Anything under 150 lines means the
    // bot is stacking and never cashing in.
    assert(g.linesCleared() > 150u);
    assert(g.attackSent() > 0u);
    // Not an "ok" line -- RUN() prints that. These are the numbers worth reading.
    std::printf("      1000 pieces: lines=%u attack=%u tspins=%u maxb2b=%u\n",
                g.linesCleared(), g.attackSent(), g.tSpinCount(), (unsigned)g.maxB2b());
}

// ---------------------------------------------------------------------------
// bindings/snapshot.h layout. These are the golden numbers the TypeScript side
// learns at runtime via getSnapshotLayout(). If this test fails, JS is reading
// garbage and nothing will look obviously broken (PRD section 12).
// ---------------------------------------------------------------------------
static void test_snapshot_layout() {
    using tb::Snapshot;
    using tb::Event;
    assert(sizeof(Event) == 4);
    assert(alignof(Event) == 2);
    assert(offsetof(Event, type)  == 0);
    assert(offsetof(Event, param) == 1);
    assert(offsetof(Event, frame) == 2);

    assert(sizeof(Snapshot)  == 556);
    assert(alignof(Snapshot) == 4);
    assert(offsetof(Snapshot, frame)        ==   0);
    assert(offsetof(Snapshot, rows)         ==   4);
    assert(offsetof(Snapshot, activePiece)  ==  84);
    assert(offsetof(Snapshot, activeRot)    ==  85);
    assert(offsetof(Snapshot, activeX)      ==  86);
    assert(offsetof(Snapshot, activeY)      ==  87);
    assert(offsetof(Snapshot, ghostY)       ==  88);
    assert(offsetof(Snapshot, pendingSpin)  ==  89);
    assert(offsetof(Snapshot, pathProgress) ==  90);
    assert(offsetof(Snapshot, holdPiece)    ==  91);
    assert(offsetof(Snapshot, queue)        ==  92);
    assert(offsetof(Snapshot, eventCount)   ==  97);
    assert(offsetof(Snapshot, events)       ==  98);
    assert(offsetof(Snapshot, piecesPlaced) == 132);
    assert(offsetof(Snapshot, linesCleared) == 136);
    assert(offsetof(Snapshot, attackSent)   == 140);
    assert(offsetof(Snapshot, b2bCount)     == 144);
    assert(offsetof(Snapshot, comboCount)   == 146);
    assert(offsetof(Snapshot, pps)          == 148);
    assert(offsetof(Snapshot, state)        == 152);
    assert(offsetof(Snapshot, cellPiece)    == 153);
    assert(sizeof(Snapshot::cellPiece)      == 400);
}

// ---------------------------------------------------------------------------
// core/eval.h owns the weight names, the count, and the order. This test pins
// bindings' index->slot bridge to THAT table: for every index, the slot the
// bridge returns must be the field setWeightByName() writes for the same name.
// If plan 3 ever reorders tb::Weights, this fails instead of silently tuning
// the wrong feature through JS.
// ---------------------------------------------------------------------------
static void test_weight_table() {
    assert(tb::weightNameCount() == 16);
    static const char* expected[16] = {
        "holes", "coveredCells", "bumpiness", "maxHeight", "heightPenalty",
        "rowTransitions", "columnTransitions", "wellDepth", "tSlotCount",
        "b2bActive", "attackDealt", "b2bCharge", "rowsWithHoles", "overhangs",
        "plainClear", "wastedT"
    };
    for (int i = 0; i < tb::weightNameCount(); ++i) {
        assert(std::string(tb::weightName(i)) == expected[i]);
    }
    // core/eval.h's out-of-range contract is the empty string, NOT nullptr.
    assert(tb::weightName(-1)[0] == '\0');
    assert(tb::weightName(16)[0] == '\0');

    // The bridge and setWeightByName must agree, index for index.
    for (int i = 0; i < tb::weightNameCount(); ++i) {
        tb::Weights w = tb::defaultWeights();
        const float sentinel = 1234.5f + static_cast<float>(i);
        assert(tb::setWeightByName(w, tb::weightName(i), sentinel));
        const float* slot = tb::bindingsWeightSlot(w, i);
        assert(slot != nullptr);
        assert(*slot == sentinel);
    }
    tb::Weights w = tb::defaultWeights();
    assert(tb::bindingsWeightSlot(w, -1) == nullptr);
    assert(tb::bindingsWeightSlot(w, 16) == nullptr);
    assert(tb::bindingsWeightSlot(w, 0) == &w.holes);        // spot-check the two ends
    assert(tb::bindingsWeightSlot(w, 10) == &w.attackDealt);
}

// ---------------------------------------------------------------------------
// The renderer animates by walking the replayed path states. If the replay does
// not land exactly on the placement the search chose, the piece visibly snaps at
// lock time - the exact "teleport" PRD section 11 forbids.
// ---------------------------------------------------------------------------
static void test_bot_instance_path_replay() {
    tb::BotInstance bot(42, 5.0f, 3, 40);
    const tb::Snapshot* s = bot.snapshotPtr();

    // 40 seconds of simulated wall clock at 60fps -> ~200 pieces at 5 pps.
    int distinctStatesThisPiece = 0;
    uint32_t lastPieces = 0;
    int minDistinct = 999;
    int8_t px = 0, py = 0, pr = 0;
    for (int f = 1; f <= 2400; ++f) {
        bot.tick(f * 16.6667);
        if (s->piecesPlaced != lastPieces) {
            if (lastPieces > 0 && distinctStatesThisPiece < minDistinct)
                minDistinct = distinctStatesThisPiece;
            lastPieces = s->piecesPlaced;
            distinctStatesThisPiece = 0;
        } else if (s->activeX != px || s->activeY != py || s->activeRot != pr) {
            ++distinctStatesThisPiece;
        }
        px = s->activeX; py = s->activeY; pr = s->activeRot;
        assert(s->pathProgress <= 255);
        assert(s->pendingSpin <= 2);
        assert(s->activeRot >= 0 && s->activeRot <= 3);
    }
    std::printf("      path replay: pieces=%u lines=%u attack=%u minStates=%d\n",
                s->piecesPlaced, s->linesCleared, s->attackSent, minDistinct);
    assert(s->piecesPlaced > 150);        // ~200 expected at 5 pps over 40s
    assert(s->state == 1);                // still playing, no top-out
    assert(minDistinct >= 1);             // every piece visibly moves at least once (hover -> landed)
}

// Hard drop preferred: the path's trailing soft drops collapse into one drop, the piece
// hovers while it shifts and turns, then lands in a single frame and rests on its ghost
// for the last quarter of the interval. Soft drops survive only when something follows
// them (a tuck or a spin).
static void test_bot_instance_hard_drops() {
    tb::BotInstance bot(42, 5.0f, 3, 40);
    const tb::Snapshot* s = bot.snapshotPtr();

    uint32_t lastPieces = 0;
    int pieces = 0, landedLast = 0, hardDropped = 0;
    int8_t prevY = 0, lastY = 0, lastGhost = 0;
    bool bigDrop = false, first = true;
    for (int f = 1; f <= 2400; ++f) {
        bot.tick(f * 16.6667);
        if (s->piecesPlaced != lastPieces) {
            if (lastPieces > 0) {
                ++pieces;
                if (lastY == lastGhost) ++landedLast;
                if (bigDrop) ++hardDropped;
            }
            lastPieces = s->piecesPlaced;
            bigDrop = false;
            first = true;
        } else if (!first && s->activeY <= prevY - 6) {
            bigDrop = true;
        }
        first = false;
        prevY = s->activeY;
        lastY = s->activeY;
        lastGhost = s->ghostY;
    }
    std::printf("      hard drops: %d pieces, %d rested on the ghost, %d dropped >= 6 in a frame\n",
                pieces, landedLast, hardDropped);
    assert(pieces > 150);
    assert(landedLast == pieces);
    assert(hardDropped * 2 >= pieces);
}

// ---------------------------------------------------------------------------
// Pacing is UNIFORM. Every placement occupies exactly one piece interval whether
// or not it is a spin. This replaces the old tempo-dilation test: dilation was
// removed because the reference handling does not have it, and this asserts the
// removal stayed removed rather than silently coming back.
// ---------------------------------------------------------------------------
static void test_uniform_pacing() {
    tb::BotInstance bot(42, 20.0f, 4, 60);          // 20 pps -> 50 ms per placement
    const tb::Snapshot* s = bot.snapshotPtr();
    double pieceStart = 0.0;
    uint32_t serial = 0;
    bool sawSpinPiece = false, spinThisPiece = false;
    double longestSpin = 0.0, longestPlain = 0.0;
    int spins = 0, plains = 0;

    for (int f = 1; f <= 3600; ++f) {               // 60s of simulated 60fps
        const double now = f * 16.6667;
        bot.tick(now);
        if (s->pendingSpin != 0) spinThisPiece = true;
        if (s->piecesPlaced != serial) {
            if (serial > 0) {
                const double dur = now - pieceStart;
                if (spinThisPiece) { ++spins; sawSpinPiece = true;
                                     if (dur > longestSpin) longestSpin = dur; }
                else               { ++plains;
                                     if (dur > longestPlain) longestPlain = dur; }
            }
            serial = s->piecesPlaced;
            pieceStart = now;
            spinThisPiece = false;
        }
    }
    std::printf("      uniform pacing: %d spin / %d plain placements, longest %.1f / %.1f ms\n",
                spins, plains, longestSpin, longestPlain);
    assert(sawSpinPiece);              // the bot must actually spin, or this proves nothing
    // 50 ms nominal; one 16.7 ms observation frame of slack on either side.
    assert(longestSpin  <= 70.0);      // a dilated spin would be 200+ ms
    assert(longestPlain <= 70.0);
}

// ---------------------------------------------------------------------------
// tb::Game (the native CLI's driver) and tb::BotInstance (the browser's driver)
// run the same core through two hand-written loops. PRD section 11's criteria
// 1-3 are measured on Game; the browser runs BotInstance. They must stay
// observably identical or those criteria certify code that never ships.
//
// SCOPE: board bits and every counter, piece for piece. NOT the event stream -
// the two deliberately differ there, and bindings/bot_instance.cpp is the
// authority for anything the renderer consumes:
//   * Game pushes GEV_LINE_CLEAR on every clear AND then the specific event;
//     BotInstance emits exactly one clear event per placement. The renderer's
//     calloutText() returns null for LINE_CLEAR, so both render the same.
//   * Game pushes GEV_B2B_EXTEND on the first difficult clear (chain length 1);
//     BotInstance waits for the second, because "BACK-TO-BACK x1" is not a
//     back-to-back. If core/game.cpp is ever reconciled, match BotInstance.
//
// Search is anytime: it returns best-so-far the instant it crosses timeBudgetMs.
// That makes its answer a function of the WALL CLOCK, not just of its inputs, so
// two loops running the same position can pick different placements whenever one
// of them happens to be descheduled. Averages do not save this - a run compares
// 1000 searches, so even a rare hiccup diverges most runs. Measured at the
// shipped 4.8 ms budget this test failed roughly 1 run in 5.
//
// Both sides therefore run with the budget effectively disabled. This test is
// about the two LOOPS being equivalent; test_search_time_budget is what pins the
// anytime behaviour, and it is the only place that should depend on the clock.
// ---------------------------------------------------------------------------
static void test_game_bot_instance_parity() {
    tb::SearchConfig cfg;
    cfg.depth        = 3;
    cfg.beamWidth    = 24;
    cfg.timeBudgetMs = 1.0e9f;

    const uint32_t seed = 42;
    tb::Game        game(seed, cfg);
    tb::BotInstance bot(seed, 20.0f, cfg.depth, cfg.beamWidth);
    bot.setTimeBudget(cfg.timeBudgetMs);
    const tb::Snapshot* s = bot.snapshotPtr();

    const int kPieces = 500;
    double now = 0.0;
    int matched = 0;

    for (int p = 1; p <= kPieces; ++p) {
        game.stepPiece();
        if (game.toppedOut()) break;

        // Run the animation clock forward until BotInstance has locked piece p.
        // At 20 pps a placement is >= 50 ms, so a 16.67 ms tick never straddles
        // two locks; the equality assert below is the tripwire if it ever does.
        int guard = 0;
        while (s->piecesPlaced < (uint32_t)p && s->state != 2 && guard++ < 10000) {
            now += 16.6667;
            bot.tick(now);
        }
        if (s->state == 2) break;
        assert(s->piecesPlaced == (uint32_t)p);

        for (int y = 0; y < tb::BOARD_H; ++y) assert(game.board().rows[y] == s->rows[y]);
        assert(game.linesCleared() == s->linesCleared);
        assert(game.attackSent()   == s->attackSent);
        assert(game.b2bCount()     == s->b2bCount);
        assert(game.comboCount()   == s->comboCount);
        ++matched;
    }

    std::printf("      game/bot parity: %d pieces identical\n", matched);
    assert(matched >= 200);
}

static void test_all_spin_immobile() {
    using namespace tb;
    auto boxed = [](PieceType p, Rot r, int x, int y) {
        Board b{};
        for (int row = 0; row < 6; ++row) b.rows[row] = FULL_ROW;
        const Cell* cs = pieceCells(p, r);
        for (int i = 0; i < 4; ++i) {
            const int cx = x + cs[i].dx;
            const int cy = y + cs[i].dy;
            b.rows[cy] = static_cast<uint16_t>(b.rows[cy] & ~(1u << cx));
        }
        return b;
    };

    const PieceType spinners[6] = { PIECE_I, PIECE_J, PIECE_L, PIECE_O, PIECE_S, PIECE_Z };
    for (PieceType p : spinners) {
        Board b = boxed(p, ROT_0, 3, 1);
        assert(!collides(b, p, ROT_0, 3, 1));
        assert(isImmobile(b, p, ROT_0, 3, 1));
        assert(classifySpin(b, p, ROT_0, 3, 1, true, 0) == SPIN_MINI);
        assert(classifySpin(b, p, ROT_0, 3, 1, false, 0) == SPIN_NONE);
    }

    Board empty{};
    assert(!isImmobile(empty, PIECE_S, ROT_0, 3, 1));
    assert(classifySpin(empty, PIECE_S, ROT_0, 3, 1, true, 0) == SPIN_NONE);

    // Boxed in, T has all four corners: FULL by its own rule, not the immobile MINI.
    Board tb_ = boxed(PIECE_T, ROT_0, 3, 1);
    assert(classifySpin(tb_, PIECE_T, ROT_0, 3, 1, true, 0) == SPIN_FULL);
    assert(classifySpin(tb_, PIECE_T, ROT_0, 3, 1, true, 0)
           == classifyTSpin(tb_, ROT_0, 3, 1, true, 0));

    // Two corners only (the floor pair): nothing until an overhang makes it immobile.
    const char* cavity[] = {
        "....#.....",
        "###...####",
        "###...####",
    };
    Board cav = boardFromAscii(cavity, 3);
    cav.rows[2] = 0;
    assert(!collides(cav, PIECE_T, ROT_0, 3, -1));
    assert(classifyTSpin(cav, ROT_0, 3, -1, true, 0) == SPIN_NONE);
    assert(classifySpin(cav, PIECE_T, ROT_0, 3, -1, true, 0) == SPIN_NONE);
    cav = boardFromAscii(cavity, 3);
    assert(isImmobile(cav, PIECE_T, ROT_0, 3, -1));
    assert(classifyTSpin(cav, ROT_0, 3, -1, true, 0) == SPIN_MINI);
    assert(classifySpin(cav, PIECE_T, ROT_0, 3, -1, true, 0) == SPIN_MINI);
    assert(classifySpin(cav, PIECE_T, ROT_0, 3, -1, false, 0) == SPIN_NONE);
}

// A piece in a rectangular cavity its own size, sky open, can rise: not immobile until
// one cell caps it. (howtotetris diagram 16-2: the J-spin single TETR.IO does not count.)
static void test_immobile_requires_an_overhang() {
    using namespace tb;
    for (int pi = 0; pi < NUM_PIECES; ++pi) {
        const PieceType p = static_cast<PieceType>(pi);
        const Cell* cs = pieceCells(p, ROT_0);
        int x0 = 9, x1 = 0, y0 = 9, y1 = 0;
        for (int i = 0; i < 4; ++i) {
            if (cs[i].dx < x0) x0 = cs[i].dx;
            if (cs[i].dx > x1) x1 = cs[i].dx;
            if (cs[i].dy < y0) y0 = cs[i].dy;
            if (cs[i].dy > y1) y1 = cs[i].dy;
        }
        const int ox = 3, oy = -y0;
        Board b{};
        for (int row = 0; row <= y1 - y0; ++row) {
            uint16_t r = FULL_ROW;
            for (int dx = x0; dx <= x1; ++dx) r = static_cast<uint16_t>(r & ~(1u << (ox + dx)));
            b.rows[row] = r;
        }
        assert(!collides(b, p, ROT_0, ox, oy));
        assert(collides(b, p, ROT_0, ox - 1, oy) && collides(b, p, ROT_0, ox + 1, oy));
        assert(collides(b, p, ROT_0, ox, oy - 1));
        assert(!isImmobile(b, p, ROT_0, ox, oy));

        for (int i = 0; i < 4; ++i) {
            if (cs[i].dy == y1) { b.rows[y1 - y0 + 1] = static_cast<uint16_t>(1u << (ox + cs[i].dx)); break; }
        }
        assert(isImmobile(b, p, ROT_0, ox, oy));
    }

    // The canonical S-spin notch has open sky and still pins the piece: rising drives its
    // top cells into the stack beside the notch. That is why the basic setup needs no overhang.
    const char* notch[] = {
        "##..######",   // y = 1
        "#..#######",   // y = 0
    };
    const Board nb = boardFromAscii(notch, 2);
    assert(!collides(nb, PIECE_S, ROT_0, 1, -1));   // S ROT_0 occupies dy 1..2
    assert(isImmobile(nb, PIECE_S, ROT_0, 1, -1));
}

// ---------------------------------------------------------------------------
// A STARVED SEARCH MUST STILL PICK A PROPERLY EVALUATED MOVE. The root level is
// never interrupted by the clock, so a search whose budget is already blown still
// sweeps every root placement -- and must therefore return exactly what a full
// depth-1 search returns. Before this, an interrupted root left the answer as the
// best of whatever handful of placements happened to be enumerated first, which is
// how the bot topped out on a loaded machine.
// ---------------------------------------------------------------------------
static void test_starved_search_still_sweeps_the_root() {
    tb::Board b{};
    b.rows[0] = 0b0011111111;
    b.rows[1] = 0b0000111111;
    b.rows[2] = 0b0000000111;

    const tb::PieceType queue[5] = { tb::PIECE_I, tb::PIECE_O, tb::PIECE_S,
                                     tb::PIECE_L, tb::PIECE_J };

    tb::SearchConfig starved;
    starved.depth = 5; starved.beamWidth = 100; starved.timeBudgetMs = 0.0f;

    tb::SearchConfig oneDeep;
    oneDeep.depth = 1; oneDeep.beamWidth = 100; oneDeep.timeBudgetMs = 1.0e9f;

    for (int pi = 0; pi < tb::NUM_PIECES; ++pi) {
        const tb::PieceType p = static_cast<tb::PieceType>(pi);
        const tb::SearchResult a = tb::search(b, p, tb::PIECE_NONE, queue, 5, false, 0, starved);
        const tb::SearchResult c = tb::search(b, p, tb::PIECE_NONE, queue, 5, false, 0, oneDeep);
        assert(a.valid);
        assert(c.valid);
        assert(a.useHold == c.useHold);
        assert(a.placement.x == c.placement.x);
        assert(a.placement.y == c.placement.y);
        assert(a.placement.rot == c.placement.rot);
    }
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
    RUN(test_lockPiece_sets_exactly_four_bits);
    RUN(test_clearLines_removes_none_when_no_row_is_full);
    RUN(test_clearLines_single_row_shifts_above_down_by_one);
    RUN(test_clearLines_two_non_adjacent_rows);
    RUN(test_clearLines_four_rows_is_a_tetris);
    RUN(test_board_add_garbage);
    RUN(test_dropY_falls_to_the_floor_on_an_empty_board);
    RUN(test_dropY_lands_on_a_stack);
    RUN(test_dropY_is_idempotent_at_rest);
    RUN(test_columnHeight);
    RUN(test_columnHeight_counts_over_a_hole);
    RUN(test_isEmpty);
    RUN(test_kick_tables_first_test_is_identity);
    RUN(test_kick_table_jlstz_duplicate_structure);
    RUN(test_kick_table_i_duplicate_structure);
    RUN(test_kick_table_spot_checks);
    RUN(test_kick_180_every_offset_is_within_one_column_and_two_rows);
    RUN(test_kick_case1_tspin_triple);
    RUN(test_kick_case2_s_piece);
    RUN(test_kick_case3_z_piece_mirrors_case2);
    RUN(test_kick_case4_i_piece_asserts_test_order);
    RUN(test_rotate_o_never_kicks);
    RUN(test_rotate_rejected_when_every_test_collides);
    RUN(test_rotate_to_the_same_rotation_is_rejected);
    RUN(test_kick_case5_180_floor_kick);
    RUN(test_180_in_open_space_uses_test_zero);
    RUN(test_180_i_piece_has_exactly_one_test);
    RUN(test_180_i_piece_is_rejected_when_its_only_test_collides);
    RUN(test_xorshift32_is_deterministic_and_never_zero);
    RUN(test_bag_every_seven_pieces_is_a_permutation);
    RUN(test_bag_same_seed_same_sequence);
    RUN(test_bag_different_seeds_diverge);
    RUN(test_bag_reset_replays_the_same_sequence);
    RUN(test_bag_seed_zero_is_remapped_and_still_shuffles);
    RUN(test_attack_prd_table_without_b2b_or_combo);
    RUN(test_attack_b2b_bonus_is_applied);
    RUN(test_attack_b2b_bonus_is_not_applied);
    RUN(test_b2bMaintaining_rules);
    RUN(test_b2b_is_maintained_by_a_mini_that_clears_lines);
    RUN(test_attack_combo_multiplier);
    RUN(test_attack_combo_log_floor_for_zero_base);
    RUN(test_attack_combo_multiplier_is_unbounded);
    RUN(test_attack_all_clear_adds_five_and_holds_b2b);
    RUN(test_surge_charge_by_chain_length);
    RUN(test_attack_surge_is_sent_on_break);
    RUN(test_attack_is_zero_when_no_lines_clear);
    RUN(test_mg_t_center_is_origin_plus_one_one);
    RUN(test_mg_cell_offsets_fit_zero_to_three);
    RUN(test_mg_tryRotate_supports_180);
    RUN(test_mg_tryRotate_o_always_succeeds);
    RUN(test_mg_state_index_roundtrips);
    RUN(test_mg_state_bounds_reject_out_of_window);
    RUN(test_mg_rotation_helpers);
    RUN(test_mg_cell_occupied_semantics);
    RUN(test_mg_classify_three_corner_gate);
    RUN(test_mg_classify_requires_rotation_last);
    RUN(test_mg_classify_counts_walls_and_floor);
    RUN(test_mg_classify_mini_vs_full_same_centre);
    RUN(test_mg_classify_rot_l_front_pair);
    RUN(test_mg_classify_kick_index_four_promotes);
    RUN(test_mg_empty_board_placement_counts);
    RUN(test_mg_every_placement_is_legal_and_grounded);
    RUN(test_mg_topped_out_board_yields_nothing);
    RUN(test_mg_every_path_replays_to_its_placement);
    RUN(test_mg_shortest_path_wins_when_rotation_earns_no_spin);
    RUN(test_mg_tspin_single_fixture);
    RUN(test_mg_tspin_double_fixture);
    RUN(test_mg_tspin_triple_fixture);
    RUN(test_mg_tspin_kick4_promotion_fixture);
    RUN(test_mg_tspin_mini_fixture);
    RUN(test_mg_negative_control_no_spins_anywhere);
    RUN(test_mg_no_duplicate_placement_keys);
    RUN(test_mg_placement_fields_are_consistent);
    RUN(test_eval_ascii_convention);
    RUN(test_eval_heights);
    RUN(test_eval_holes);
    RUN(test_eval_covered_cells);
    RUN(test_eval_bumpiness);
    RUN(test_eval_height_penalty);
    RUN(test_eval_row_transitions);
    RUN(test_eval_column_transitions);
    RUN(test_eval_well_depth);
    RUN(test_eval_rows_with_holes);
    RUN(test_eval_overhangs);
    RUN(test_eval_t_slots);
    RUN(test_eval_dot_product);
    RUN(test_weight_by_name);
    RUN(test_search_empty_board);
    RUN(test_search_is_anytime);
    RUN(test_search_uses_full_depth);
    RUN(test_search_prefers_hold_when_better);
    RUN(test_search_plain_clear_penalty);
    RUN(test_search_wasted_t);
    RUN(test_search_node_budget);
    RUN(test_search_topout_semantics);
    RUN(test_search_time_budget);
    RUN(test_game_steps);
    RUN(test_game_b2b_survives_non_clearing_pieces);
    RUN(test_game_surge_accounting);
    RUN(test_game_determinism);
    RUN(test_game_garbage);
    RUN(test_thousand_piece_run);
    RUN(test_snapshot_layout);
    RUN(test_weight_table);
    RUN(test_bot_instance_path_replay);
    RUN(test_all_spin_immobile);
    RUN(test_bot_instance_hard_drops);
    RUN(test_mg_no_pointless_final_rotation);
    RUN(test_immobile_requires_an_overhang);
    RUN(test_starved_search_still_sweeps_the_root);
    RUN(test_uniform_pacing);
    RUN(test_game_bot_instance_parity);
    std::printf("all %d tests passed\n", g_testCount);
    return 0;
}
