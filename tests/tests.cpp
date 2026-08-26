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
#include "core/pc.h"
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

    assert(sizeof(tb::Board) == 80);
    tb::Board b{};
    for (int y = 0; y < tb::BOARD_H; ++y) assert(b.rows[y] == 0);
}

static bool hasCell(const tb::Cell* cells, int dx, int dy) {
    for (int i = 0; i < 4; ++i)
        if (cells[i].dx == dx && cells[i].dy == dy) return true;
    return false;
}

static int boxSize(tb::PieceType p) {
    if (p == tb::PIECE_I) return 4;
    if (p == tb::PIECE_O) return 2;
    return 3;
}

static void test_piece_cells_spawn_shapes() {

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

static void test_boardFromAscii_maps_first_row_to_top() {

    const char* rows[] = {
        "..........",
        "#........#",
        ".#########",
    };
    const tb::Board b = tb::boardFromAscii(rows, 3);
    assert(b.rows[0] == 0x3FE);
    assert(b.rows[1] == 0x201);
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

    assert(!tb::collides(empty, tb::PIECE_O, tb::ROT_0, 0, 0));
    assert(tb::collides(empty, tb::PIECE_O, tb::ROT_0, -1, 0));

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

    assert(!tb::collides(empty, tb::PIECE_O, tb::ROT_0, 4, 0));
    assert(tb::collides(empty, tb::PIECE_O, tb::ROT_0, 4, -1));

    assert(!tb::collides(empty, tb::PIECE_T, tb::ROT_0, 3, -1));
    assert(tb::collides(empty, tb::PIECE_T, tb::ROT_0, 3, -2));

    assert(!tb::collides(empty, tb::PIECE_I, tb::ROT_0, 3, tb::BOARD_H - 3));
    assert(tb::collides(empty, tb::PIECE_I, tb::ROT_0, 3, tb::BOARD_H - 2));
}

static void test_collides_existing_stack() {
    const char* rows[] = {
        "....#.....",
        "##########",
    };
    const tb::Board b = tb::boardFromAscii(rows, 2);
    assert(tb::collides(b, tb::PIECE_O, tb::ROT_0, 4, 0));
    assert(tb::collides(b, tb::PIECE_O, tb::ROT_0, 4, 1));
    assert(!tb::collides(b, tb::PIECE_O, tb::ROT_0, 6, 1));
    assert(!tb::collides(b, tb::PIECE_O, tb::ROT_0, 4, 2));
}

static int popcount16(uint16_t v) {
    int n = 0;
    for (int i = 0; i < 16; ++i) n += (v >> i) & 1;
    return n;
}

static void test_lockPiece_sets_exactly_four_bits() {
    tb::Board b{};
    tb::lockPiece(b, tb::PIECE_T, tb::ROT_0, 3, 0);

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
        "#.........",
        "##########",
        "..#.......",
    };
    tb::Board b = tb::boardFromAscii(rows, 3);
    assert(tb::clearLines(b) == 1);
    assert(b.rows[0] == 0x004);
    assert(b.rows[1] == 0x001);
    assert(b.rows[2] == 0x000);
    assert(b.rows[tb::BOARD_H - 1] == 0x000);
}

static void test_clearLines_two_non_adjacent_rows() {
    const char* rows[] = {
        ".....#....",
        "##########",
        "....#.....",
        "##########",
        "#.........",
    };
    tb::Board b = tb::boardFromAscii(rows, 5);
    assert(tb::clearLines(b) == 2);
    assert(b.rows[0] == 0x001);
    assert(b.rows[1] == 0x010);
    assert(b.rows[2] == 0x020);
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

static void test_board_add_garbage() {
    const char* rows[] = { "#.........", "##........" };
    tb::Board b = tb::boardFromAscii(rows, 2);
    b.rows[tb::BOARD_H - 1] = 0x1;
    tb::addGarbage(b, 2, 3);
    assert(b.rows[0] == (tb::FULL_ROW & ~(1u << 3)));
    assert(b.rows[1] == (tb::FULL_ROW & ~(1u << 3)));
    assert(b.rows[2] == 0x3);
    assert(b.rows[3] == 0x1);
    assert(b.rows[4] == 0);
    assert(b.rows[tb::BOARD_H - 1] == 0);
    tb::Board c = b;
    tb::addGarbage(c, 0, 5);
    for (int y = 0; y < tb::BOARD_H; ++y) assert(c.rows[y] == b.rows[y]);
}

static void test_dropY_falls_to_the_floor_on_an_empty_board() {
    const tb::Board empty{};

    assert(tb::dropY(empty, tb::PIECE_O, tb::ROT_0, 4, 21) == 0);

    assert(tb::dropY(empty, tb::PIECE_T, tb::ROT_0, 3, 21) == -1);
}

static void test_dropY_lands_on_a_stack() {
    const char* rows[] = {
        "#.........",
        "#.........",
        "#.........",
    };
    const tb::Board b = tb::boardFromAscii(rows, 3);

    assert(tb::dropY(b, tb::PIECE_O, tb::ROT_0, 0, 21) == 3);

    assert(tb::dropY(b, tb::PIECE_O, tb::ROT_0, 4, 21) == 0);
}

static void test_dropY_is_idempotent_at_rest() {
    const tb::Board empty{};
    const int y = tb::dropY(empty, tb::PIECE_O, tb::ROT_0, 4, 21);
    assert(tb::dropY(empty, tb::PIECE_O, tb::ROT_0, 4, y) == y);
}

static void test_columnHeight() {
    const char* rows[] = {
        "#.........",
        "#.........",
        "#.#.......",
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
        "#.........",
        "..........",
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
    assert(!tb::isEmpty(b));
    b = tb::Board{};
    assert(tb::isEmpty(b));
}

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

    assert(kickEq(tb::KICKS_JLSTZ[0][4], -1, -2));

    assert(kickEq(tb::KICKS_JLSTZ[7][4], 1, -2));

    assert(kickEq(tb::KICKS_JLSTZ[4][4], 1, -2));

    assert(kickEq(tb::KICKS_JLSTZ[5][4], -1, 2));

    assert(kickEq(tb::KICKS_I[0][3], -2, -1));
    assert(kickEq(tb::KICKS_I[0][4], 1, 2));

    assert(kickEq(tb::KICKS_I[2][4], 2, -1));

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

static void expectKickTestCollides(const tb::Board& b, tb::PieceType p, tb::Rot to,
                                   int x, int y, int dx, int dy) {
    assert(tb::collides(b, p, to, x + dx, y + dy));
}

static void test_kick_case1_tspin_triple() {

    const char* rows[] = {
        ".....#####",
        "......####",
        "#####.####",
        "####..####",
        "#####.####",
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

    assert(tb::collides(b, tb::PIECE_T, tb::ROT_L, ox, oy - 1));
    tb::lockPiece(b, tb::PIECE_T, tb::ROT_L, ox, oy);
    assert(tb::clearLines(b) == 3);
}

static void test_kick_case2_s_piece() {

    const char* rows[] = {
        "#####.....",
        "####......",
        "####.####.",
        "####..####",
        "#####.####",
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

    assert(tb::collides(b, tb::PIECE_S, tb::ROT_R, ox, oy - 1));
    tb::lockPiece(b, tb::PIECE_S, tb::ROT_R, ox, oy);
    assert(tb::clearLines(b) == 2);
}

static void test_kick_case3_z_piece_mirrors_case2() {

    const char* rows[] = {
        ".....#####",
        "......####",
        ".####.####",
        "####..####",
        "####.#####",
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

    assert(tb::collides(b, tb::PIECE_Z, tb::ROT_L, ox, oy - 1));
    tb::lockPiece(b, tb::PIECE_Z, tb::ROT_L, ox, oy);
    assert(tb::clearLines(b) == 2);
}

static void test_kick_case4_i_piece_asserts_test_order() {

    const char* rows[] = {
        "....#.....",
        "..........",
        "####.#####",
        "####.#####",
        "####.#####",
        "#########.",
    };
    tb::Board b = tb::boardFromAscii(rows, 6);
    assert(!tb::collides(b, tb::PIECE_I, tb::ROT_0, 4, 2));
    expectKickTestCollides(b, tb::PIECE_I, tb::ROT_R, 4, 2, 0, 0);
    expectKickTestCollides(b, tb::PIECE_I, tb::ROT_R, 4, 2, -2, 0);
    expectKickTestCollides(b, tb::PIECE_I, tb::ROT_R, 4, 2, 1, 0);
    assert(!tb::collides(b, tb::PIECE_I, tb::ROT_R, 4 + 1, 2 + 2));

    int ox = -99, oy = -99;
    uint8_t k = 255;
    assert(tb::tryRotate(b, tb::PIECE_I, tb::ROT_0, tb::ROT_R, 4, 2, &ox, &oy, &k));
    assert(ox == 2);
    assert(oy == 1);
    assert(k == 3);

    assert(tb::collides(b, tb::PIECE_I, tb::ROT_R, ox, oy - 1));
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
            assert(ox == 4);
            assert(oy == 3);
            assert(k == 0);
        }
    }
}

static void test_rotate_rejected_when_every_test_collides() {

    const char* rows[] = {
        "##.#######",
        "#...######",
        "##########",
        "##########",
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

static void test_kick_case5_180_floor_kick() {

    const tb::Board empty{};
    assert(!tb::collides(empty, tb::PIECE_T, tb::ROT_0, 3, -1));
    expectKickTestCollides(empty, tb::PIECE_T, tb::ROT_2, 3, -1, 0, 0);

    int ox = -99, oy = -99;
    uint8_t k = 255;
    assert(tb::tryRotate(empty, tb::PIECE_T, tb::ROT_0, tb::ROT_2, 3, -1, &ox, &oy, &k));
    assert(ox == 3);
    assert(oy == 0);
    assert(k == 1);

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

    const char* rows[] = {
        "..........",
        "##########",
        "..........",
    };
    const tb::Board b = tb::boardFromAscii(rows, 3);
    assert(!tb::collides(b, tb::PIECE_I, tb::ROT_0, 3, 0));
    assert(tb::collides(b, tb::PIECE_I, tb::ROT_2, 3, 0));

    int ox = -99, oy = -99;
    uint8_t k = 255;
    assert(!tb::tryRotate(b, tb::PIECE_I, tb::ROT_0, tb::ROT_2, 3, 0, &ox, &oy, &k));
    assert(ox == -99 && oy == -99 && k == 255);
}

static void test_xorshift32_is_deterministic_and_never_zero() {
    uint32_t a = 12345;
    uint32_t b = 12345;
    for (int i = 0; i < 1000; ++i) {
        const uint32_t va = tb::xorshift32(a);
        const uint32_t vb = tb::xorshift32(b);
        assert(va == vb);
        assert(va != 0);
        assert(a == va);
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

static void test_bag_remaining_mask() {
    tb::Bag bag(123u);
    assert(bag.remainingMask() == 0);  // pre-refill: nothing pending

    const tb::PieceType first = bag.next();
    const uint8_t m = bag.remainingMask();
    assert(__builtin_popcount(m) == 6);
    assert((m & (1u << first)) == 0);

    for (int i = 0; i < 6; ++i) bag.next();
    assert(bag.remainingMask() == 0);  // bag exhausted again
}

static tb::ClearInfo ci(int lines, tb::SpinKind spin, bool perfectClear) {
    tb::ClearInfo c;
    c.lines = static_cast<uint8_t>(lines);
    c.spin = spin;
    c.perfectClear = perfectClear;
    return c;
}

static void test_attack_prd_table_without_b2b_or_combo() {

    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, false), false, 0) == 0);
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), false, 0) == 1);
    assert(tb::computeAttack(ci(3, tb::SPIN_NONE, false), false, 0) == 2);
    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, false), false, 0) == 4);
    assert(tb::computeAttack(ci(1, tb::SPIN_MINI, false), false, 0) == 0);
    assert(tb::computeAttack(ci(2, tb::SPIN_MINI, false), false, 0) == 1);
    assert(tb::computeAttack(ci(3, tb::SPIN_MINI, false), false, 0) == 2);
    assert(tb::computeAttack(ci(4, tb::SPIN_MINI, false), false, 0) == 4);
    assert(tb::computeAttack(ci(1, tb::SPIN_FULL, false), false, 0) == 2);
    assert(tb::computeAttack(ci(2, tb::SPIN_FULL, false), false, 0) == 4);
    assert(tb::computeAttack(ci(3, tb::SPIN_FULL, false), false, 0) == 6);
    assert(tb::computeAttack(ci(4, tb::SPIN_FULL, false), false, 0) == 10);
}

static void test_attack_b2b_bonus_is_applied() {

    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, false), true, 0) == 5);
    assert(tb::computeAttack(ci(1, tb::SPIN_FULL, false), true, 0) == 3);
    assert(tb::computeAttack(ci(2, tb::SPIN_FULL, false), true, 0) == 5);
    assert(tb::computeAttack(ci(3, tb::SPIN_FULL, false), true, 0) == 7);
    assert(tb::computeAttack(ci(1, tb::SPIN_MINI, false), true, 0) == 1);
    assert(tb::computeAttack(ci(2, tb::SPIN_MINI, false), true, 0) == 2);
}

static void test_attack_b2b_bonus_is_not_applied() {

    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, false), true, 0) == 0);
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), true, 0) == 1);
    assert(tb::computeAttack(ci(3, tb::SPIN_NONE, false), true, 0) == 2);

    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, false), false, 0) == 4);
    assert(tb::computeAttack(ci(2, tb::SPIN_FULL, false), false, 0) == 4);
}

static void test_b2bMaintaining_rules() {
    assert(tb::b2bMaintaining(ci(4, tb::SPIN_NONE, false)));
    assert(tb::b2bMaintaining(ci(1, tb::SPIN_FULL, false)));
    assert(tb::b2bMaintaining(ci(2, tb::SPIN_FULL, false)));
    assert(tb::b2bMaintaining(ci(3, tb::SPIN_FULL, false)));
    assert(!tb::b2bMaintaining(ci(1, tb::SPIN_NONE, false)));
    assert(!tb::b2bMaintaining(ci(2, tb::SPIN_NONE, false)));
    assert(!tb::b2bMaintaining(ci(3, tb::SPIN_NONE, false)));

    assert(!tb::b2bMaintaining(ci(0, tb::SPIN_NONE, false)));
    assert(!tb::b2bMaintaining(ci(0, tb::SPIN_MINI, false)));
    assert(!tb::b2bMaintaining(ci(0, tb::SPIN_FULL, false)));
}

static void test_b2b_is_maintained_by_a_mini_that_clears_lines() {

    assert(tb::b2bMaintaining(ci(1, tb::SPIN_MINI, false)));
    assert(tb::b2bMaintaining(ci(2, tb::SPIN_MINI, false)));

    assert(!tb::b2bMaintaining(ci(0, tb::SPIN_MINI, false)));
}

static void test_attack_combo_multiplier() {
    const int dbl[13] = {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4};
    for (int c = 0; c < 13; ++c) {
        assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), false, c) == dbl[c]);
    }
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), false, 20) == 6);
    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, false), false, 1) == 5);
    assert(tb::computeAttack(ci(2, tb::SPIN_FULL, false), true, 3) == 8);
    assert(tb::computeAttack(ci(2, tb::SPIN_FULL, false), true, 4) == 10);
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), false, -1) == 1);
}

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

static void test_attack_all_clear_adds_five_and_holds_b2b() {
    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, true), false, 0) == 9);
    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, true), false, 0) == 5);
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, true), true, 0) == 1 + 1 + 5);
    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, true), true, 5) == 11 + 5);
    assert(tb::b2bMaintaining(ci(1, tb::SPIN_NONE, true)));
    assert(tb::b2bMaintaining(ci(2, tb::SPIN_NONE, true)));
    assert(!tb::b2bMaintaining(ci(2, tb::SPIN_NONE, false)));
}

static void test_b2b_after_clear() {
    using tb::ClearInfo;
    assert(tb::b2bAfterClear(ClearInfo{0, tb::SPIN_NONE, false}, 7) == 7);
    assert(tb::b2bAfterClear(ClearInfo{1, tb::SPIN_NONE, false}, 5) == 0);
    assert(tb::b2bAfterClear(ClearInfo{4, tb::SPIN_NONE, false}, 3) == 4);
    assert(tb::b2bAfterClear(ClearInfo{1, tb::SPIN_MINI, false}, 1) == 2);
    assert(tb::b2bAfterClear(ClearInfo{1, tb::SPIN_NONE, true},  0) == 2);
    assert(tb::b2bAfterClear(ClearInfo{4, tb::SPIN_NONE, true},  1) == 3);
}

static void test_surge_charge_by_chain_length() {
    for (int c = 0; c <= 4; ++c) assert(tb::surgeCharge(c) == 0);
    assert(tb::surgeCharge(5) == 4);
    assert(tb::surgeCharge(9) == 8);
    assert(tb::surgeCharge(31) == 30);
}

static void test_attack_surge_is_sent_on_break() {
    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, false), 5, 0) == 4);
    assert(tb::computeAttack(ci(1, tb::SPIN_NONE, false), 4, 0) == 0);
    assert(tb::computeAttack(ci(2, tb::SPIN_NONE, false), 9, 3) == 1 + 8);
    assert(tb::computeAttack(ci(4, tb::SPIN_NONE, false), 9, 0) == 4 + 1);
    assert(tb::computeAttack(ci(1, tb::SPIN_MINI, false), 9, 0) == 0 + 1);
    assert(tb::computeAttack(ci(0, tb::SPIN_NONE, false), 9, 0) == 0);
}

static void test_attack_is_zero_when_no_lines_clear() {
    assert(tb::computeAttack(ci(0, tb::SPIN_NONE, false), true, 8) == 0);
    assert(tb::computeAttack(ci(0, tb::SPIN_MINI, false), true, 8) == 0);
    assert(tb::computeAttack(ci(0, tb::SPIN_FULL, false), true, 8) == 0);
}

static void test_mg_t_center_is_origin_plus_one_one() {
    for (int r = 0; r < 4; ++r) {
        const tb::Cell* c = tb::pieceCells(tb::PIECE_T, static_cast<tb::Rot>(r));
        bool found = false;
        for (int i = 0; i < 4; ++i)
            if (c[i].dx == 1 && c[i].dy == 1) found = true;
        assert(found);
    }

    const tb::Cell* t0 = tb::pieceCells(tb::PIECE_T, tb::ROT_0);
    const tb::Cell* tR = tb::pieceCells(tb::PIECE_T, tb::ROT_R);
    const tb::Cell* t2 = tb::pieceCells(tb::PIECE_T, tb::ROT_2);
    const tb::Cell* tL = tb::pieceCells(tb::PIECE_T, tb::ROT_L);
    bool nub0 = false, nubR = false, nub2 = false, nubL = false;
    for (int i = 0; i < 4; ++i) {
        if (t0[i].dx == 1 && t0[i].dy == 2) nub0 = true;
        if (tR[i].dx == 2 && tR[i].dy == 1) nubR = true;
        if (t2[i].dx == 1 && t2[i].dy == 0) nub2 = true;
        if (tL[i].dx == 0 && tL[i].dy == 1) nubL = true;
    }
    assert(nub0 && nubR && nub2 && nubL);
}

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

static void test_mg_state_index_roundtrips() {

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

static void test_mg_cell_occupied_semantics() {
    tb::Board b{};
    b.rows[3] = static_cast<uint16_t>(1u << 7);
    assert(tb::mgCellOccupied(b, 7, 3));
    assert(!tb::mgCellOccupied(b, 6, 3));
    assert(tb::mgCellOccupied(b, -1, 5));
    assert(tb::mgCellOccupied(b, -100, 5));
    assert(tb::mgCellOccupied(b, 10, 5));
    assert(tb::mgCellOccupied(b, 999, 5));
    assert(tb::mgCellOccupied(b, 4, -1));
    assert(tb::mgCellOccupied(b, 4, -50));
    assert(!tb::mgCellOccupied(b, 4, tb::BOARD_H));
    assert(!tb::mgCellOccupied(b, 4, 5000));
}

static void test_mg_classify_three_corner_gate() {
    const char* rows[] = {
        "..........",
        "..........",
        "...#.#....",
    };
    const tb::Board b = tb::boardFromAscii(rows, 3);

    assert(tb::classifyTSpin(b, tb::ROT_0, 3, 0, true, 0) == tb::SPIN_NONE);

    assert(tb::classifyTSpin(b, tb::ROT_2, 3, 0, true, 0) == tb::SPIN_NONE);

    assert(tb::classifyTSpin(b, tb::ROT_0, 3, 0, true, 4) == tb::SPIN_NONE);

    const tb::Board empty{};
    assert(tb::classifyTSpin(empty, tb::ROT_0, 4, 0, true, 0) == tb::SPIN_NONE);
}

static void test_mg_classify_requires_rotation_last() {
    const char* rows[] = {
        "..........",
        "...#......",
        "###...####",
        ".###.#####",
    };
    const tb::Board b = tb::boardFromAscii(rows, 4);

    assert(tb::classifyTSpin(b, tb::ROT_2, 3, 0, true, 0) == tb::SPIN_FULL);

    assert(tb::classifyTSpin(b, tb::ROT_2, 3, 0, false, 255) == tb::SPIN_NONE);
}

static void test_mg_classify_counts_walls_and_floor() {

    const char* wallRows[] = {
        "..........",
        "..........",
        ".#........",
    };
    const tb::Board wall = tb::boardFromAscii(wallRows, 3);
    assert(tb::classifyTSpin(wall, tb::ROT_R, -1, 0, true, 0) == tb::SPIN_MINI);

    const char* floorRows[] = {
        "..........",
        "#.........",
        "..........",
    };
    const tb::Board floorBoard = tb::boardFromAscii(floorRows, 3);
    assert(tb::classifyTSpin(floorBoard, tb::ROT_0, 0, -1, true, 0) == tb::SPIN_MINI);
}

static void test_mg_classify_mini_vs_full_same_centre() {
    const char* rows[] = {
        "..........",
        ".......#..",
        "#####...##",
        ".#####.###",
    };
    const tb::Board b = tb::boardFromAscii(rows, 4);

    assert(tb::classifyTSpin(b, tb::ROT_0, 5, 0, true, 0) == tb::SPIN_MINI);

    assert(tb::classifyTSpin(b, tb::ROT_2, 5, 0, true, 0) == tb::SPIN_FULL);

    assert(tb::classifyTSpin(b, tb::ROT_2, 5, 0, true, 4) == tb::SPIN_FULL);

    assert(tb::classifyTSpin(b, tb::ROT_L, 5, 0, true, 0) == tb::SPIN_MINI);
}

static void test_mg_classify_rot_l_front_pair() {
    const char* rows[] = {
        "...#.#....",
        "..........",
        ".....#....",
    };
    const tb::Board b = tb::boardFromAscii(rows, 3);

    assert(tb::classifyTSpin(b, tb::ROT_L, 3, 0, true, 0) == tb::SPIN_MINI);
}

static void test_mg_classify_kick_index_four_promotes() {
    const char* rows[] = {
        "..........",
        "##........",
        "#.........",
        "#.########",
        "#..#######",
        "#..#######",
    };
    const tb::Board b = tb::boardFromAscii(rows, 6);

    assert(tb::classifyTSpin(b, tb::ROT_R, 0, 0, true, 0) == tb::SPIN_MINI);
    assert(tb::classifyTSpin(b, tb::ROT_R, 0, 0, true, 3) == tb::SPIN_MINI);
    assert(tb::classifyTSpin(b, tb::ROT_R, 0, 0, true, 4) == tb::SPIN_FULL);

    assert(tb::classifyTSpin(b, tb::ROT_R, 0, 0, true, 255) == tb::SPIN_MINI);
}

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

                if (pl.spin == tb::SPIN_FULL) assert(p == tb::PIECE_T);
            }
        }
    }
}

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

static void test_mg_topped_out_board_yields_nothing() {
    tb::Board b{};
    for (int y = 0; y < tb::BOARD_H; ++y) b.rows[y] = tb::FULL_ROW;
    static tb::MoveList ml;
    tb::generateMoves(b, tb::PIECE_T, &ml);
    assert(ml.count == 0);
}

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
                nk = 255;
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

    assert(checked > 700);
}

static void test_mg_shortest_path_wins_when_rotation_earns_no_spin() {
    const char* rows[] = {
        "..........",
        "..........",
        "..........",
        "###.....##",
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

static int mgLinesFor(const tb::Board& b, tb::PieceType p, const tb::Placement& pl) {
    tb::Board c = b;
    tb::lockPiece(c, p, pl.rot, pl.x, pl.y);
    return tb::clearLines(c);
}

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
        "..........",
        "...#......",
        "###...####",
        ".###.#####",
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

}

static void test_mg_tspin_double_fixture() {
    const char* rows[] = {
        "..........",
        "...#......",
        "###...####",
        "####.#####",
    };
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

static void test_mg_tspin_triple_fixture() {
    const char* rows[] = {
        "..........",
        "##........",
        "#.........",
        "#.########",
        "#..#######",
        "#.########",
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

    assert(pl->kickIndex == 4);
    assert(mgLinesFor(b, tb::PIECE_T, *pl) == 3);

    assert(tb::classifyTSpin(b, tb::ROT_R, 0, 0, true, 0) == tb::SPIN_FULL);
}

static void test_mg_tspin_kick4_promotion_fixture() {
    const char* rows[] = {
        "..........",
        "##........",
        "#.........",
        "#.########",
        "#..#######",
        "#..#######",
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

    assert(tb::classifyTSpin(b, tb::ROT_R, 0, 0, true, 0) == tb::SPIN_MINI);

    assert(pl->spin == tb::SPIN_FULL);
}

static void test_mg_tspin_mini_fixture() {
    const char* rows[] = {
        "..........",
        ".......#..",
        "#####...##",
        ".#####.###",
    };
    const tb::Board b = tb::boardFromAscii(rows, 4);
    static tb::MoveList ml;
    tb::generateMoves(b, tb::PIECE_T, &ml);

    const tb::Placement* mini = mgFind(ml, 5, 0, tb::ROT_0);
    assert(mini != nullptr);
    assert(mini->lastWasRotation);
    assert(mini->kickIndex == 0);
    assert(mini->spin == tb::SPIN_MINI);
    assert(mgLinesFor(b, tb::PIECE_T, *mini) == 1);

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
        "..........",
        "##......##",
        "####..####",
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

static void test_mg_pathless_matches() {
    const char* cheese[] = {
        "..........",
        "....##....",
        "...####...",
        "##.#####.#",
        "#.########",
    };
    const tb::Board boards[2] = { tb::Board{}, tb::boardFromAscii(cheese, 5) };
    for (const tb::Board& b : boards) {
        for (int p = 0; p < 7; ++p) {
            tb::MoveList with{};
            tb::MoveList without{};
            tb::generateMoves(b, static_cast<tb::PieceType>(p), &with, true);
            tb::generateMoves(b, static_cast<tb::PieceType>(p), &without, false);
            assert(with.count == without.count);
            for (int i = 0; i < with.count; ++i) {
                const tb::Placement& a = with.items[i];
                const tb::Placement& c = without.items[i];
                assert(a.x == c.x && a.y == c.y && a.rot == c.rot);
                assert(a.spin == c.spin && a.lastWasRotation == c.lastWasRotation);
                assert(a.kickIndex == c.kickIndex);
                assert(c.pathLen == 0);
            }
        }
    }
    std::printf("  ok  test_mg_pathless_matches\n");
}

static void test_mg_seeded_matches_classic() {
    const char* cheese[] = {
        "..........",
        "....##....",
        "...####...",
        "##.#####.#",
        "#.########",
    };
    const char* overhang[] = {
        "..........",
        "#######...",
        "########.#",
        "#.######.#",
        "#..#####.#",
    };
    const tb::Board boards[3] = {
        tb::Board{},
        tb::boardFromAscii(cheese, 5),
        tb::boardFromAscii(overhang, 5),
    };
    for (const tb::Board& b : boards) {
        for (int p = 0; p < 7; ++p) {
            tb::MoveList seeded{};
            tb::MoveList classic{};
            tb::mgForceClassicBfs(false);
            tb::generateMoves(b, static_cast<tb::PieceType>(p), &seeded, true);
            tb::mgForceClassicBfs(true);
            tb::generateMoves(b, static_cast<tb::PieceType>(p), &classic, true);
            tb::mgForceClassicBfs(false);
            assert(seeded.count == classic.count);

            for (int i = 0; i < seeded.count; ++i) {
                const tb::Placement& a = seeded.items[i];
                bool found = false;
                for (int j = 0; j < classic.count; ++j) {
                    const tb::Placement& c = classic.items[j];
                    if (a.x == c.x && a.y == c.y && a.rot == c.rot) {
                        assert(a.spin == c.spin);
                        found = true;
                        break;
                    }
                }
                assert(found && "seeded placement missing from classic BFS");
            }
        }
    }
    std::printf("  ok  test_mg_seeded_matches_classic\n");
}

static void test_mg_seeded_paths_replayable() {
    const char* overhang[] = {
        "..........",
        "#######...",
        "########.#",
        "#.######.#",
        "#..#####.#",
    };
    const tb::Board b = tb::boardFromAscii(overhang, 5);
    for (int p = 0; p < 7; ++p) {
        tb::MoveList ml{};
        tb::generateMoves(b, static_cast<tb::PieceType>(p), &ml, true);
        assert(ml.count > 0);
        for (int i = 0; i < ml.count; ++i) {
            const tb::Placement& pl = ml.items[i];
            int x = tb::SPAWN_X;
            int y = tb::SPAWN_Y;
            tb::Rot r = tb::ROT_0;
            assert(!tb::collides(b, static_cast<tb::PieceType>(p), r, x, y));
            for (int k = 0; k < pl.pathLen; ++k) {
                int nx = 0, ny = 0;
                tb::Rot nr = tb::ROT_0;
                uint8_t nk = 255;
                const bool ok = tb::mgApplyActionForTest(
                    b, static_cast<tb::PieceType>(p),
                    static_cast<tb::Action>(pl.path[k]), x, y, r, &nx, &ny, &nr, &nk);
                assert(ok && "seeded path contains an illegal action");
                x = nx; y = ny; r = nr;
            }
            assert(x == pl.x && y == pl.y && r == pl.rot);
            assert(tb::collides(b, static_cast<tb::PieceType>(p), r, x, y - 1));
        }
    }
    std::printf("  ok  test_mg_seeded_paths_replayable\n");
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

                assert(pl.kickIndex <= 4 || pl.kickIndex == 255);

                if (pl.kickIndex != 255) assert(pl.lastWasRotation);

                if (!pl.lastWasRotation) assert(pl.kickIndex == 255);

                if (pl.lastWasRotation) {
                    assert(pl.pathLen > 0);
                    assert(tb::isRotateAction(pl.path[pl.pathLen - 1]));
                }

                if (pl.spin != tb::SPIN_NONE) {
                    assert(p != tb::PIECE_I && p != tb::PIECE_O);
                    assert(pl.lastWasRotation);
                }

                if (pl.spin == tb::SPIN_MINI) assert(p == tb::PIECE_T);

                if (pl.pathLen == 0) {
                    assert(pl.x == tb::SPAWN_X && pl.y == tb::SPAWN_Y &&
                           pl.rot == tb::ROT_0);
                }
                assert(pl.pathLen <= tb::MAX_PATH_LEN);
            }
        }
    }
}

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

    assert(f.holes == 3);
    assert(tb::extractFeatures(fixA()).holes == 0);
}

static void test_eval_covered_cells() {
    tb::Features f = tb::extractFeatures(fixB());

    assert(f.coveredCells == 5);
    assert(tb::extractFeatures(fixA()).coveredCells == 0);
}

static void test_eval_bumpiness() {

    assert(tb::extractFeatures(fixA()).bumpiness == 5);

    assert(tb::extractFeatures(fixB()).bumpiness == 4);
}

static tb::Board fixWell() {
    const char* rows[14];
    for (int i = 0; i < 14; ++i) rows[i] = "#########.";
    return tb::boardFromAscii(rows, 14);
}

static void test_eval_height_penalty() {

    assert(tb::extractFeatures(fixA()).heightPenalty == 0);

    tb::Features fw = tb::extractFeatures(fixWell());
    assert(fw.maxHeight == 14);
    assert(fw.heightPenalty == 4);
}

static void test_eval_row_transitions() {

    assert(tb::extractFeatures(fixA()).rowTransitions == 10);

    assert(tb::extractFeatures(fixB()).rowTransitions == 12);
}

static void test_eval_column_transitions() {

    assert(tb::extractFeatures(fixA()).columnTransitions == 9);

    assert(tb::extractFeatures(fixB()).columnTransitions == 12);
}

static void test_eval_well_depth() {

    assert(tb::extractFeatures(fixA()).wellDepth == 0);

    tb::Features fw = tb::extractFeatures(fixWell());
    assert(fw.wellDepth == 105);
    assert(fw.bumpiness == 14);
    assert(fw.holes == 0);
}

static void test_eval_rows_with_holes() {

    assert(tb::extractFeatures(fixB()).rowsWithHoles == 2);
    assert(tb::extractFeatures(fixA()).rowsWithHoles == 0);
    const char* rows[] = { "####......", "#..#......", "#.##......" };
    tb::Features f = tb::extractFeatures(tb::boardFromAscii(rows, 3));
    assert(f.holes == 3);
    assert(f.rowsWithHoles == 2);

    tb::Board tall{};
    tall.rows[18] = 0x1; tall.rows[2] = 0x1;
    assert(tb::extractFeatures(tall).rowsWithHoles == 17);
}

static void test_eval_tslot_cutout_tsd() {
    const char* fx[] = {
        "..........",
        "...#......",
        "###...####",
        "####.#####",
        "######.###",
    };
    const tb::Board b = tb::boardFromAscii(fx, 5);
    tb::Board c = b;
    int l1 = 0, l2 = 0;
    const int cuts = tb::cutoutTSlots(&c, 1, &l1, &l2);
    assert(cuts == 1 && l1 == 0 && l2 == 1);
    const tb::Features fr = tb::extractFeatures(b);
    const tb::Features fc = tb::extractFeatures(c);
    assert(fr.holes == 2);
    assert(fc.holes == 0);
    assert(fr.maxHeight == 4 && fc.maxHeight == 2);

    tb::Weights w = tb::defaultWeights();
    assert(tb::evaluate(b, w, 0, 0, 1) == tb::evaluate(b, w, 0, 0, 0));
    w.tslot2 = 300.0f;
    assert(tb::evaluate(b, w, 0, 0, 1) != tb::evaluate(b, w, 0, 0, 0));
    std::printf("  ok  test_eval_tslot_cutout_tsd\n");
}

static void test_eval_tslot_cutout_needs_t() {
    const char* fx[] = {
        "..........",
        "...#......",
        "###...####",
        "####.#####",
        "######.###",
    };
    const tb::Board b = tb::boardFromAscii(fx, 5);
    tb::Board c = b;
    int l1 = 0, l2 = 0;
    const int cuts = tb::cutoutTSlots(&c, 0, &l1, &l2);
    assert(cuts == 0 && l1 == 0 && l2 == 0);
    for (int y = 0; y < tb::BOARD_H; ++y) assert(c.rows[y] == b.rows[y]);
    std::printf("  ok  test_eval_tslot_cutout_needs_t\n");
}

static void test_eval_tslot_cutout_iterates() {
    const char* fx[] = {
        "..........",
        "...#......",
        "###...####",
        "####.#####",
        "###...####",
        "####.#####",
        "#########.",
    };
    const tb::Board b = tb::boardFromAscii(fx, 7);
    tb::Board c = b;
    int l1 = 0, l2 = 0;
    const int cuts = tb::cutoutTSlots(&c, 2, &l1, &l2);
    assert(cuts == 2 && l1 == 0 && l2 == 2);
    std::printf("  ok  test_eval_tslot_cutout_iterates\n");
}

static void test_eval_tslot_no_cutout_without_clear() {
    const char* fx[] = {
        "..........",
        "...#......",
        "###....###",
        "####.####.",
        "######.###",
    };
    const tb::Board b = tb::boardFromAscii(fx, 5);
    tb::Board c = b;
    int l1 = 0, l2 = 0;
    const int cuts = tb::cutoutTSlots(&c, 2, &l1, &l2);
    assert(cuts == 0 && l1 == 0 && l2 == 0);
    assert(tb::countTSlots(b) >= 1);
    std::printf("  ok  test_eval_tslot_no_cutout_without_clear\n");
}

static void test_eval_overhangs() {

    const char* open[] = { "##........", "#........." };
    tb::Features f = tb::extractFeatures(tb::boardFromAscii(open, 2));
    assert(f.holes == 1);
    assert(f.overhangs == 1);
    const char* closed[] = { "###.......", "#.#......." };
    f = tb::extractFeatures(tb::boardFromAscii(closed, 2));
    assert(f.holes == 1);
    assert(f.overhangs == 0);

    const char* roofed[] = { "####......", "#..#......", "#.##......" };
    f = tb::extractFeatures(tb::boardFromAscii(roofed, 3));
    assert(f.overhangs == 0);

    assert(tb::extractFeatures(fixB()).overhangs == 1);
    assert(tb::extractFeatures(tb::Board{}).overhangs == 0);
}

static void test_eval_t_slots() {

    const char* clear[] = {
        "..........",
        "####......",
        "###...####",
        "####.#####",
    };
    tb::Board bClear = tb::boardFromAscii(clear, 4);
    assert(tb::countTSlots(bClear) == 1);
    assert(tb::extractFeatures(bClear).tSlotCount == 1);

    const char* nearMiss[] = {
        "..........",
        "###.......",
        "###...####",
        "####.#####",
    };
    assert(tb::countTSlots(tb::boardFromAscii(nearMiss, 4)) == 0);

    const char* covered[] = {
        "..........",
        "######....",
        "###...####",
        "####.#####",
    };
    assert(tb::countTSlots(tb::boardFromAscii(covered, 4)) == 0);

    assert(tb::countTSlots(fixA()) == 0);
    assert(tb::countTSlots(fixB()) == 0);
    assert(tb::countTSlots(tb::Board{}) == 0);
}

static void test_eval_dot_product() {

    tb::Weights ones;
    ones.holes = 1.0f; ones.coveredCells = 1.0f; ones.bumpiness = 1.0f;
    ones.maxHeight = 1.0f; ones.heightPenalty = 1.0f; ones.rowTransitions = 1.0f;
    ones.columnTransitions = 1.0f; ones.wellDepth = 1.0f; ones.tSlotCount = 1.0f;
    ones.tslot1 = 1.0f; ones.tslot2 = 1.0f;
    ones.b2bLevel = 1.0f; ones.b2bBreak = 1.0f;
    ones.b2bActive = 1.0f; ones.b2bCharge = 1.0f; ones.attackDealt = 1.0f;
    ones.rowsWithHoles = 1.0f; ones.overhangs = 1.0f;
    ones.plainClear = 1.0f; ones.wastedT = 1.0f;
    ones.incomingRisk = 1.0f;

    tb::Board b = fixB();
    assert(std::fabs(tb::evaluate(b, ones, 0) - 43.0f) < 1e-3f);

    assert(std::fabs(tb::evaluate(b, ones, 1) - 45.0f) < 1e-3f);
    assert(std::fabs(tb::evaluate(b, ones, 4) - 48.0f) < 1e-3f);
    assert(std::fabs(tb::evaluate(b, ones, 5) - 53.0f) < 1e-3f);

    assert(std::fabs(tb::evaluate(b, ones, 0, 8) - 43.0f) < 1e-3f);
    assert(std::fabs(tb::evaluate(b, ones, 0, 9) - 44.0f) < 1e-3f);
    assert(std::fabs(tb::evaluate(b, ones, 0, 10) - 47.0f) < 1e-3f);

    tb::Weights onlyHoles{};
    onlyHoles.holes = -2.0f;
    assert(std::fabs(tb::evaluate(b, onlyHoles, 0) - (-6.0f)) < 1e-3f);

    tb::Weights d = tb::defaultWeights();
    assert(std::fabs(tb::evaluate(tb::Board{}, d, 0)) < 1e-6f);

    assert(d.holes < 0.0f && d.coveredCells < 0.0f);

    assert(d.rowTransitions < 0.0f && std::fabs(d.columnTransitions) < 15.0f);
    assert(d.tSlotCount > 0.0f && d.b2bActive > 0.0f && d.b2bCharge > 0.0f && d.attackDealt > 0.0f);

    assert(d.tslot1 >= 0.0f && d.tslot2 >= d.tslot1);

    assert(d.b2bLevel >= 0.0f && d.b2bBreak <= 0.0f);

    assert(d.rowsWithHoles < 0.0f);

    assert(std::fabs(d.overhangs) < -d.holes);
    assert(d.plainClear < 0.0f && d.wastedT < 0.0f);

    assert(d.incomingRisk < 0.0f && d.incomingRisk > -200.0f);

    assert(std::fabs(d.bumpiness) < 5.0f && std::fabs(d.wellDepth) < 15.0f);

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

    assert(std::fabs(w.holes - tb::defaultWeights().holes) < 1e-3f);

    assert(!tb::setWeightByName(w, "maxheight", 1.0f));
    assert(!tb::setWeightByName(w, "", 1.0f));
    assert(!tb::setWeightByName(w, "tSlotCounts", 1.0f));

    assert(tb::weightNameCount() == 21);
    for (int i = 0; i < tb::weightNameCount(); ++i) {
        assert(tb::setWeightByName(w, tb::weightName(i), 1.0f));
    }
    assert(std::fabs(w.holes - 1.0f) < 1e-3f);
    assert(std::fabs(w.wellDepth - 1.0f) < 1e-3f);
    assert(std::fabs(w.b2bActive - 1.0f) < 1e-3f);
    assert(std::fabs(w.b2bCharge - 1.0f) < 1e-3f);

    assert(tb::weightName(-1)[0] == '\0');
    assert(tb::weightName(tb::weightNameCount())[0] == '\0');

    tb::Weights v = tb::defaultWeights();
    assert(tb::setWeightByName(v, "wastedT", -77.0f));
    assert(std::fabs(tb::weightValue(v, 19) - (-77.0f)) < 1e-6f);
    assert(std::fabs(tb::weightValue(v, 0) - tb::defaultWeights().holes) < 1e-6f);
    assert(tb::weightValue(v, -1) == 0.0f && tb::weightValue(v, 21) == 0.0f);
}

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
    assert(cfg.depth == 7);
    assert(cfg.beamWidth == 30);
    assert(std::fabs(cfg.gamma - 0.95f) < 1e-6f);

    assert(std::fabs(cfg.timeBudgetMs - 4.5f) < 1e-6f);

    tb::SearchResult r = tb::search(b, tb::PIECE_T, tb::PIECE_NONE, queue,
                                    tb::PREVIEW_LEN, false, 0, cfg);
    assert(r.valid);

    tb::PieceType placed = r.useHold ? queue[0] : tb::PIECE_T;
    assert(!tb::collides(b, placed, r.placement.rot, r.placement.x, r.placement.y));
    assert(tb::collides(b, placed, r.placement.rot, r.placement.x,
                        (int)r.placement.y - 1));

    assert(tb::mgInStateBounds(r.placement.x, r.placement.y));
}

static void test_search_is_anytime() {

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

    tb::Board b{};
    for (int y = 0; y < 4; ++y) b.rows[y] = (uint16_t)0x1FF;
    b.rows[4] = (uint16_t)0x200;
    b.rows[5] = (uint16_t)0x200;

    tb::PieceType queue[tb::PREVIEW_LEN] = {
        tb::PIECE_O, tb::PIECE_O, tb::PIECE_O, tb::PIECE_O, tb::PIECE_O
    };

    tb::SearchConfig one;
    one.timeBudgetMs = 1e9f;
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

    assert(std::fabs(r1.score - (-6.0f)) < 1e-3f);

    assert(r5.score <= -7.0f + 1e-3f);
    assert(r5.score < r1.score);
}

static void test_search_prefers_hold_when_better() {

    tb::Board b{};
    for (int y = 0; y < 4; ++y) b.rows[y] = (uint16_t)(tb::FULL_ROW & ~(1u << 9));

    tb::PieceType queue[tb::PREVIEW_LEN] = {
        tb::PIECE_O, tb::PIECE_L, tb::PIECE_J, tb::PIECE_Z, tb::PIECE_O
    };
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;

    tb::SearchResult r = tb::search(b, tb::PIECE_S, tb::PIECE_I, queue,
                                    tb::PREVIEW_LEN, false, 0, cfg);
    assert(r.valid);
    assert(r.useHold);

    assert(!tb::collides(b, tb::PIECE_I, r.placement.rot, r.placement.x, r.placement.y));
    tb::Board after = b;
    tb::lockPiece(after, tb::PIECE_I, r.placement.rot, r.placement.x, r.placement.y);
    assert(tb::clearLines(after) == 4);

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

static void test_search_b2b_break_term() {

    tb::Board b{};
    b.rows[0] = (uint16_t)(tb::FULL_ROW & ~(1u << 9));
    tb::PieceType queue[tb::PREVIEW_LEN] = {
        tb::PIECE_O, tb::PIECE_O, tb::PIECE_O, tb::PIECE_O, tb::PIECE_O
    };
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;
    cfg.depth = 1;
    cfg.weights.plainClear = 0.0f;
    cfg.weights.b2bBreak = -1.0e6f;
    tb::SearchResult r = tb::search(b, tb::PIECE_I, tb::PIECE_NONE, queue,
                                    tb::PREVIEW_LEN,  3, 0, cfg);
    assert(r.valid);
    const tb::PieceType placed = r.useHold ? queue[0] : tb::PIECE_I;
    tb::Board after = b;
    tb::lockPiece(after, placed, r.placement.rot, r.placement.x, r.placement.y);
    assert(tb::clearLines(after) == 0);
    std::printf("  ok  test_search_b2b_break_term\n");
}

static void test_search_wasted_t() {

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

static void test_search_incoming_cancel() {

    tb::Board b{};
    for (int y = 0; y < 9; ++y) b.rows[y] = (uint16_t)(tb::FULL_ROW & ~1u);
    tb::PieceType queue[tb::PREVIEW_LEN] = {
        tb::PIECE_O, tb::PIECE_O, tb::PIECE_O, tb::PIECE_O, tb::PIECE_O
    };
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;
    cfg.depth = 1;
    cfg.weights = tb::Weights{};
    cfg.weights.incomingRisk = -100.0f;
    cfg.weights.attackDealt  = -50.0f;

    auto linesOf = [&](const tb::SearchResult& r) {
        const tb::PieceType placed = r.useHold ? queue[0] : tb::PIECE_I;
        tb::Board a = b;
        tb::lockPiece(a, placed, r.placement.rot, r.placement.x, r.placement.y);
        return tb::clearLines(a);
    };

    tb::SearchResult r0 = tb::search(b, tb::PIECE_I, tb::PIECE_NONE, queue,
                                     tb::PREVIEW_LEN, 0, 0, cfg, 0);
    assert(r0.valid);
    assert(linesOf(r0) == 0);

    tb::SearchResult r6 = tb::search(b, tb::PIECE_I, tb::PIECE_NONE, queue,
                                     tb::PREVIEW_LEN, 0, 0, cfg, 6);
    assert(r6.valid);
    assert(!r6.useHold);
    assert(linesOf(r6) == 4);
}

static void test_search_node_budget() {

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

    cfg.nodeBudget = 0;
    tb::SearchResult u = tb::search(b, tb::PIECE_T, tb::PIECE_O, queue, tb::PREVIEW_LEN, 0, 0, cfg);
    assert(u.nodes > a.nodes);

    assert(u.nodes < 22000);

    cfg.nodeBudget = 1;
    tb::SearchResult r = tb::search(b, tb::PIECE_T, tb::PIECE_O, queue, tb::PREVIEW_LEN, 0, 0, cfg);
    assert(r.valid && r.nodes > 1);
}

static void test_search_insertion_dedup_deterministic() {
    tb::SearchConfig cfg;
    cfg.depth = 5;
    cfg.beamWidth = 100;
    cfg.timeBudgetMs = 1e9f;
    cfg.nodeBudget = 4000;
    cfg.measureDupes = true;
    tb::Board b{};
    tb::addGarbage(b, 6, 3);
    tb::PieceType q[5] = { tb::PIECE_T, tb::PIECE_I, tb::PIECE_S, tb::PIECE_Z, tb::PIECE_O };
    tb::SearchResult r1 = tb::search(b, tb::PIECE_L, tb::PIECE_NONE, q, 5, 1, 0, cfg, 0);
    tb::SearchResult r2 = tb::search(b, tb::PIECE_L, tb::PIECE_NONE, q, 5, 1, 0, cfg, 0);
    assert(r1.valid && r2.valid);
    assert(r1.nodes == r2.nodes && r1.score == r2.score);
    assert(r1.placement.x == r2.placement.x && r1.placement.rot == r2.placement.rot);

    assert(r1.beamSlots > 0);
    assert(r1.dupes * 5 < r1.beamSlots);
    std::printf("      insertion dedup: %ld dupes in %ld slots\n", r1.dupes, r1.beamSlots);
}

static void test_search_topout_semantics() {
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;
    tb::PieceType queueO[tb::PREVIEW_LEN] = {
        tb::PIECE_O, tb::PIECE_O, tb::PIECE_O, tb::PIECE_O, tb::PIECE_O
    };

    tb::Board full{};
    for (int y = 0; y < 26; ++y) full.rows[y] = tb::FULL_ROW;
    tb::SearchResult rFull = tb::search(full, tb::PIECE_T, tb::PIECE_NONE, queueO,
                                        tb::PREVIEW_LEN, false, 0, cfg);
    assert(!rFull.valid);

    tb::Board b{};
    for (int y = 0; y < 20; ++y) b.rows[y] = (uint16_t)0x0FF;
    tb::SearchConfig zero;
    zero.timeBudgetMs = 1e9f;
    zero.depth = 1;
    zero.weights = tb::Weights{};

    tb::SearchResult r = tb::search(b, tb::PIECE_O, tb::PIECE_NONE, queueO,
                                    tb::PREVIEW_LEN, false, 0, zero);
    assert(r.valid);
    tb::Board after = b;
    tb::lockPiece(after, tb::PIECE_O, r.placement.rot, r.placement.x, r.placement.y);
    int maxRow = -1;
    for (int y = tb::BOARD_H - 1; y >= 0; --y) { if (after.rows[y] != 0) { maxRow = y; break; } }
    assert(maxRow < tb::VISIBLE_H);
}

static void test_search_time_budget() {

    tb::Board b{};
    for (int y = 0; y < 10; ++y) b.rows[y] = (uint16_t)(tb::FULL_ROW & ~(1u << y));
    tb::PieceType queue[tb::PREVIEW_LEN] = {
        tb::PIECE_S, tb::PIECE_Z, tb::PIECE_L, tb::PIECE_J, tb::PIECE_I
    };

    tb::SearchConfig cfg;
    auto t0 = std::chrono::steady_clock::now();
    tb::SearchResult r = tb::search(b, tb::PIECE_T, tb::PIECE_O, queue,
                                    tb::PREVIEW_LEN, true, 0, cfg);
    double ms = msSince(t0);
    assert(r.valid);

    assert(ms < 25.0);

    cfg.timeBudgetMs = 1.0f;
    t0 = std::chrono::steady_clock::now();
    tb::SearchResult r1 = tb::search(b, tb::PIECE_T, tb::PIECE_O, queue,
                                     tb::PREVIEW_LEN, true, 0, cfg);
    double ms1 = msSince(t0);
    assert(r1.valid);
    assert(ms1 < 10.0);
    assert(ms1 <= ms + 1.0);
    assert(!tb::collides(b, r1.useHold ? tb::PIECE_O : tb::PIECE_T,
                         r1.placement.rot, r1.placement.x, r1.placement.y));

    std::printf("      budget: full=%.2fms cut=%.2fms\n", ms, ms1);
}

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

        for (int e = 0; e < g.eventCount(); ++e) {
            assert(g.eventAt(e).type <= tb::GEV_TOPOUT);
        }

        for (int y = tb::VISIBLE_H; y < tb::BOARD_H; ++y) assert(g.board().rows[y] == 0);
    }
    assert(g.linesCleared() > 0u);

    assert(g.holdPiece() != tb::PIECE_NONE);

    g.reset(7u);
    assert(g.piecesPlaced() == 0u);
    assert(g.linesCleared() == 0u);
    assert(g.attackSent() == 0u);
    assert(g.maxB2b() == 0);
    assert(g.tSpinCount() == 0u);
    assert(g.holdPiece() == tb::PIECE_NONE);
    assert(tb::isEmpty(g.board()));
}

static void test_game_surge_accounting() {
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;

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

static void test_game_b2b_survives_non_clearing_pieces() {
    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;
    tb::Game g(42u, cfg);

    int      longestQuietRunMidChain = 0;
    int      quietRun   = 0;
    int      easyBreaks = 0;
    uint32_t prevLines  = 0;
    uint16_t prevB2b    = 0;

    for (int i = 0; i < 250 && !g.toppedOut(); ++i) {
        g.stepPiece();
        const uint32_t lines = g.linesCleared() - prevLines;
        const uint16_t b2b   = g.b2bCount();

        if (lines == 0) {

            assert(b2b == prevB2b);
            if (prevB2b > 0) {
                ++quietRun;
                if (quietRun > longestQuietRunMidChain) longestQuietRunMidChain = quietRun;
            }
        } else {
            quietRun = 0;

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

    assert(longestQuietRunMidChain >= 3);

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

    g.queueGarbage(2);
    g.stepPiece();
    assert(g.garbageReceived() == 2u);
    assert(countBits(g.board().rows[0]) == 9);
    assert(g.board().rows[0] == g.board().rows[1]);
    assert(countBits(g.board().rows[2]) + countBits(g.board().rows[3]) +
           countBits(g.board().rows[4]) + countBits(g.board().rows[5]) == 4);

    tb::Game h(42u, cfg);
    h.setMessiness(0.0f);
    h.queueGarbage(2);
    h.stepPiece();
    assert(h.board().rows[0] == g.board().rows[0]);

    // pending garbage suppresses the pc gate, so the twins only stay comparable
    // at step k with the gate off on both
    tb::SearchConfig ncfg = cfg;
    ncfg.pc.enabled = false;
    tb::Game a(7u, ncfg);
    int k = -1, atk = 0;
    for (int i = 0; i < 300 && k < 0; ++i) {
        const uint32_t before = a.attackSent();
        a.stepPiece();
        if (a.attackSent() > before) { k = i; atk = (int)(a.attackSent() - before); }
    }
    assert(k >= 0);
    tb::Game b(7u, ncfg);
    b.setMessiness(0.0f);
    for (int i = 0; i < k; ++i) b.stepPiece();
    b.queueGarbage(atk + 1);
    b.stepPiece();
    assert(b.garbageReceived() == 1u);
    assert(countBits(b.board().rows[0]) == 9);

    b.queueGarbage(5);
    b.reset(7u);
    b.stepPiece();
    assert(b.garbageReceived() == 0u);
    assert(countBits(b.board().rows[0]) <= 4);
}

static void test_game_determinism() {

    tb::SearchConfig cfg;
    cfg.timeBudgetMs = 1e9f;
    cfg.pc.timeBudgetMs = 1e9f;  // wall-clock abort must not diverge the twins

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

    tb::Game c(99u, cfg);
    for (int i = 0; i < 200; ++i) c.stepPiece();
    assert(c.attackSent() != a.attackSent() || c.linesCleared() != a.linesCleared());
}

static void test_thousand_piece_run() {
    tb::SearchConfig cfg;
    tb::Game g(42u, cfg);
    for (int i = 0; i < 1000; ++i) {
        g.stepPiece();
        if (g.toppedOut()) {
            std::printf("TOPPED OUT after %u pieces\n", g.piecesPlaced());
            assert(false);
        }
    }
    assert(g.piecesPlaced() == 1000u);

    assert(g.linesCleared() > 150u);
    assert(g.attackSent() > 0u);

    std::printf("      1000 pieces: lines=%u attack=%u tspins=%u maxb2b=%u\n",
                g.linesCleared(), g.attackSent(), g.tSpinCount(), (unsigned)g.maxB2b());
}

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

static void test_weight_table() {
    assert(tb::weightNameCount() == 21);
    static const char* expected[21] = {
        "holes", "coveredCells", "bumpiness", "maxHeight", "heightPenalty",
        "rowTransitions", "columnTransitions", "wellDepth", "tSlotCount",
        "tslot1", "tslot2",
        "b2bActive", "b2bLevel", "attackDealt", "b2bCharge", "rowsWithHoles", "overhangs",
        "plainClear", "b2bBreak", "wastedT", "incomingRisk"
    };
    for (int i = 0; i < tb::weightNameCount(); ++i) {
        assert(std::string(tb::weightName(i)) == expected[i]);
    }

    assert(tb::weightName(-1)[0] == '\0');
    assert(tb::weightName(21)[0] == '\0');

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
    assert(tb::bindingsWeightSlot(w, 21) == nullptr);
    assert(tb::bindingsWeightSlot(w, 0) == &w.holes);
    assert(tb::bindingsWeightSlot(w, 13) == &w.attackDealt);
}

static void test_bot_instance_path_replay() {
    tb::BotInstance bot(42, 5.0f, 3, 40);
    const tb::Snapshot* s = bot.snapshotPtr();

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
    assert(s->piecesPlaced > 150);
    assert(s->state == 1);
    assert(minDistinct >= 1);
}

static void test_bot_instance_hard_drops() {
    tb::BotInstance bot(42, 5.0f, 3, 40);
    const tb::Snapshot* s = bot.snapshotPtr();

    uint32_t lastPieces = 0;
    int pieces = 0, landedLast = 0, animatedFall = 0, maxFrameDrop = 0;
    int descentFrames = 0;
    int8_t prevY = 0, lastY = 0, lastGhost = 0;
    bool first = true;
    for (int f = 1; f <= 2400; ++f) {
        bot.tick(f * 16.6667);
        if (s->piecesPlaced != lastPieces) {
            if (lastPieces > 0) {
                ++pieces;
                if (lastY == lastGhost) ++landedLast;
                if (descentFrames >= 2) ++animatedFall;
            }
            lastPieces = s->piecesPlaced;
            descentFrames = 0;
            first = true;
        } else if (!first && s->activeY < prevY) {
            ++descentFrames;
            const int drop = prevY - s->activeY;
            if (drop > maxFrameDrop) maxFrameDrop = drop;
        }
        first = false;
        prevY = s->activeY;
        lastY = s->activeY;
        lastGhost = s->ghostY;
    }
    std::printf("      falls: %d pieces, %d rested on the ghost, %d fell over 2+ frames, "
                "max %d cells in a frame\n",
                pieces, landedLast, animatedFall, maxFrameDrop);
    assert(pieces > 150);
    assert(landedLast == pieces);
    assert(animatedFall * 2 >= pieces);
    assert(maxFrameDrop <= 12);
}

static void test_uniform_pacing() {
    tb::BotInstance bot(42, 20.0f, 4, 60);
    const tb::Snapshot* s = bot.snapshotPtr();
    double pieceStart = 0.0;
    uint32_t serial = 0;
    bool sawSpinPiece = false, spinThisPiece = false;
    double longestSpin = 0.0, longestPlain = 0.0;
    int spins = 0, plains = 0;

    for (int f = 1; f <= 3600; ++f) {
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
    assert(sawSpinPiece);

    assert(longestSpin  <= 70.0);
    assert(longestPlain <= 70.0);
}

static void test_game_bot_instance_parity() {
    tb::SearchConfig cfg;
    cfg.depth        = 3;
    cfg.beamWidth    = 24;
    cfg.timeBudgetMs = 1.0e9f;
    cfg.nodeBudget   = 4500;
    cfg.pc.timeBudgetMs = 1.0e9f;  // both sides must abort identically, i.e. never

    const uint32_t seed = 42;
    tb::Game        game(seed, cfg);
    tb::BotInstance bot(seed, 20.0f, cfg.depth, cfg.beamWidth);
    bot.setTimeBudget(cfg.timeBudgetMs);
    bot.setPcConfig(cfg.pc);
    const tb::Snapshot* s = bot.snapshotPtr();

    const int kPieces = 500;
    double now = 0.0;
    int matched = 0;

    for (int p = 1; p <= kPieces; ++p) {

        if (p % 10 == 0) {
            const int k = 1 + (p / 10) % 4;
            game.queueGarbage(k);
            bot.queueGarbage(k);
        }
        game.stepPiece();
        if (game.toppedOut()) break;

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

    Board tb_ = boxed(PIECE_T, ROT_0, 3, 1);
    assert(classifySpin(tb_, PIECE_T, ROT_0, 3, 1, true, 0) == SPIN_FULL);
    assert(classifySpin(tb_, PIECE_T, ROT_0, 3, 1, true, 0)
           == classifyTSpin(tb_, ROT_0, 3, 1, true, 0));

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

    const char* notch[] = {
        "##..######",
        "#..#######",
    };
    const Board nb = boardFromAscii(notch, 2);
    assert(!collides(nb, PIECE_S, ROT_0, 1, -1));
    assert(isImmobile(nb, PIECE_S, ROT_0, 1, -1));
}

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

static void test_pc_regions_ok() {
    // 1-high, cols 1-6 filled: segments {0}=1 and {7,8,9}=3 -> both fail mod 4
    {
        static const char* rows[] = {".######..."};
        const tb::Board b = tb::boardFromAscii(rows, 1);
        assert(!tb::pcRegionsOk(b, 1));
    }
    // 1-high, walls at 0-2 and 7-9, middle gap of 4 -> ok
    {
        static const char* rows[] = {"###....###"};
        const tb::Board b = tb::boardFromAscii(rows, 1);
        assert(tb::pcRegionsOk(b, 1));
    }
    // 1-high, wall at 4-5 splits 4 + 4 -> ok
    {
        static const char* rows[] = {"....##...."};
        const tb::Board b = tb::boardFromAscii(rows, 1);
        assert(tb::pcRegionsOk(b, 1));
    }
    // 2-high: col 0 full both rows is a wall; leaves 2*9=18 -> 18 % 4 != 0 -> fail
    {
        static const char* rows[] = {"#.........",
                                     "#........."};
        const tb::Board b = tb::boardFromAscii(rows, 2);
        assert(!tb::pcRegionsOk(b, 2));
    }
    // empty 2-high board = one region of 20 -> ok
    {
        tb::Board b{};
        assert(tb::pcRegionsOk(b, 2));
    }
    // 2-high, col 0 filled on the top row only -> NOT a wall; 1 + 8 = 9 -> fail
    {
        static const char* rows[] = {"#....#####",
                                     ".....#####"};
        const tb::Board b = tb::boardFromAscii(rows, 2);
        assert(!tb::pcRegionsOk(b, 2));
    }
    // leading segment of 1 fails at the wall even though the trailing 4 is clean
    {
        static const char* rows[] = {".#####...."};
        const tb::Board b = tb::boardFromAscii(rows, 1);
        assert(!tb::pcRegionsOk(b, 1));
    }
}

static void test_pc_fillable_ok() {
    // 2-deep empty column walled on both sides: nothing reaches it
    {
        static const char* rows[] = {"##.#######",
                                     "##.#######"};
        const tb::Board b = tb::boardFromAscii(rows, 2);
        assert(!tb::pcFillableOk(b, 2));
    }
    // 4-deep empty column at height 4: vertical I fills it
    {
        static const char* rows[] = {"#########.",
                                     "#########.",
                                     "#########.",
                                     "#########."};
        const tb::Board b = tb::boardFromAscii(rows, 4);
        assert(tb::pcFillableOk(b, 4));
    }
    // single notch blocked left/right with floor below
    {
        static const char* rows[] = {"##.#######",
                                     "##########"};
        const tb::Board b = tb::boardFromAscii(rows, 2);
        assert(!tb::pcFillableOk(b, 2));
    }
    // edge column counts the wall as a blocked side
    {
        static const char* rows[] = {".#########",
                                     "##########"};
        const tb::Board b = tb::boardFromAscii(rows, 2);
        assert(!tb::pcFillableOk(b, 2));
    }
    // adjacent empties leave an open side -> fillable
    {
        static const char* rows[] = {"#..#######",
                                     "##########"};
        const tb::Board b = tb::boardFromAscii(rows, 2);
        assert(tb::pcFillableOk(b, 2));
    }
    // empty board: every side is open
    {
        tb::Board b{};
        assert(tb::pcFillableOk(b, 2));
    }
}

static void test_pc_solve_o_rain_two_line() {
    tb::Board b{};
    const tb::PieceType q[4] = {tb::PIECE_O, tb::PIECE_O, tb::PIECE_O, tb::PIECE_O};
    tb::PcConfig cfg;
    const tb::PcResult r = tb::pcSolve(b, tb::PIECE_O, tb::PIECE_NONE, q, 4, 0, cfg);
    assert(r.valid);
    assert(r.prob == 1.0f);
    assert(r.height == 2);
    assert(!r.useHold);
    assert(r.move.pathLen > 0);  // real path attached for the renderer
}

static void test_pc_solve_prefers_two_line_tiebreak() {
    tb::Board b{};
    tb::PieceType q[9];
    for (int i = 0; i < 9; ++i) q[i] = tb::PIECE_O;
    tb::PcConfig cfg;
    const tb::PcResult r = tb::pcSolve(b, tb::PIECE_O, tb::PIECE_NONE, q, 9, 0, cfg);
    assert(r.valid);
    assert(r.prob == 1.0f);
    // ten O pieces tile 4x10; 2-line is also reachable with the first five, so
    // height 2 wins the tie by being tried first
    assert(r.height == 2);
}

static void test_pc_solve_vertical_i_finish() {
    static const char* rows[] = {"#########.",
                                 "#########.",
                                 "#########.",
                                 "#########."};
    const tb::Board b = tb::boardFromAscii(rows, 4);
    tb::PcConfig cfg;
    const tb::PcResult r = tb::pcSolve(b, tb::PIECE_I, tb::PIECE_NONE, nullptr, 0, 0, cfg);
    assert(r.valid);
    assert(r.prob == 1.0f);
    assert(r.height == 4);
    assert(!r.useHold);
}

static void test_pc_solve_rejects_mod4() {
    static const char* rows[] = {"#########."};  // 1 empty in row 0
    const tb::Board b = tb::boardFromAscii(rows, 1);
    tb::PcConfig cfg;
    const tb::PcResult r = tb::pcSolve(b, tb::PIECE_I, tb::PIECE_NONE, nullptr, 0, 0, cfg);
    assert(!r.valid);  // 11 and 31 empties -> no candidate height
}

static void test_pc_solve_s_cannot_fill_column() {
    // 2-wide 4-high well at cols 8,9; supply is S,S -> impossible
    static const char* rows[] = {"########..",
                                 "########..",
                                 "########..",
                                 "########.."};
    const tb::Board b = tb::boardFromAscii(rows, 4);
    const tb::PieceType q[1] = {tb::PIECE_S};
    tb::PcConfig cfg;
    const tb::PcResult r = tb::pcSolve(b, tb::PIECE_S, tb::PIECE_NONE, q, 1, 0, cfg);
    assert(!r.valid);
}

static void test_pc_solve_hold_swap() {
    // 2x2 notch at cols 8,9; current S is useless, hold O completes
    static const char* rows[] = {"########..",
                                 "########.."};
    const tb::Board b = tb::boardFromAscii(rows, 2);
    tb::PcConfig cfg;
    const tb::PcResult r = tb::pcSolve(b, tb::PIECE_S, tb::PIECE_O, nullptr, 0, 0, cfg);
    assert(r.valid);
    assert(r.prob == 1.0f);
    assert(r.useHold);
}

static void test_pc_solve_empty_hold_defer() {
    // same notch; hold empty, O is first in queue: root holds S and places O
    static const char* rows[] = {"########..",
                                 "########.."};
    const tb::Board b = tb::boardFromAscii(rows, 2);
    const tb::PieceType q[1] = {tb::PIECE_O};
    tb::PcConfig cfg;
    const tb::PcResult r = tb::pcSolve(b, tb::PIECE_S, tb::PIECE_NONE, q, 1, 0, cfg);
    assert(r.valid);
    assert(r.prob == 1.0f);
    assert(r.useHold);
}

static void test_pc_solve_four_line_double_i() {
    // 2-wide 4-high well at cols 8,9: two vertical I pieces complete at H=4
    static const char* rows[] = {"########..",
                                 "########..",
                                 "########..",
                                 "########.."};
    const tb::Board b = tb::boardFromAscii(rows, 4);
    const tb::PieceType q[1] = {tb::PIECE_I};
    tb::PcConfig cfg;
    const tb::PcResult r = tb::pcSolve(b, tb::PIECE_I, tb::PIECE_NONE, q, 1, 0, cfg);
    assert(r.valid);
    assert(r.prob == 1.0f);
    assert(r.height == 4);
}

static void test_pc_solve_guaranteed_forced_draw() {
    // 4-wide 2-high notch; root places O leaving 2x2, sole unknown draw is O
    static const char* rows[] = {"######....",
                                 "######...."};
    const tb::Board b = tb::boardFromAscii(rows, 2);
    tb::PcConfig cfg;
    const tb::PcResult r = tb::pcSolveHeight(b, 2, tb::PIECE_O, tb::PIECE_Z,
                                             nullptr, 0, 1u << tb::PIECE_O, cfg);
    assert(r.valid);
    assert(r.prob == 1.0f);
    assert(!r.useHold);
}

static void test_pc_solve_unknown_refutes() {
    // same notch, unknown draw is O or I: the I branch cannot finish the 2x2,
    // so under guaranteed semantics the line is refused outright
    static const char* rows[] = {"######....",
                                 "######...."};
    const tb::Board b = tb::boardFromAscii(rows, 2);
    tb::PcConfig cfg;
    const uint8_t mask = static_cast<uint8_t>((1u << tb::PIECE_O) | (1u << tb::PIECE_I));
    const tb::PcResult r = tb::pcSolveHeight(b, 2, tb::PIECE_O, tb::PIECE_Z,
                                             nullptr, 0, mask, cfg);
    assert(!r.valid);
    assert(r.prob == 0.0f);
}

static void test_pc_solve_skips_undecidable_supply() {
    // empty board, one known piece: neither height is decidable, so the solver
    // must refuse instantly instead of burning its budget on refutation
    tb::Board b{};
    tb::PcConfig cfg;
    const tb::PcResult r = tb::pcSolve(b, tb::PIECE_O, tb::PIECE_NONE, nullptr, 0, 0x7F, cfg);
    assert(!r.valid);
    assert(!r.aborted);
    assert(r.nodes == 0);
}

static void test_pc_solver_node_budget_aborts() {
    tb::Board b{};
    const tb::PieceType q[4] = {tb::PIECE_O, tb::PIECE_O, tb::PIECE_O, tb::PIECE_O};
    tb::PcConfig cfg;
    cfg.nodeBudget = 2;  // cannot possibly finish a 5-piece solve
    const tb::PcResult r = tb::pcSolve(b, tb::PIECE_O, tb::PIECE_NONE, q, 4, 0, cfg);
    assert(!r.valid);
    assert(r.aborted);
    assert(r.nodes <= 8);  // stopped promptly; nodes now accumulate across both heights
}

static void test_pc_plan_gate() {
    static const char* rows[] = {"#########.",
                                 "#########.",
                                 "#########.",
                                 "#########."};
    const tb::Board b = tb::boardFromAscii(rows, 4);
    tb::SearchConfig cfg;
    tb::SearchResult out{};

    assert(tb::pcPlan(b, tb::PIECE_I, tb::PIECE_NONE, nullptr, 0, 0, 0, cfg, &out));
    assert(out.valid);
    assert(!out.useHold);
    assert(out.placement.rot == tb::ROT_R);
    assert(out.placement.x == 7);  // box origin; I at ROT_R occupies column x+2 = 9
    tb::Board done = b;
    tb::lockPiece(done, tb::PIECE_I, out.placement.rot, out.placement.x, out.placement.y);
    assert(tb::clearLines(done) == 4);
    assert(tb::isEmpty(done));

    assert(!tb::pcPlan(b, tb::PIECE_I, tb::PIECE_NONE, nullptr, 0, 0, 3, cfg, &out));

    tb::SearchConfig off = cfg;
    off.pc.enabled = false;
    assert(!tb::pcPlan(b, tb::PIECE_I, tb::PIECE_NONE, nullptr, 0, 0, 0, off, &out));

    tb::Board tall = b;
    tall.rows[4] = 1u;
    assert(!tb::pcPlan(tall, tb::PIECE_I, tb::PIECE_NONE, nullptr, 0, 0, 0, cfg, &out));
}

static void test_game_pc_determinism() {
    tb::SearchConfig cfg;
    cfg.nodeBudget = 2000;
    cfg.timeBudgetMs = 1000000000.0f;
    cfg.pc.timeBudgetMs = 1000000000.0f;  // wall-clock abort must not diverge the twins
    tb::Game a(42u, cfg), c(42u, cfg);
    for (int i = 0; i < 150 && !a.toppedOut(); ++i) { a.stepPiece(); c.stepPiece(); }
    assert(a.piecesPlaced() == c.piecesPlaced());
    assert(a.attackSent() == c.attackSent());
    assert(a.pcCount() == c.pcCount());
    assert(a.pcCount() > 0);
    for (int y = 0; y < tb::BOARD_H; ++y) assert(a.board().rows[y] == c.board().rows[y]);
}

static void test_game_pc_solver_finds_pcs() {
    tb::SearchConfig on;
    on.nodeBudget = 2000;
    on.timeBudgetMs = 1000000000.0f;
    on.pc.timeBudgetMs = 1000000000.0f;
    tb::SearchConfig off = on;
    off.pc.enabled = false;

    uint32_t pcsOn = 0, pcsOff = 0;
    for (uint32_t seed = 1; seed <= 4; ++seed) {
        tb::Game g1(seed, on), g2(seed, off);
        for (int i = 0; i < 400 && !g1.toppedOut(); ++i) g1.stepPiece();
        for (int i = 0; i < 400 && !g2.toppedOut(); ++i) g2.stepPiece();
        pcsOn  += g1.pcCount();
        pcsOff += g2.pcCount();
    }
    assert(pcsOn >= 1);
    assert(pcsOn >= pcsOff);
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
    RUN(test_bag_remaining_mask);
    RUN(test_attack_prd_table_without_b2b_or_combo);
    RUN(test_attack_b2b_bonus_is_applied);
    RUN(test_attack_b2b_bonus_is_not_applied);
    RUN(test_b2bMaintaining_rules);
    RUN(test_b2b_is_maintained_by_a_mini_that_clears_lines);
    RUN(test_attack_combo_multiplier);
    RUN(test_attack_combo_log_floor_for_zero_base);
    RUN(test_attack_combo_multiplier_is_unbounded);
    RUN(test_attack_all_clear_adds_five_and_holds_b2b);
    RUN(test_b2b_after_clear);
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
    RUN(test_eval_tslot_cutout_tsd);
    RUN(test_eval_tslot_cutout_needs_t);
    RUN(test_eval_tslot_cutout_iterates);
    RUN(test_eval_tslot_no_cutout_without_clear);
    RUN(test_eval_t_slots);
    RUN(test_eval_dot_product);
    RUN(test_weight_by_name);
    RUN(test_search_empty_board);
    RUN(test_search_is_anytime);
    RUN(test_search_uses_full_depth);
    RUN(test_search_prefers_hold_when_better);
    RUN(test_search_plain_clear_penalty);
    RUN(test_search_b2b_break_term);
    RUN(test_search_wasted_t);
    RUN(test_search_incoming_cancel);
    RUN(test_search_node_budget);
    RUN(test_search_insertion_dedup_deterministic);
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
    RUN(test_mg_pathless_matches);
    RUN(test_mg_seeded_matches_classic);
    RUN(test_mg_seeded_paths_replayable);
    RUN(test_immobile_requires_an_overhang);
    RUN(test_starved_search_still_sweeps_the_root);
    RUN(test_uniform_pacing);
    RUN(test_game_bot_instance_parity);
    RUN(test_pc_regions_ok);
    RUN(test_pc_fillable_ok);
    RUN(test_pc_solve_o_rain_two_line);
    RUN(test_pc_solve_prefers_two_line_tiebreak);
    RUN(test_pc_solve_vertical_i_finish);
    RUN(test_pc_solve_rejects_mod4);
    RUN(test_pc_solve_s_cannot_fill_column);
    RUN(test_pc_solve_hold_swap);
    RUN(test_pc_solve_empty_hold_defer);
    RUN(test_pc_solve_four_line_double_i);
    RUN(test_pc_solve_guaranteed_forced_draw);
    RUN(test_pc_solve_unknown_refutes);
    RUN(test_pc_solve_skips_undecidable_supply);
    RUN(test_pc_solver_node_budget_aborts);
    RUN(test_pc_plan_gate);
    RUN(test_game_pc_determinism);
    RUN(test_game_pc_solver_finds_pcs);
    std::printf("all %d tests passed\n", g_testCount);
    return 0;
}
