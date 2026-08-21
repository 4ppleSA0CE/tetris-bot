// tests/tests.cpp -- the entire test suite for tetris-bot.
// Plain assert(), one binary, no framework. Run with `make test`.

#ifdef NDEBUG
#error "tests must be built with assert() enabled -- NDEBUG must not be defined"
#endif

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "core/types.h"

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

int main() {
    RUN(test_types_constants);
    std::printf("all %d tests passed\n", g_testCount);
    return 0;
}
