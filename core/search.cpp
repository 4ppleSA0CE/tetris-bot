#include "core/search.h"
#include "core/board.h"
#include "core/attack.h"
#include "core/pc.h"
#include <algorithm>
#include <chrono>

namespace tb {

namespace {

constexpr int   MAX_BEAM         = 256;
constexpr int   MAX_DEPTH        = 8;
constexpr int   MAX_SEQ          = 1 + PREVIEW_LEN + 2;
constexpr long  CLOCK_CHECK_MASK = 31;

struct Node {
    Board     board;
    PieceType hold;
    int8_t    queueIdx;
    uint8_t   b2bCount;
    uint8_t   combo;
    uint8_t   remaining;
    float     pathReward;
    float     score;
    int16_t   rootIdx;
};

struct RootMove {
    Placement placement;
    bool      useHold;
};

struct Scratch {
    Node     beamA[MAX_BEAM];
    Node     beamB[MAX_BEAM];
    RootMove roots[2 * MAX_PLACEMENTS];
    MoveList moves;
};

Scratch g_scratch;

struct WorseFirst {
    bool operator()(const Node& a, const Node& b) const { return a.score > b.score; }
};

inline double elapsedMs(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - t0).count();
}

constexpr float TOPOUT_PENALTY = -1.0e6f;

inline bool aboveField(const Board& b) {
    for (int y = VISIBLE_H; y < BOARD_H; ++y) if (b.rows[y] != 0) return true;
    return false;
}

inline uint64_t stateHash(const Node& n) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    for (int y = 0; y < BOARD_H; ++y) mix(n.board.rows[y]);
    mix((uint64_t)(uint8_t)n.hold);
    mix((uint64_t)(uint8_t)n.queueIdx);
    mix(n.b2bCount);
    mix(n.combo);
    mix(n.remaining);
    return h;
}

}

SearchResult search(const Board& b, PieceType current, PieceType hold,
                    const PieceType* queue, int queueLen,
                    int b2bCount, int comboCount, const SearchConfig& cfg,
                    int incoming)
{
    const auto t0 = std::chrono::steady_clock::now();
    const double budgetMs   = (double)cfg.timeBudgetMs;
    const long   nodeBudget = cfg.nodeBudget;

    SearchResult res;
    res.placement = Placement{};
    res.useHold   = false;
    res.score     = 0.0f;
    res.valid     = false;
    res.nodes     = 0;
    res.dupes     = 0;
    res.beamSlots = 0;

    Scratch& S = g_scratch;

    PieceType seq[MAX_SEQ];
    int seqLen = 0;
    seq[seqLen++] = current;
    for (int i = 0; i < queueLen && seqLen < MAX_SEQ; ++i) seq[seqLen++] = queue[i];

    int beamWidth = cfg.beamWidth;
    if (beamWidth < 1)         beamWidth = 1;
    if (beamWidth > MAX_BEAM)  beamWidth = MAX_BEAM;
    int depth = cfg.depth;
    if (depth < 1)             depth = 1;
    if (depth > MAX_DEPTH)     depth = MAX_DEPTH;

    Node* cur  = S.beamA;
    Node* next = S.beamB;

    cur[0].board       = b;
    cur[0].hold        = hold;
    cur[0].queueIdx    = 0;
    cur[0].b2bCount    = (uint8_t)(b2bCount < 0 ? 0 : (b2bCount > 255 ? 255 : b2bCount));
    cur[0].combo       = (uint8_t)(comboCount < 0 ? 0 : (comboCount > 255 ? 255 : comboCount));
    cur[0].remaining   = (uint8_t)(incoming < 0 ? 0 : (incoming > 255 ? 255 : incoming));
    cur[0].pathReward  = 0.0f;
    cur[0].score       = 0.0f;
    cur[0].rootIdx     = -1;
    int curCount = 1;

    int   rootCount = 0;

    float bestScore = 0.0f;
    int   bestRoot  = -1;

    float levelBest = 0.0f;
    int   levelRoot = -1;
    long  scored    = 0;
    bool  outOfTime = false;
    float discount  = 1.0f;

    for (int d = 0; d < depth && !outOfTime; ++d) {

        if (d > 0 && (elapsedMs(t0) > budgetMs || (nodeBudget > 0 && scored >= nodeBudget))) break;

        int nextCount = 0;
        levelBest = 0.0f;
        levelRoot = -1;

        constexpr int ITSIZE = 8192;
        static uint64_t ihash[ITSIZE];
        static float    ireward[ITSIZE];
        static bool     iused[ITSIZE];
        int itCount = 0;
        for (int t = 0; t < ITSIZE; ++t) iused[t] = false;

        bool skip[MAX_BEAM];
        if (d > 0 && curCount > 1) {
            constexpr int TSIZE = 512;
            int16_t  table[TSIZE];
            uint64_t hashes[MAX_BEAM];
            for (int t = 0; t < TSIZE; ++t) table[t] = -1;
            for (int p2 = 0; p2 < curCount; ++p2) {
                skip[p2] = false;
                const uint64_t h = stateHash(cur[p2]);
                hashes[p2] = h;
                int slot = (int)(h & (uint64_t)(TSIZE - 1));
                for (;;) {
                    const int16_t o = table[slot];
                    if (o < 0) { table[slot] = (int16_t)p2; break; }
                    if (hashes[o] == h) {
                        if (cur[p2].score > cur[o].score) { skip[o] = true; table[slot] = (int16_t)p2; }
                        else                              { skip[p2] = true; }
                        break;
                    }
                    slot = (slot + 1) & (TSIZE - 1);
                }
            }
        } else {
            for (int p2 = 0; p2 < curCount; ++p2) skip[p2] = false;
        }

        for (int i = 0; i < curCount && !outOfTime; ++i) {
            if (skip[i]) continue;
            const Node& parent = cur[i];

            for (int branch = 0; branch < 2 && !outOfTime; ++branch) {
                PieceType piece;
                PieceType newHold;
                int       newQueueIdx;

                if (branch == 0) {
                    if (parent.queueIdx >= seqLen) continue;
                    piece       = seq[parent.queueIdx];
                    newHold     = parent.hold;
                    newQueueIdx = parent.queueIdx + 1;
                } else if (parent.hold == PIECE_NONE) {
                    if (parent.queueIdx + 1 >= seqLen) continue;
                    piece       = seq[parent.queueIdx + 1];
                    newHold     = seq[parent.queueIdx];
                    newQueueIdx = parent.queueIdx + 2;
                } else {
                    if (parent.queueIdx >= seqLen) continue;

                    if (parent.hold == seq[parent.queueIdx]) continue;
                    piece       = parent.hold;
                    newHold     = seq[parent.queueIdx];
                    newQueueIdx = parent.queueIdx + 1;
                }

                generateMoves(parent.board, piece, &S.moves, d == 0);

                for (int m = 0; m < S.moves.count; ++m) {
                    const Placement& pl = S.moves.items[m];

                    Node child;
                    child.board = parent.board;
                    lockPiece(child.board, piece, pl.rot, pl.x, pl.y);
                    const int lines = clearLines(child.board);

                    ClearInfo ci;
                    ci.lines        = (uint8_t)lines;
                    ci.spin         = pl.spin;
                    ci.perfectClear = (lines > 0) && isEmpty(child.board);
                    const int atk = computeAttack(ci, parent.b2bCount, parent.combo);

                    child.hold        = newHold;
                    child.queueIdx    = (int8_t)newQueueIdx;
                    const int nb      = b2bAfterClear(ci, parent.b2bCount);
                    child.b2bCount    = (uint8_t)(nb > 255 ? 255 : nb);
                    child.combo       = (lines > 0)
                                          ? (uint8_t)(parent.combo < 255 ? parent.combo + 1 : 255)
                                          : (uint8_t)0;
                    child.remaining   = (uint8_t)(parent.remaining > atk
                                          ? parent.remaining - atk : 0);
                    float reward = cfg.weights.attackDealt * (float)atk;
                    if (lines > 0 && !b2bMaintaining(ci)) {
                        reward += cfg.weights.plainClear;

                        if (parent.b2bCount > 0) reward += cfg.weights.b2bBreak;
                    }
                    if (piece == PIECE_T && pl.spin == SPIN_NONE)  reward += cfg.weights.wastedT;
                    child.pathReward = parent.pathReward + reward * discount;

                    if (itCount < ITSIZE - ITSIZE / 8) {
                        const uint64_t chash = stateHash(child);
                        int slot = (int)(chash & (uint64_t)(ITSIZE - 1));
                        bool dominated = false;
                        for (;;) {
                            if (!iused[slot]) {
                                iused[slot]   = true;
                                ihash[slot]   = chash;
                                ireward[slot] = child.pathReward;
                                ++itCount;
                                break;
                            }
                            if (ihash[slot] == chash) {
                                if (child.pathReward <= ireward[slot]) dominated = true;
                                else ireward[slot] = child.pathReward;
                                break;
                            }
                            slot = (slot + 1) & (ITSIZE - 1);
                        }
                        if (dominated) continue;
                    }

                    int tAvail = (newHold == PIECE_T) ? 1 : 0;
                    for (int q = newQueueIdx; q < seqLen; ++q) {
                        if (seq[q] == PIECE_T && ++tAvail >= 2) break;
                    }
                    float terminal = evaluate(child.board, cfg.weights, child.b2bCount,
                                              (int)child.remaining, tAvail);
                    if (cfg.weights.pcNext != 0.0f) {
                        const PieceType nextUp = (newQueueIdx < seqLen) ? seq[newQueueIdx]
                                                                        : PIECE_NONE;
                        if (pcNextPiece(child.board, nextUp) ||
                            pcNextPiece(child.board, child.hold))
                            terminal += cfg.weights.pcNext;
                    }
                    if (aboveField(child.board)) terminal += TOPOUT_PENALTY;
                    child.score = child.pathReward + terminal;

                    if (d == 0) {
                        if (rootCount >= (int)(sizeof(S.roots) / sizeof(S.roots[0]))) continue;
                        S.roots[rootCount].placement = pl;
                        S.roots[rootCount].useHold   = (branch == 1);
                        child.rootIdx = (int16_t)rootCount;
                        ++rootCount;
                    } else {
                        child.rootIdx = parent.rootIdx;
                    }

                    ++scored;

                    if (child.rootIdx >= 0 && (levelRoot < 0 || child.score > levelBest)) {
                        levelBest = child.score;
                        levelRoot = child.rootIdx;
                    }

                    if (nextCount < beamWidth) {
                        next[nextCount++] = child;
                        std::push_heap(next, next + nextCount, WorseFirst{});
                    } else if (child.score > next[0].score) {
                        std::pop_heap(next, next + nextCount, WorseFirst{});
                        next[nextCount - 1] = child;
                        std::push_heap(next, next + nextCount, WorseFirst{});
                    }

                    if (d > 0 && (scored & CLOCK_CHECK_MASK) == 0 &&
                        (elapsedMs(t0) > budgetMs || (nodeBudget > 0 && scored >= nodeBudget))) {
                        outOfTime = true;
                        break;
                    }
                }
            }
        }

        if (levelRoot >= 0 && (!outOfTime || bestRoot < 0)) {
            bestScore = levelBest;
            bestRoot  = levelRoot;
        }

        if (nextCount == 0) break;
        if (cfg.measureDupes) {
            uint64_t seen[MAX_BEAM];
            int seenCount = 0;
            for (int i2 = 0; i2 < nextCount; ++i2) {
                const uint64_t h = stateHash(next[i2]);
                bool dup = false;
                for (int j2 = 0; j2 < seenCount; ++j2) if (seen[j2] == h) { dup = true; break; }
                if (dup) ++res.dupes; else seen[seenCount++] = h;
            }
            res.beamSlots += nextCount;
        }
        std::swap(cur, next);
        curCount = nextCount;
        discount *= cfg.gamma;
    }

    res.nodes = scored;
    if (bestRoot < 0) return res;

    res.placement = S.roots[bestRoot].placement;
    res.useHold   = S.roots[bestRoot].useHold;
    res.score     = bestScore;
    res.valid     = true;
    return res;
}

}
