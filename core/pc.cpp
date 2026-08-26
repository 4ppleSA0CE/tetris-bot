#include "core/pc.h"
#include "core/board.h"
#include "core/piece.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <memory>
#include <unordered_map>

namespace tb {
namespace {

constexpr int PC_MAX_DEPTH = 16;
constexpr uint8_t FULL_BAG = 0x7F;

bool tooHigh(PieceType p, const Placement& pl, int hRem) {
    const Cell* cs = pieceCells(p, pl.rot);
    for (int i = 0; i < 4; ++i)
        if (pl.y + cs[i].dy >= hRem) return true;
    return false;
}

struct Solver {
    PieceType known[16];
    int       knownLen = 0;
    long      nodes = 0;
    long      nodeBudget = 0;
    bool      aborted = false;
    std::chrono::steady_clock::time_point deadline;
    MoveList  moves[PC_MAX_DEPTH];
    std::unordered_map<uint64_t, float> memo;

    uint64_t stateKey(const Board& b, int hRem, PieceType cur, PieceType hold,
                      int qIdx, uint8_t mask) const {
        uint64_t k = 0;
        for (int y = 0; y < hRem; ++y) k = (k << 10) | b.rows[y];
        k = (k << 3) | static_cast<uint64_t>(hRem);
        k = (k << 3) | static_cast<uint64_t>(cur);
        k = (k << 3) | static_cast<uint64_t>(hold == PIECE_NONE ? 7 : hold);
        const int q = qIdx < knownLen ? qIdx : knownLen;
        k = (k << 4) | static_cast<uint64_t>(q);
        k = (k << 7) | static_cast<uint64_t>(mask);
        return k;
    }

    float expectNext(const Board& b, int hRem, PieceType hold, int qIdx,
                     uint8_t mask, int depth) {
        if (qIdx < knownLen)
            return solveCur(b, hRem, known[qIdx], hold, qIdx + 1, mask, depth);
        // unknowns use guaranteed (min) semantics: the PC must survive every
        // possible draw order; exact expectimax averaging is intractable here
        const uint8_t m = mask ? mask : FULL_BAG;
        float worst = 1.0f;
        for (int p = 0; p < NUM_PIECES; ++p) {
            if (!(m & (1u << p))) continue;
            const float v = solveCur(b, hRem, static_cast<PieceType>(p), hold, qIdx + 1,
                                     static_cast<uint8_t>(m & ~(1u << p)), depth);
            if (v < worst) worst = v;
            if (worst <= 0.0f || aborted) break;
        }
        return worst;
    }

    float placeAndGo(const Board& b, int hRem, PieceType piece, PieceType holdAfter,
                     int qIdx, uint8_t mask, int depth) {
        assert(depth < PC_MAX_DEPTH);
        MoveList& ml = moves[depth];
        generateMoves(b, piece, &ml, false);
        float best = 0.0f;
        for (int i = 0; i < ml.count && !aborted; ++i) {
            if (++nodes > nodeBudget ||
                ((nodes & 2047) == 0 && std::chrono::steady_clock::now() > deadline)) {
                aborted = true;
                break;
            }
            const Placement& pl = ml.items[i];
            if (tooHigh(piece, pl, hRem)) continue;
            Board child = b;
            lockPiece(child, piece, pl.rot, pl.x, pl.y);
            const int lines = clearLines(child);
            if (isEmpty(child)) return 1.0f;
            const int h2 = hRem - lines;
            if (!pcRegionsOk(child, h2)) continue;
            const float v = expectNext(child, h2, holdAfter, qIdx, mask, depth + 1);
            if (v > best) best = v;
            if (best >= 1.0f) break;
        }
        return best;
    }

    float solveCur(const Board& b, int hRem, PieceType cur, PieceType hold,
                   int qIdx, uint8_t mask, int depth) {
        const uint64_t key = stateKey(b, hRem, cur, hold, qIdx, mask);
        const auto it = memo.find(key);
        if (it != memo.end()) return it->second;

        float best = placeAndGo(b, hRem, cur, hold, qIdx, mask, depth);
        if (best < 1.0f && !aborted) {
            if (hold != PIECE_NONE)
                best = std::max(best, placeAndGo(b, hRem, hold, cur, qIdx, mask, depth));
            else
                // stash-current: value-equal to (hold=cur, cur=draw); may double-hold
                // in one turn, which the game cannot, but reachable sets are identical
                best = std::max(best, expectNext(b, hRem, cur, qIdx, mask, depth));
        }
        if (!aborted) memo.emplace(key, best);
        return best;
    }
};

}  // namespace

// necessary, not sufficient: full columns split the window into independently tileable segments
bool pcRegionsOk(const Board& b, int height) {
    int segEmpty = 0;
    for (int c = 0; c < BOARD_W; ++c) {
        int colEmpty = 0;
        for (int y = 0; y < height; ++y)
            if (!(b.rows[y] & static_cast<uint16_t>(1u << c))) ++colEmpty;
        if (colEmpty == 0) {
            if (segEmpty % 4 != 0) return false;
            segEmpty = 0;
        } else {
            segEmpty += colEmpty;
        }
    }
    return segEmpty % 4 == 0;
}

PcResult pcSolveHeight(const Board& b, int height, PieceType current, PieceType hold,
                       const PieceType* queue, int queueLen, uint8_t bagMask,
                       const PcConfig& cfg) {
    assert(height >= 1 && height <= 4);
    bagMask = static_cast<uint8_t>(bagMask & FULL_BAG);
    PcResult res{};
    std::unique_ptr<Solver> s(new Solver);
    if (queueLen > 15) queueLen = 15;
    for (int i = 0; i < queueLen; ++i) s->known[s->knownLen++] = queue[i];
    s->nodeBudget = cfg.nodeBudget > 0 ? cfg.nodeBudget : 1000000000L;
    s->deadline = std::chrono::steady_clock::now() +
                  std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                      std::chrono::duration<float, std::milli>(
                          cfg.timeBudgetMs > 0 ? cfg.timeBudgetMs : 1e9f));

    // with empty hold and empty queue the stash line has no concrete move to
    // return, so it is not offered at the root; real callers pass queueLen >= 5
    struct Root { PieceType piece; PieceType holdAfter; int qIdx; bool useHold; };
    Root opts[3];
    int nOpts = 0;
    opts[nOpts++] = {current, hold, 0, false};
    if (hold != PIECE_NONE) opts[nOpts++] = {hold, current, 0, true};
    else if (queueLen >= 1)  opts[nOpts++] = {queue[0], current, 1, true};

    float bestV = 0.0f;
    Root  bestOpt{};
    Placement bestPl{};
    for (int o = 0; o < nOpts && !s->aborted; ++o) {
        MoveList& ml = s->moves[0];
        generateMoves(b, opts[o].piece, &ml, false);
        for (int i = 0; i < ml.count && !s->aborted; ++i) {
            if (++s->nodes > s->nodeBudget ||
                ((s->nodes & 2047) == 0 &&
                 std::chrono::steady_clock::now() > s->deadline)) {
                s->aborted = true;
                break;
            }
            const Placement& pl = ml.items[i];
            if (tooHigh(opts[o].piece, pl, height)) continue;
            Board child = b;
            lockPiece(child, opts[o].piece, pl.rot, pl.x, pl.y);
            const int lines = clearLines(child);
            float v;
            if (isEmpty(child)) {
                v = 1.0f;
            } else {
                const int h2 = height - lines;
                if (!pcRegionsOk(child, h2)) continue;
                v = s->expectNext(child, h2, opts[o].holdAfter, opts[o].qIdx,
                                  bagMask, 1);
            }
            if (v > bestV) { bestV = v; bestOpt = opts[o]; bestPl = pl; }
            if (bestV >= 1.0f) break;
        }
        if (bestV >= 1.0f) break;
    }

    res.prob    = bestV;
    res.nodes   = s->nodes;
    res.height  = height;
    res.aborted = s->aborted;
    res.valid   = bestV > 0.0f && (bestV >= 1.0f || !s->aborted);
    if (res.valid) {
        res.useHold = bestOpt.useHold;
        // re-generate with paths and pick the exact same landing spot
        bool found = false;
        MoveList full;
        generateMoves(b, bestOpt.piece, &full, true);
        for (int i = 0; i < full.count; ++i) {
            const Placement& pl = full.items[i];
            if (pl.x == bestPl.x && pl.y == bestPl.y && pl.rot == bestPl.rot) {
                res.move = pl;
                found = true;
                break;
            }
        }
        if (!found) res.valid = false;
    }
    return res;
}

PcResult pcSolve(const Board& b, PieceType current, PieceType hold,
                 const PieceType* queue, int queueLen, uint8_t bagMask,
                 const PcConfig& cfg) {
    int maxH = 0;
    for (int c = 0; c < BOARD_W; ++c) {
        const int h = columnHeight(b, c);
        if (h > maxH) maxH = h;
    }
    PcResult best{};
    long totalNodes = 0;
    bool anyAborted = false;
    const long budget = cfg.nodeBudget > 0 ? cfg.nodeBudget : 1000000000L;
    const int supply = 1 + queueLen + (hold != PIECE_NONE ? 1 : 0);
    static const int HEIGHTS[2] = {2, 4};
    // ponytail: heights 2/4 only; add 6-line support if duel data ever demands it
    for (int hi = 0; hi < 2; ++hi) {
        const int H = HEIGHTS[hi];
        if (H < maxH) continue;
        int filled = 0;
        for (int y = 0; y < H; ++y) filled += __builtin_popcount(b.rows[y]);
        const int empties = H * BOARD_W - filled;
        if (empties == 0 || empties % 4 != 0) continue;
        // >1 unknown slot makes the guaranteed search unterminating; a stash
        // with empty hold consumes one extra draw, so this bounds unknowns to 1
        if (empties / 4 > supply) continue;
        if (!pcRegionsOk(b, H)) continue;
        PcConfig hcfg = cfg;
        hcfg.nodeBudget = budget > totalNodes ? budget - totalNodes : 1;
        const PcResult r = pcSolveHeight(b, H, current, hold, queue, queueLen,
                                         bagMask, hcfg);
        totalNodes += r.nodes;
        anyAborted = anyAborted || r.aborted;
        if (r.valid && r.prob > best.prob) best = r;
        if (best.prob >= 1.0f) break;
    }
    best.nodes   = totalNodes;
    best.aborted = anyAborted && best.prob < 1.0f;
    return best;
}

bool pcPlan(const Board& b, PieceType current, PieceType hold,
            const PieceType* queue, int queueLen, uint8_t bagMask,
            int pendingGarbage, const SearchConfig& cfg, SearchResult* out) {
    if (!cfg.pc.enabled || pendingGarbage > 0) return false;
    for (int c = 0; c < BOARD_W; ++c)
        if (columnHeight(b, c) > 4) return false;
    const PcResult r = pcSolve(b, current, hold, queue, queueLen, bagMask, cfg.pc);
    if (!r.valid) return false;
    out->placement = r.move;
    out->useHold   = r.useHold;
    out->valid     = true;
    out->score     = r.prob;
    out->nodes     = r.nodes;
    out->dupes     = 0;
    out->beamSlots = 0;
    return true;
}

}
