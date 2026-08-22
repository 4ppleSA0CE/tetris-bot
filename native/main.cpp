// native/main.cpp -- development CLI harness. Runs the core natively, no browser.
//
//   ./tetris_bot [--seed N] [--pieces N] [--random] [--print] [--stats]
//
// This milestone implements --random only: uniformly random hard-drop
// placements, no search, no evaluation, no spin classification. The search
// milestone adds --depth and --width.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/attack.h"
#include "core/board.h"
#include "core/movegen.h"
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
                 "       tetris_bot --movegen [FIXTURE|all]\n"
                 "  --random   play uniformly random hard-drop placements\n"
                 "  --movegen  inspect generated T placements for a fixture\n"
                 "  --movegen-bench [N]  time generateMoves throughput\n"
                 "  --print    dump the visible board after every piece\n"
                 "  --stats    print totals at the end\n");
}

} // namespace

// ---------------------------------------------------------------------------
// --movegen : move-generation inspection mode (milestone 2).
//
// Runs generateMoves for the T piece on a named hand-built fixture, prints
// every placement classified as a spin together with the action path that
// reaches it, and prints one GATE line per fixture stating whether the exact
// expected placement was found. Exit code 0 only if every gate passes.
// ---------------------------------------------------------------------------
namespace {

struct MovegenFixture {
    const char*        name;
    const char* const* rows;
    int                nRows;
    // expX >= 0 : this exact spin placement must exist
    // expX == -1: there must be ZERO spin placements
    // expX == -2: no expectation, printed for inspection only
    int                expX, expY;
    tb::Rot            expRot;
    tb::SpinKind       expSpin;
    int                expKick;
    int                expLines;
};

const char* const MG_FIX_TSS[] = {
    "..........", "...#......", "###...####", ".###.#####",
};
const char* const MG_FIX_TSD[] = {
    "..........", "...#......", "###...####", "####.#####",
};
const char* const MG_FIX_TST[] = {
    "..........", "##........", "#.........", "#.########", "#..#######", "#.########",
};
const char* const MG_FIX_MINI[] = {
    "..........", ".......#..", "#####...##", ".#####.###",
};
const char* const MG_FIX_STSD[] = {
    "..........", "##........", "#.........", "#.########", "#..#######", "#..#######",
};
const char* const MG_FIX_NONE[] = {
    "..........", "##......##", "####..####",
};
const char* const MG_FIX_EMPTY[] = {
    "..........",
};
const char* const MG_FIX_MID[] = {
    "..........", "..........", "#...####..", "##..#####.", "###.#####.",
    "####.####.", "#####.####", "######.###", "#######.##", "########.#",
};

const MovegenFixture MG_FIXTURES[] = {
    { "tss",   MG_FIX_TSS,   4,  3, 0, tb::ROT_2, tb::SPIN_FULL, 0, 1 },
    { "tsd",   MG_FIX_TSD,   4,  3, 0, tb::ROT_2, tb::SPIN_FULL, 0, 2 },
    { "tst",   MG_FIX_TST,   6,  0, 0, tb::ROT_R, tb::SPIN_FULL, 4, 3 },
    { "mini",  MG_FIX_MINI,  4,  5, 0, tb::ROT_0, tb::SPIN_MINI, 0, 1 },
    { "stsd",  MG_FIX_STSD,  6,  0, 0, tb::ROT_R, tb::SPIN_FULL, 4, 2 },
    { "none",  MG_FIX_NONE,  3, -1, 0, tb::ROT_0, tb::SPIN_NONE, 0, 0 },
    { "empty", MG_FIX_EMPTY, 1, -2, 0, tb::ROT_0, tb::SPIN_NONE, 0, 0 },
    { "mid",   MG_FIX_MID,  10, -2, 0, tb::ROT_0, tb::SPIN_NONE, 0, 0 },
};
constexpr int MG_FIXTURE_COUNT =
    static_cast<int>(sizeof(MG_FIXTURES) / sizeof(MG_FIXTURES[0]));

const char* mgRotName(tb::Rot r) {
    switch (r) {
        case tb::ROT_0: return "0";
        case tb::ROT_R: return "R";
        case tb::ROT_2: return "2";
        default:        return "L";
    }
}

const char* mgSpinName(tb::SpinKind s) {
    switch (s) {
        case tb::SPIN_MINI: return "MINI";
        case tb::SPIN_FULL: return "FULL";
        default:            return "NONE";
    }
}

const char* mgActionName(tb::Action a) {
    switch (a) {
        case tb::ACT_LEFT:      return "L";
        case tb::ACT_RIGHT:     return "R";
        case tb::ACT_CW:        return "CW";
        case tb::ACT_CCW:       return "CCW";
        case tb::ACT_180:       return "180";
        case tb::ACT_SOFT_DROP: return "SD";
        default:                return "?";
    }
}

int mgLinesFor(const tb::Board& b, tb::PieceType p, const tb::Placement& pl) {
    tb::Board c = b;
    tb::lockPiece(c, p, pl.rot, pl.x, pl.y);
    return tb::clearLines(c);
}

void mgPrintPlacement(const tb::Board& b, const tb::Placement& pl) {
    std::printf("  x=%d y=%d rot=%s spin=%s kick=%u lines=%d pathLen=%u path=",
                pl.x, pl.y, mgRotName(pl.rot), mgSpinName(pl.spin),
                static_cast<unsigned>(pl.kickIndex),
                mgLinesFor(b, tb::PIECE_T, pl),
                static_cast<unsigned>(pl.pathLen));
    for (int i = 0; i < pl.pathLen; ++i)
        std::printf("%s%s", i ? "," : "", mgActionName(pl.path[i]));
    std::printf("\n");
}

// Returns true if the fixture's gate passes.
bool mgRunFixture(const MovegenFixture& f) {
    const tb::Board b = tb::boardFromAscii(f.rows, f.nRows);
    static tb::MoveList ml;
    tb::generateMoves(b, tb::PIECE_T, &ml);

    std::printf("== movegen: %s ==\n", f.name);
    std::printf("board (top row first):\n");
    for (int y = f.nRows - 1; y >= 0; --y) {
        char line[tb::BOARD_W + 1];
        for (int x = 0; x < tb::BOARD_W; ++x)
            line[x] = ((b.rows[y] >> x) & 1u) ? '#' : '.';
        line[tb::BOARD_W] = '\0';
        std::printf("%s\n", line);
    }

    int spins = 0;
    for (int i = 0; i < ml.count; ++i)
        if (ml.items[i].spin != tb::SPIN_NONE) ++spins;
    std::printf("piece=T placements=%d spins=%d\n", ml.count, spins);

    std::printf("spin placements:\n");
    if (spins == 0) std::printf("  (none)\n");
    for (int i = 0; i < ml.count; ++i)
        if (ml.items[i].spin != tb::SPIN_NONE) mgPrintPlacement(b, ml.items[i]);

    if (f.expX == -2) {
        std::printf("GATE %s: SKIP (informational)\n", f.name);
        return true;
    }
    if (f.expX == -1) {
        const bool ok = (spins == 0);
        std::printf("GATE %s: %s (%d spin placements)\n",
                    f.name, ok ? "PASS" : "FAIL", spins);
        return ok;
    }
    for (int i = 0; i < ml.count; ++i) {
        const tb::Placement& pl = ml.items[i];
        if (pl.x == f.expX && pl.y == f.expY && pl.rot == f.expRot &&
            pl.spin == f.expSpin &&
            static_cast<int>(pl.kickIndex) == f.expKick &&
            mgLinesFor(b, tb::PIECE_T, pl) == f.expLines) {
            std::printf("GATE %s: PASS (x=%d y=%d rot=%s spin=%s kick=%d lines=%d)\n",
                        f.name, f.expX, f.expY, mgRotName(f.expRot),
                        mgSpinName(f.expSpin), f.expKick, f.expLines);
            return true;
        }
    }
    std::printf("GATE %s: FAIL (expected x=%d y=%d rot=%s spin=%s kick=%d lines=%d)\n",
                f.name, f.expX, f.expY, mgRotName(f.expRot),
                mgSpinName(f.expSpin), f.expKick, f.expLines);
    return false;
}

int runMovegenMode(const char* which) {
    bool all = true;
    bool matched = false;
    for (int i = 0; i < MG_FIXTURE_COUNT; ++i) {
        if (std::strcmp(which, "all") != 0 &&
            std::strcmp(which, MG_FIXTURES[i].name) != 0) continue;
        matched = true;
        if (!mgRunFixture(MG_FIXTURES[i])) all = false;
    }
    if (!matched) {
        std::fprintf(stderr, "unknown fixture '%s'. known: all", which);
        for (int i = 0; i < MG_FIXTURE_COUNT; ++i)
            std::fprintf(stderr, " %s", MG_FIXTURES[i].name);
        std::fprintf(stderr, "\n");
        return 2;
    }
    std::printf("GATE ALL: %s\n", all ? "PASS" : "FAIL");
    return all ? 0 : 1;
}

// --movegen-bench : how many generateMoves calls per second, on a board with
// realistic clutter. PRD 4.5 needs roughly 200,000/sec for a depth-5,
// width-100 beam with hold branching to fit inside 5 ms.
int runMovegenBench(int iters) {
    const tb::Board b = tb::boardFromAscii(MG_FIX_MID, 10);
    static tb::MoveList ml;
    unsigned long long sink = 0;

    std::printf("movegen-bench: board=mid iters=%d\n", iters);

    // Warm the branch predictors and the static arena before timing anything.
    for (int i = 0; i < 1000; ++i) {
        tb::generateMoves(b, tb::PIECE_T, &ml);
        sink += static_cast<unsigned>(ml.count);
    }

    double totalSeconds = 0.0;
    long long totalCalls = 0;
    for (int pi = 0; pi < tb::NUM_PIECES; ++pi) {
        const tb::PieceType p = static_cast<tb::PieceType>(pi);
        tb::generateMoves(b, p, &ml);
        const int placements = ml.count;

        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) {
            tb::generateMoves(b, p, &ml);
            sink += static_cast<unsigned>(ml.count);   // keep the call alive
        }
        const auto t1 = std::chrono::steady_clock::now();

        const double secs = std::chrono::duration<double>(t1 - t0).count();
        totalSeconds += secs;
        totalCalls += iters;
        std::printf("  piece=%d placements=%d us_per_call=%.2f calls_per_sec=%.0f\n",
                    pi, placements, (secs * 1e6) / iters, iters / secs);
    }

    std::printf("  TOTAL us_per_call=%.2f calls_per_sec=%.0f\n",
                (totalSeconds * 1e6) / static_cast<double>(totalCalls),
                static_cast<double>(totalCalls) / totalSeconds);
    std::printf("  (sink=%llu -- ignore, it exists so the calls are not "
                "optimised away)\n", sink);
    std::printf("  PRD 4.5 wants about 200000 calls_per_sec for depth 5 / width 100.\n");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--movegen") == 0) {
            const char* which = (i + 1 < argc) ? argv[i + 1] : "all";
            return runMovegenMode(which);
        }
        if (std::strcmp(argv[i], "--movegen-bench") == 0) {
            const int iters = (i + 1 < argc) ? std::atoi(argv[i + 1]) : 0;
            return runMovegenBench(iters > 0 ? iters : 20000);
        }
    }

    uint32_t seed = 42;
    int pieces = 50;
    bool doPrint = false;
    bool doStats = false;
    bool randomMode = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--pieces") == 0 && i + 1 < argc) {
            // atoi() would turn "abc" into 0 and accept "-5", either of which
            // runs the loop zero times and prints an all-zero stats block that
            // is shape-identical to a real run. Scripted weight-tuning sweeps
            // would silently record that as data, so reject it loudly instead.
            char* end = nullptr;
            const long v = std::strtol(argv[++i], &end, 10);
            if (end == argv[i] || *end != '\0' || v <= 0) {
                std::fprintf(stderr, "--pieces must be a positive integer: %s\n", argv[i]);
                usage();
                return 2;
            }
            pieces = static_cast<int>(v);
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
