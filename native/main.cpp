// native/main.cpp -- development CLI harness. Runs the core natively, no browser.
//
//   ./tetris_bot [--seed N] [--pieces N] [--random] [--print] [--stats]
//
// This milestone implements --random only: uniformly random hard-drop
// placements, no search, no evaluation, no spin classification. The search
// milestone adds --depth and --width.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/attack.h"
#include "core/board.h"
#include "core/piece.h"
#include "core/rng.h"
#include "core/srs.h"
#include "core/types.h"

namespace {

constexpr int SPAWN_X = 4;
constexpr int SPAWN_Y = 21;

struct Stats {
    uint32_t pieces = 0;
    uint32_t lines = 0;
    uint32_t attack = 0;
    uint32_t tspins = 0;
    uint32_t topouts = 0;
    uint32_t maxB2b = 0;
};

void printBoard(const tb::Board& b, unsigned index) {
    std::printf("--- piece %u ---\n", index);
    for (int y = tb::VISIBLE_H - 1; y >= 0; --y) {
        std::putchar('|');
        for (int x = 0; x < tb::BOARD_W; ++x)
            std::putchar(((b.rows[y] >> x) & 1u) ? '#' : '.');
        std::printf("|\n");
    }
    std::printf("+----------+\n");
}

void usage() {
    std::fprintf(stderr,
                 "usage: tetris_bot [--seed N] [--pieces N] --random [--print] [--stats]\n"
                 "  --random   play uniformly random hard-drop placements\n"
                 "  --print    dump the visible board after every piece\n"
                 "  --stats    print totals at the end\n");
}

} // namespace

int main(int argc, char** argv) {
    uint32_t seed = 42;
    int pieces = 50;
    bool doPrint = false;
    bool doStats = false;
    bool randomMode = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--pieces") == 0 && i + 1 < argc) {
            pieces = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--print") == 0) {
            doPrint = true;
        } else if (std::strcmp(argv[i], "--stats") == 0) {
            doStats = true;
        } else if (std::strcmp(argv[i], "--random") == 0) {
            randomMode = true;
        } else {
            std::fprintf(stderr, "unknown or incomplete option: %s\n", argv[i]);
            usage();
            return 2;
        }
    }

    if (!randomMode) {
        std::fprintf(stderr, "no mode selected\n");
        usage();
        return 2;
    }

    tb::Bag bag(seed);
    uint32_t rngState = (seed == 0u) ? 0x9E3779B9u : seed;

    tb::Board board{};
    tb::PieceType queue[tb::PREVIEW_LEN + 1];
    for (int i = 0; i < tb::PREVIEW_LEN + 1; ++i) queue[i] = bag.next();

    tb::PieceType hold = tb::PIECE_NONE;
    bool b2bActive = false;
    int combo = 0;
    uint32_t b2bRun = 0;
    Stats st;

    for (int n = 0; n < pieces; ++n) {
        tb::PieceType current = queue[0];
        for (int i = 0; i < tb::PREVIEW_LEN; ++i) queue[i] = queue[i + 1];
        queue[tb::PREVIEW_LEN] = bag.next();

        // One hold swap per piece, taken about one time in eight.
        if ((tb::xorshift32(rngState) & 7u) == 0u) {
            if (hold == tb::PIECE_NONE) {
                hold = current;
                current = queue[0];
                for (int i = 0; i < tb::PREVIEW_LEN; ++i) queue[i] = queue[i + 1];
                queue[tb::PREVIEW_LEN] = bag.next();
            } else {
                const tb::PieceType swap = hold;
                hold = current;
                current = swap;
            }
        }

        // Pick a random legal (rotation, column), then hard-drop it.
        int rot = 0;
        int x = 0;
        int y = 0;
        bool found = false;
        if (!tb::collides(board, current, tb::ROT_0, SPAWN_X, SPAWN_Y)) {
            for (int attempt = 0; attempt < 200 && !found; ++attempt) {
                rot = static_cast<int>(tb::xorshift32(rngState) & 3u);
                x = static_cast<int>(tb::xorshift32(rngState) % 13u) - 2;
                if (tb::collides(board, current, static_cast<tb::Rot>(rot), x, SPAWN_Y)) continue;
                y = tb::dropY(board, current, static_cast<tb::Rot>(rot), x, SPAWN_Y);
                found = true;
            }
        }

        if (!found) {
            ++st.topouts;
            board = tb::Board{};
            hold = tb::PIECE_NONE;
            b2bActive = false;
            combo = 0;
            b2bRun = 0;
            if (doPrint) {
                std::printf("topout\n");
                printBoard(board, static_cast<unsigned>(n + 1));
            }
            continue;
        }

        tb::lockPiece(board, current, static_cast<tb::Rot>(rot), x, y);
        const int cleared = tb::clearLines(board);

        tb::ClearInfo info;
        info.lines = static_cast<uint8_t>(cleared);
        info.spin = tb::SPIN_NONE;   // random play never classifies a spin
        info.perfectClear = (cleared > 0) && tb::isEmpty(board);

        st.attack += static_cast<uint32_t>(tb::computeAttack(info, b2bActive, combo));
        st.lines += static_cast<uint32_t>(cleared);
        if (info.spin != tb::SPIN_NONE && cleared > 0) ++st.tspins;

        if (cleared > 0) {
            if (tb::b2bMaintaining(info)) {
                if (b2bActive) ++b2bRun;
                b2bActive = true;
            } else {
                b2bActive = false;
                b2bRun = 0;
            }
            ++combo;
        } else {
            combo = 0;
        }
        if (b2bRun > st.maxB2b) st.maxB2b = b2bRun;
        ++st.pieces;

        if (doPrint) printBoard(board, static_cast<unsigned>(n + 1));
    }

    if (doStats) {
        // Labels and column layout are the shared contract's, verbatim, so the
        // search milestone can replace this harness without changing the
        // meaning of any downstream grep. The contract's seventh line,
        // "search ms  p50 ... p99 ...", is absent on purpose: this milestone
        // has no search to time. Plan 3 appends it in the same label column.
        const double tspinRate =
            st.pieces ? 100.0 * static_cast<double>(st.tspins) / static_cast<double>(st.pieces)
                      : 0.0;
        std::printf("%-12s%5u\n", "pieces", st.pieces);
        std::printf("%-12s%5u\n", "lines", st.lines);
        std::printf("%-12s%5u\n", "attack", st.attack);
        std::printf("%-12s%5u   (%.2f / 100)\n", "t-spins", st.tspins, tspinRate);
        std::printf("%-12s%5u\n", "max b2b", st.maxB2b);
        std::printf("%-12s%5u\n", "top-outs", st.topouts);
    }

    return 0;
}
