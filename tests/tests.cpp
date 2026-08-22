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
#include "core/srs_tables.h"
#include "core/srs.h"
#include "core/rng.h"
#include "core/attack.h"
#include "core/movegen.h"

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
    assert(tb::computeAttack(ci(1, tb::SPIN_MINI, false), false, 0) == 0);   // t-spin mini
    assert(tb::computeAttack(ci(2, tb::SPIN_MINI, false), false, 0) == 0);   // t-spin mini
    assert(tb::computeAttack(ci(1, tb::SPIN_FULL, false), false, 0) == 2);   // t-spin single
    assert(tb::computeAttack(ci(2, tb::SPIN_FULL, false), false, 0) == 4);   // t-spin double
    assert(tb::computeAttack(ci(3, tb::SPIN_FULL, false), false, 0) == 6);   // t-spin triple
}

static void test_attack_b2b_bonus_is_applied() {
    // +1, and only when the clear is itself b2b-maintaining AND the chain was
    // already live before this clear.
    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, false), true, 0) == 5);   // tetris
    assert(tb::computeAttack(ci(1, tb::SPIN_FULL, false), true, 0) == 3);   // tss
    assert(tb::computeAttack(ci(2, tb::SPIN_FULL, false), true, 0) == 5);   // tsd
    assert(tb::computeAttack(ci(3, tb::SPIN_FULL, false), true, 0) == 7);   // tst
    assert(tb::computeAttack(ci(1, tb::SPIN_MINI, false), true, 0) == 1);   // mini single
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

static void test_attack_combo_table_indexing() {
    // Combo bonus {0,0,1,1,1,2,2,3,3,4,4,4,5}, indexed by the number of
    // consecutive PRIOR clears. A double has base 1, so expect base + bonus.
    const int bonus[13] = {0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5};
    for (int combo = 0; combo < 13; ++combo)
        assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), false, combo) == 1 + bonus[combo]);
    // A negative count is clamped to the first entry.
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), false, -1) == 1);
}

static void test_attack_combo_clamps_at_the_last_entry() {
    const int atTwelve = tb::computeAttack(ci(2, tb::SPIN_NONE, false), false, 12);
    assert(atTwelve == 6);   // base 1 + bonus 5
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), false, 13) == atTwelve);
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), false, 50) == atTwelve);
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), false, 100000) == atTwelve);
}

static void test_attack_perfect_clear_adds_ten() {
    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, true), false, 0) == 14);
    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, true), false, 0) == 10);
    // Stacks with the b2b bonus and the combo bonus.
    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, true), true, 5) == 4 + 1 + 2 + 10);
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
    RUN(test_attack_combo_table_indexing);
    RUN(test_attack_combo_clamps_at_the_last_entry);
    RUN(test_attack_perfect_clear_adds_ten);
    RUN(test_attack_is_zero_when_no_lines_clear);
    RUN(test_mg_t_center_is_origin_plus_one_one);
    RUN(test_mg_cell_offsets_fit_zero_to_three);
    RUN(test_mg_tryRotate_supports_180);
    RUN(test_mg_tryRotate_o_always_succeeds);
    RUN(test_mg_state_index_roundtrips);
    RUN(test_mg_state_bounds_reject_out_of_window);
    RUN(test_mg_rotation_helpers);
    std::printf("all %d tests passed\n", g_testCount);
    return 0;
}
