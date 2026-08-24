#include "core/search.h"
#include "core/board.h"
#include "core/attack.h"
#include <algorithm>
#include <chrono>

namespace tb {

namespace {

constexpr int   MAX_BEAM         = 256;
constexpr int   MAX_DEPTH        = 8;
constexpr int   MAX_SEQ          = 1 + PREVIEW_LEN + 2;
constexpr long  CLOCK_CHECK_MASK = 31;   // check the clock every 32 scored children

struct Node {
    Board     board;
    PieceType hold;
    int8_t    queueIdx;      // index of the next unplaced piece in seq[]
    uint8_t   b2bCount;
    uint8_t   combo;
    float     pathReward;    // sum of discounted move rewards (attack, plain clear, wasted T)
    float     score;         // pathReward + evaluate(terminal board)
    int16_t   rootIdx;       // which depth-0 move this path started with; -1 at the root
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

// Single-threaded by design: PRD 5.3 rules out pthreads and SharedArrayBuffer, so one static
// scratch buffer is safe and keeps ~70KB of Nodes and MoveLists off the (small) WASM stack.
Scratch g_scratch;

// Comparator that makes std::push_heap/pop_heap maintain a MIN-heap on score, so heap[0] is
// the worst node currently in the beam and is the one a better child evicts.
struct WorseFirst {
    bool operator()(const Node& a, const Node& b) const { return a.score > b.score; }
};

inline double elapsedMs(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - t0).count();
}

// A placement is a lock-out if, after line clears, anything is still sitting at or above the
// top of the visible field. It stays in the beam so a doomed board still yields a legal move,
// but it loses to every placement that keeps the stack inside the well.
constexpr float TOPOUT_PENALTY = -1.0e6f;

inline bool aboveField(const Board& b) {
    for (int y = VISIBLE_H; y < BOARD_H; ++y) if (b.rows[y] != 0) return true;
    return false;
}

} // namespace

SearchResult search(const Board& b, PieceType current, PieceType hold,
                    const PieceType* queue, int queueLen,
                    int b2bCount, int comboCount, const SearchConfig& cfg)
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
    cur[0].pathReward  = 0.0f;
    cur[0].score       = 0.0f;
    cur[0].rootIdx     = -1;
    int curCount = 1;

    int   rootCount = 0;
    // The answer: the winning root of the deepest level that ran to completion.
    float bestScore = 0.0f;
    int   bestRoot  = -1;
    // The level currently being expanded. Reset at every depth boundary; children are only
    // ever compared against other children of the SAME level, because a depth-0 score and a
    // depth-4 score measure different boards and are not on the same scale.
    float levelBest = 0.0f;
    int   levelRoot = -1;
    long  scored    = 0;
    bool  outOfTime = false;
    float discount  = 1.0f;   // gamma^d for the placement made at depth d

    for (int d = 0; d < depth && !outOfTime; ++d) {
        // CLOCK CHECK 1 of 2: depth boundary. Depth 0 is never skipped.
        if (d > 0 && (elapsedMs(t0) > budgetMs || (nodeBudget > 0 && scored >= nodeBudget))) break;

        int nextCount = 0;
        levelBest = 0.0f;
        levelRoot = -1;

        for (int i = 0; i < curCount && !outOfTime; ++i) {
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
                    // identical to branch 0 in every respect; expanding it would clone the beam
                    if (parent.hold == seq[parent.queueIdx]) continue;
                    piece       = parent.hold;
                    newHold     = seq[parent.queueIdx];
                    newQueueIdx = parent.queueIdx + 1;
                }

                generateMoves(parent.board, piece, &S.moves);

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
                    child.b2bCount    = (lines == 0) ? parent.b2bCount
                                      : b2bMaintaining(ci)
                                          ? (uint8_t)(parent.b2bCount < 255 ? parent.b2bCount + 1 : 255)
                                          : (uint8_t)0;
                    child.combo       = (lines > 0)
                                          ? (uint8_t)(parent.combo < 255 ? parent.combo + 1 : 255)
                                          : (uint8_t)0;
                    float reward = cfg.weights.attackDealt * (float)atk;
                    if (lines > 0 && !b2bMaintaining(ci))          reward += cfg.weights.plainClear;
                    if (piece == PIECE_T && pl.spin == SPIN_NONE)  reward += cfg.weights.wastedT;
                    child.pathReward = parent.pathReward + reward * discount;
                    float terminal = evaluate(child.board, cfg.weights, child.b2bCount);
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
                    // BEST OF THIS LEVEL. The levelRoot < 0 guard makes the first child of the
                    // level win no matter how negative it is, so a level always has a candidate.
                    // Nothing here touches bestScore/bestRoot: cross-depth comparison is the bug
                    // this structure exists to prevent.
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

                    // CLOCK CHECK 2 of 2: every 32 scored children, and NEVER at
                    // depth 0. Interrupting the root level leaves the answer as the
                    // best of however many placements happened to be enumerated
                    // first, which is close to arbitrary and is how a starved search
                    // tops the bot out. A complete root sweep costs ~2 generateMoves
                    // plus <=256 feature extractions - about 0.15 ms against a 4.8 ms
                    // budget - so finishing it is affordable even when already over.
                    if (d > 0 && (scored & CLOCK_CHECK_MASK) == 0 &&
                        (elapsedMs(t0) > budgetMs || (nodeBudget > 0 && scored >= nodeBudget))) {
                        outOfTime = true;
                        break;
                    }
                }   // for m
            }       // for branch
        }           // for i

        // PROMOTE. A level that ran to completion replaces the answer, so the returned move is
        // the deepest fully searched level's winner. A level cut short by the clock does not,
        // so an overrun falls back to the last level that did finish -- EXCEPT when depth 0
        // itself was interrupted and there is no answer at all yet, in which case a partially
        // explored depth 0 is still a legal move and that is what "anytime" promises.
        if (levelRoot >= 0 && (!outOfTime || bestRoot < 0)) {
            bestScore = levelBest;
            bestRoot  = levelRoot;
        }

        if (nextCount == 0) break;   // nothing expanded anywhere; keep the promoted answer
        std::swap(cur, next);
        curCount = nextCount;
        discount *= cfg.gamma;
    }

    res.nodes = scored;
    if (bestRoot < 0) return res;    // genuinely no legal placement: valid stays false

    res.placement = S.roots[bestRoot].placement;
    res.useHold   = S.roots[bestRoot].useHold;
    res.score     = bestScore;
    res.valid     = true;
    return res;
}

} // namespace tb
