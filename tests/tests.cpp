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
    std::printf("all %d tests passed\n", g_testCount);
    return 0;
}
