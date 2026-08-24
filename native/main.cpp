// native/main.cpp -- development CLI harness. Runs the core natively, no browser.
//
//   ./tetris_bot [--seed N] [--pieces N] [--random] [--print] [--stats]
//
// This milestone implements --random only: uniformly random hard-drop
// placements, no search, no evaluation, no spin classification. The search
// milestone adds --depth and --width.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/attack.h"
#include "core/board.h"
#include "core/eval.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/piece.h"
#include "core/rng.h"
#include "core/search.h"
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
                 "usage: tetris_bot [--seed N] [--pieces N] [--stats] [--json] [--print]\n"
                 "                  [--list-weights]\n"
                 "                  [--depth N] [--width N] [--budget MS] [--nodes N] [--heights]\n"
                 "                  [--weights name=value,...] [--garbage L/P] [--messiness F]\n"
                 "       tetris_bot --random [--seed N] [--pieces N] [--print] [--stats]\n"
                 "       tetris_bot --versus \"name=value,...\" [--seed N] [--seed2 M] [--nodes2 K] [--json]\n"
                 "       tetris_bot --movegen <fixture|all>\n"
                 "       tetris_bot --movegen-bench [iters]\n"
                 "\n"
                 "  (default)         drive the bot with search + eval and report --stats\n"
                 "  --random          plan 1's uniformly random hard-drop placements\n"
                 "  --movegen         plan 2's move-generation inspection gate\n"
                 "  --movegen-bench   plan 2's generateMoves throughput benchmark\n");
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

namespace {

// Every numeric flag on this CLI goes through one of these two. std::atoi maps "abc" to 0 and
// accepts "-5"; either runs a degenerate loop and prints a stats block that is shape-identical
// to a real result, and a scripted weight sweep would record that as data. Reject it loudly.
long parseIntArg(const char* flag, const char* text, long lo, long hi) {
    char* end = nullptr;
    const long v = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || v < lo || v > hi) {
        std::fprintf(stderr, "%s must be an integer in [%ld, %ld]: %s\n", flag, lo, hi, text);
        std::exit(2);
    }
    return v;
}

// The !(v > 0.0f) form also rejects a NaN, which every plain comparison would quietly accept.
float parsePositiveFloatArg(const char* flag, const char* text) {
    char* end = nullptr;
    const float v = std::strtof(text, &end);
    if (end == text || *end != '\0' || !(v > 0.0f)) {
        std::fprintf(stderr, "%s must be a positive number: %s\n", flag, text);
        std::exit(2);
    }
    return v;
}

void listWeightNames(std::FILE* out) {
    for (int i = 0; i < tb::weightNameCount(); ++i)
        std::fprintf(out, " %s", tb::weightName(i));
    std::fprintf(out, "\n");
}

// Parses "name=value" or "name=value,name=value,...". Returns false on the first bad token.
bool applyWeightSpec(tb::Weights& w, const char* spec) {
    const std::string s(spec);
    size_t start = 0;
    while (start <= s.size()) {
        const size_t comma = s.find(',', start);
        const std::string tok =
            s.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (!tok.empty()) {
            const size_t eq = tok.find('=');
            if (eq == std::string::npos) {
                std::fprintf(stderr, "bad --weights token '%s' (want name=value)\n", tok.c_str());
                return false;
            }
            const std::string name = tok.substr(0, eq);
            // strtof with a discarded end pointer would read "tSlotCount=abc" as 0 and run a
            // sweep at a weight nobody asked for, so the whole value has to parse.
            const char* valueText = tok.c_str() + eq + 1;
            char* end = nullptr;
            const float value = std::strtof(valueText, &end);
            if (end == valueText || *end != '\0') {
                std::fprintf(stderr, "bad --weights value '%s' in token '%s'\n", valueText,
                             tok.c_str());
                return false;
            }
            if (!tb::setWeightByName(w, name.c_str(), value)) {
                std::fprintf(stderr, "unknown weight '%s'. known:", name.c_str());
                listWeightNames(stderr);
                return false;
            }
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return true;
}

// --versus: two Games in one process, stepping alternately (equal PPS). Attack cancels the
// sender's own pending queue first (inside Game); only the surplus is queued on the opponent,
// entering after the opponent's next lock. First top-out loses; both alive at the cap = draw.
static int runVersusMode(uint32_t seedA, uint32_t seedB, int maxPieces,
                         const tb::SearchConfig& cfgA, const tb::SearchConfig& cfgB,
                         float messiness, bool stats, bool json) {
    tb::Game A(seedA, cfgA);
    tb::Game B(seedB, cfgB);
    A.setMessiness(messiness);
    B.setMessiness(messiness);

    const char* winner = "draw";
    int rounds = 0;
    while (rounds < maxPieces) {
        ++rounds;
        // A steps; its surplus attack (after cancelling its own queue) goes to B.
        int pending = A.pendingGarbage();
        uint32_t before = A.attackSent();
        A.stepPiece();
        if (A.toppedOut()) { winner = "B"; break; }
        int atk = static_cast<int>(A.attackSent() - before);
        B.queueGarbage(atk - (atk < pending ? atk : pending));

        pending = B.pendingGarbage();
        before = B.attackSent();
        B.stepPiece();
        if (B.toppedOut()) { winner = "A"; break; }
        atk = static_cast<int>(B.attackSent() - before);
        A.queueGarbage(atk - (atk < pending ? atk : pending));
    }

    if (stats) {
        std::printf("%-12s%s\n", "winner", winner);
        std::printf("%-12s%5d\n", "rounds", rounds);
        std::printf("%-12s%5u vs %5u\n", "attack", A.attackSent(), B.attackSent());
        std::printf("%-12s%5u vs %5u\n", "garbage", A.garbageReceived(), B.garbageReceived());
        std::printf("%-12s%5u vs %5u\n", "max b2b", A.maxB2b(), B.maxB2b());
    }
    if (json) {
        std::printf("{\"winner\":\"%s\",\"rounds\":%d,\"attackA\":%u,\"attackB\":%u,"
                    "\"garbageA\":%u,\"garbageB\":%u,\"maxB2bA\":%u,\"maxB2bB\":%u}\n",
                    winner, rounds, A.attackSent(), B.attackSent(),
                    A.garbageReceived(), B.garbageReceived(),
                    static_cast<unsigned>(A.maxB2b()), static_cast<unsigned>(B.maxB2b()));
    }
    return 0;
}

} // namespace

// Plan 1's random-play harness, moved verbatim out of main() and otherwise untouched. It is the
// only mode that does not call search(), which is exactly what makes it useful when the search
// itself is the thing under suspicion.
static int runRandomMode(uint32_t seed, int pieces, bool doPrint, bool doStats) {
    tb::Bag bag(seed);
    uint32_t rngState = (seed == 0u) ? 0x9E3779B9u : seed;

    tb::Board board{};
    tb::PieceType queue[tb::PREVIEW_LEN + 1];
    for (int i = 0; i < tb::PREVIEW_LEN + 1; ++i) queue[i] = bag.next();

    tb::PieceType hold = tb::PIECE_NONE;
    int b2bCount = 0;
    int combo = 0;
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
            b2bCount = 0;
            combo = 0;
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

        st.attack += static_cast<uint32_t>(tb::computeAttack(info, b2bCount, combo));
        st.lines += static_cast<uint32_t>(cleared);
        if (info.spin != tb::SPIN_NONE && cleared > 0) ++st.tspins;

        if (cleared > 0) {
            b2bCount = tb::b2bMaintaining(info) ? b2bCount + 1 : 0;
            ++combo;
        } else {
            combo = 0;
        }
        if (b2bCount > 1 && static_cast<uint32_t>(b2bCount - 1) > st.maxB2b) st.maxB2b = static_cast<uint32_t>(b2bCount - 1);
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
        std::printf("%-12s%5u   (%.2f / 100)\n", "spins", st.tspins, tspinRate);
        std::printf("%-12s%5u\n", "max b2b", st.maxB2b);
        std::printf("%-12s%5u\n", "top-outs", st.topouts);
    }


    return 0;
}

int main(int argc, char** argv) {
    // PLAN 2'S DISPATCH, UNCHANGED AND STILL THE FIRST STATEMENT IN main(). --movegen is the
    // PRD 10 gate; it must keep working forever. It never touches the search, which is exactly
    // why it stays trustworthy when the search is the thing under suspicion.
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

    uint32_t seed   = 1;
    uint32_t seed2  = 0;
    bool     seed2Set = false;
    long     nodes2 = 0;
    const char* versusSpec = nullptr;
    int      pieces = 100;
    bool     stats = false, print = false, heights = false, randomMode = false, json = false;
    int      garbageLines = 0, garbageEvery = 0;
    float    messiness = 0.05f;
    tb::SearchConfig cfg;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto need = [&](const char* what) -> const char* {
            if (i + 1 >= argc) { std::fprintf(stderr, "%s needs a value\n", what); std::exit(2); }
            return argv[++i];
        };
        if      (!std::strcmp(a, "--seed"))
            seed = static_cast<uint32_t>(parseIntArg("--seed", need("--seed"), 0, 4294967295L));
        else if (!std::strcmp(a, "--pieces"))
            pieces = static_cast<int>(parseIntArg("--pieces", need("--pieces"), 1, 2147483647L));
        else if (!std::strcmp(a, "--depth"))
            cfg.depth = static_cast<int>(parseIntArg("--depth", need("--depth"), 1, 2147483647L));
        else if (!std::strcmp(a, "--width"))
            cfg.beamWidth = static_cast<int>(parseIntArg("--width", need("--width"), 1, 2147483647L));
        else if (!std::strcmp(a, "--budget"))
            cfg.timeBudgetMs = parsePositiveFloatArg("--budget", need("--budget"));
        else if (!std::strcmp(a, "--nodes")) {
            // Deterministic horizon: N scored children per search, clock effectively off.
            cfg.nodeBudget   = parseIntArg("--nodes", need("--nodes"), 1, 2147483647L);
            cfg.timeBudgetMs = 1e9f;
        }
        else if (!std::strcmp(a, "--weights")) { if (!applyWeightSpec(cfg.weights, need("--weights"))) return 2; }
        else if (!std::strcmp(a, "--stats"))   stats = true;
        else if (!std::strcmp(a, "--dupes"))   cfg.measureDupes = true;
        else if (!std::strcmp(a, "--json"))    json = true;
        else if (!std::strcmp(a, "--list-weights")) {
            // name=default per line; tools/tune.py and tools/bench.py read the table from here
            const tb::Weights d = tb::defaultWeights();
            for (int k = 0; k < tb::weightNameCount(); ++k)
                std::printf("%s=%g\n", tb::weightName(k), static_cast<double>(tb::weightValue(d, k)));
            return 0;
        }
        else if (!std::strcmp(a, "--garbage")) {
            // "L/P": queue L garbage lines every P pieces.
            const char* spec = need("--garbage");
            const char* slash = std::strchr(spec, '/');
            if (slash == nullptr) { std::fprintf(stderr, "--garbage wants L/P: %s\n", spec); return 2; }
            garbageLines = static_cast<int>(parseIntArg("--garbage lines", std::string(spec, slash).c_str(), 1, 20));
            garbageEvery = static_cast<int>(parseIntArg("--garbage pieces", slash + 1, 1, 2147483647L));
        }
        else if (!std::strcmp(a, "--versus"))  versusSpec = need("--versus");
        else if (!std::strcmp(a, "--nodes2"))
            nodes2 = parseIntArg("--nodes2", need("--nodes2"), 1, 100000000L);
        else if (!std::strcmp(a, "--seed2")) {
            seed2 = static_cast<uint32_t>(parseIntArg("--seed2", need("--seed2"), 0, 4294967295L));
            seed2Set = true;
        }
        else if (!std::strcmp(a, "--messiness")) {
            messiness = parsePositiveFloatArg("--messiness", need("--messiness"));
            if (messiness > 1.0f) { std::fprintf(stderr, "--messiness must be <= 1\n"); return 2; }
        }
        else if (!std::strcmp(a, "--heights")) heights = true;
        else if (!std::strcmp(a, "--print"))   print = true;
        else if (!std::strcmp(a, "--random"))  randomMode = true;
        else {
            std::fprintf(stderr, "unknown flag '%s'\n", a);
            usage();
            std::fprintf(stderr, "weights:");
            listWeightNames(stderr);
            return 2;
        }
    }

    // PLAN 1'S MODE, still reachable and still meaning what it meant.
    if (randomMode) return runRandomMode(seed, pieces, print, stats);

    if (versusSpec != nullptr) {
        tb::SearchConfig cfgB = cfg;
        cfgB.weights = tb::defaultWeights();   // B starts clean; --weights only shapes A
        if (versusSpec[0] != '\0' && !applyWeightSpec(cfgB.weights, versusSpec)) return 2;
        if (nodes2 > 0) { cfgB.nodeBudget = nodes2; cfgB.timeBudgetMs = 1000000000.0f; }
        return runVersusMode(seed, seed2Set ? seed2 : seed + 7777u, pieces,
                             cfg, cfgB, messiness, stats || !json, json);
    }

    tb::Game game(seed, cfg);
    game.setMessiness(messiness);

    std::vector<float> times;
    times.reserve(static_cast<size_t>(pieces));
    double nodeSum = 0.0;
    double dupeSum = 0.0, slotSum = 0.0;

    uint32_t topOuts = 0;
    uint32_t basePieces = 0, baseLines = 0, baseAttack = 0, baseTspins = 0, baseSurge = 0;
    uint32_t baseGarbage = 0;
    uint16_t maxB2b = 0;
    double   hSum = 0.0, hSumSq = 0.0;
    int      hMin = tb::BOARD_H + 1, hMax = 0;

    while (basePieces + game.piecesPlaced() < static_cast<uint32_t>(pieces)) {
        const uint32_t before = game.piecesPlaced();
        if (garbageEvery > 0 && (basePieces + before) % static_cast<uint32_t>(garbageEvery) == 0
            && basePieces + before > 0)
            game.queueGarbage(garbageLines);
        game.stepPiece();
        if (game.piecesPlaced() != before) {
            times.push_back(game.lastSearchMs());
            nodeSum += static_cast<double>(game.lastSearchNodes());
            dupeSum += static_cast<double>(game.lastSearchDupes());
            slotSum += static_cast<double>(game.lastSearchBeamSlots());
            if (game.maxB2b() > maxB2b) maxB2b = game.maxB2b();
            int h = 0;
            for (int c = 0; c < tb::BOARD_W; ++c) {
                const int hc = tb::columnHeight(game.board(), c);
                if (hc > h) h = hc;
            }
            hSum   += static_cast<double>(h);
            hSumSq += static_cast<double>(h) * static_cast<double>(h);
            if (h < hMin) hMin = h;
            if (h > hMax) hMax = h;
            // plan 1's printBoard, reused: same "--- piece N ---" header, same frame.
            if (print) printBoard(game.board(), static_cast<unsigned>(basePieces + game.piecesPlaced()));
        }
        if (game.toppedOut()) {
            ++topOuts;
            basePieces += game.piecesPlaced();
            baseLines  += game.linesCleared();
            baseAttack += game.attackSent();
            baseTspins += game.tSpinCount();
            baseSurge  += game.surgeSent();
            baseGarbage += game.garbageReceived();
            game.reset(seed + topOuts);
        }
    }

    const uint32_t totalPieces = basePieces + game.piecesPlaced();
    const uint32_t totalLines  = baseLines  + game.linesCleared();
    const uint32_t totalAttack = baseAttack + game.attackSent();
    const uint32_t totalTspins = baseTspins + game.tSpinCount();
    const uint32_t totalSurge  = baseSurge  + game.surgeSent();
    const uint32_t totalGarbage = baseGarbage + game.garbageReceived();

    std::sort(times.begin(), times.end());
    auto pct = [&](double p) -> double {
        if (times.empty()) return 0.0;
        size_t idx = static_cast<size_t>(p * static_cast<double>(times.size() - 1) + 0.5);
        if (idx >= times.size()) idx = times.size() - 1;
        return static_cast<double>(times[idx]);
    };
    const double hN   = static_cast<double>(times.size());
    const double hAvg = hN > 0.0 ? hSum / hN : 0.0;
    double hVar = hN > 0.0 ? (hSumSq / hN - hAvg * hAvg) : 0.0;
    if (hVar < 0.0) hVar = 0.0;

    if (stats) {
        const double rate = totalPieces ? 100.0 * static_cast<double>(totalTspins) /
                                              static_cast<double>(totalPieces)
                                        : 0.0;
        std::printf("%-12s%5u\n", "pieces", totalPieces);
        std::printf("%-12s%5u\n", "lines",  totalLines);
        std::printf("%-12s%5u\n", "attack", totalAttack);
        std::printf("%-12s%5u   (%.2f / 100)\n", "spins", totalTspins, rate);
        std::printf("%-12s%5u\n", "max b2b", static_cast<unsigned>(maxB2b));
        std::printf("%-12s%5u\n", "surge", totalSurge);
        if (garbageEvery > 0) std::printf("%-12s%5u\n", "garbage", totalGarbage);
        std::printf("%-12s%5u\n", "top-outs", topOuts);
        std::printf("%-12sp50 %.1f  p99 %.1f\n", "search ms", pct(0.50), pct(0.99));
        std::printf("%-12s%5.0f   per piece\n", "nodes", hN > 0.0 ? nodeSum / hN : 0.0);
        if (slotSum > 0.0)
            std::printf("%-12s%5.2f%%  of beam slots\n", "dupes", 100.0 * dupeSum / slotSum);
    }

    if (heights) {
        std::printf("%-12s%5.2f\n", "avg height", hAvg);
        std::printf("%-12s%5d\n",   "min height", hMin > tb::BOARD_H ? 0 : hMin);
        std::printf("%-12s%5d\n",   "max height", hMax);
        std::printf("%-12s%5.2f\n", "height sd",  std::sqrt(hVar));
    }

    // Machine-readable summary for tools/bench.py and tools/tune.py. One object, one line.
    if (json) {
        std::printf("{\"pieces\":%u,\"lines\":%u,\"attack\":%u,\"spins\":%u,\"maxB2b\":%u,"
                    "\"surge\":%u,\"topouts\":%u,\"garbage\":%u,\"p50\":%.3f,\"p99\":%.3f,"
                    "\"avgHeight\":%.3f,\"maxHeight\":%d,\"nodes\":%.0f}\n",
                    totalPieces, totalLines, totalAttack, totalTspins,
                    static_cast<unsigned>(maxB2b), totalSurge, topOuts, totalGarbage,
                    pct(0.50), pct(0.99), hAvg, hMax, hN > 0.0 ? nodeSum / hN : 0.0);
    }
    return 0;
}
