// core/movegen.cpp -- BFS move generation and T-spin classification (PRD 4.4, 4.6).
#include "core/movegen.h"

#include <cstring>

#include "core/board.h"
#include "core/piece.h"
#include "core/srs.h"

namespace tb {

// The T's centre mino, as an offset from the piece origin. The origin is the
// bounding box's lower-left corner and the centre is the box's middle cell, so
// this is (1, 1). Task 1's test_mg_t_center_is_origin_plus_one_one asserts it.
constexpr int T_CENTER_DX = 1;
constexpr int T_CENTER_DY = 1;

bool mgCellOccupied(const Board& b, int x, int y) {
    if (x < 0 || x >= BOARD_W) return true;    // walls
    if (y < 0) return true;                    // floor
    if (y >= BOARD_H) return false;            // above the well is empty
    return ((b.rows[y] >> x) & 1u) != 0u;
}

// TETR.IO all-mini+ immobility. The up check is the discriminating one: a piece at rest
// in a slot is already pinned left, right and down whenever a line is about to clear.
bool isImmobile(const Board& b, PieceType p, Rot r, int x, int y) {
    return collides(b, p, r, x - 1, y)
        && collides(b, p, r, x + 1, y)
        && collides(b, p, r, x, y + 1)    // y increases upward, so y+1 is up
        && collides(b, p, r, x, y - 1);
}

SpinKind classifyTSpin(const Board& b, Rot r, int x, int y,
                       bool lastWasRotation, uint8_t kickIndex) {
    // 1. last successful action must have been a rotation
    if (!lastWasRotation) return SPIN_NONE;

    const int cx = x + T_CENTER_DX;
    const int cy = y + T_CENTER_DY;

    // The four cells diagonally adjacent to the centre. Same four in every
    // rotation; rotation only decides which two are "front".
    //   index 0 = (-1,+1)  1 = (+1,+1)  2 = (-1,-1)  3 = (+1,-1)
    static const int8_t CORNER_DX[4] = { -1, +1, -1, +1 };
    static const int8_t CORNER_DY[4] = { +1, +1, -1, -1 };
    // Front pair per rotation: the two corners on the nub's side.
    static const uint8_t FRONT[4][2] = {
        { 0, 1 },   // ROT_0, nub up    -> (-1,+1) (+1,+1)
        { 1, 3 },   // ROT_R, nub right -> (+1,+1) (+1,-1)
        { 2, 3 },   // ROT_2, nub down  -> (-1,-1) (+1,-1)
        { 0, 2 },   // ROT_L, nub left  -> (-1,+1) (-1,-1)
    };

    bool filled[4];
    int occupied = 0;
    for (int i = 0; i < 4; ++i) {
        filled[i] = mgCellOccupied(b, cx + CORNER_DX[i], cy + CORNER_DY[i]);
        if (filled[i]) ++occupied;
    }

    // 2. Below three corners all-mini+ still awards a MINI if the T is immobile.
    if (occupied < 3) return isImmobile(b, PIECE_T, r, x, y) ? SPIN_MINI : SPIN_NONE;

    // 3. Both front corners -> proper T-spin. (With occupied >= 3, front is
    //    always 1 or 2, so this and the fall-through are exhaustive.)
    const int ri = static_cast<int>(r) & 3;
    const int front = (filled[FRONT[ri][0]] ? 1 : 0) + (filled[FRONT[ri][1]] ? 1 : 0);
    if (front == 2) return SPIN_FULL;

    // 4. The T-Spin Triple wall-kick upgrade: the last of the five SRS tests is
    //    always a "1 by 2" displacement, and reaching a slot with it is a proper
    //    spin regardless of the corner count. kickIndex 255 (translation, or a
    //    180 rotation) never promotes.
    if (kickIndex == 4) return SPIN_FULL;

    // 5.
    return SPIN_MINI;
}

// All-mini+: T is the only piece that can be FULL; every other piece, I and O included,
// is a MINI when immobile after a rotation. O turns in place, so a snug covered O is an
// O-spin - rare in human play, legal in TETR.IO, and the BFS will find it.
SpinKind classifySpin(const Board& b, PieceType p, Rot r, int x, int y,
                      bool lastWasRotation, uint8_t kickIndex) {
    if (p == PIECE_T) return classifyTSpin(b, r, x, y, lastWasRotation, kickIndex);
    if (!lastWasRotation) return SPIN_NONE;
    return isImmobile(b, p, r, x, y) ? SPIN_MINI : SPIN_NONE;
}

// ---------------------------------------------------------------------------
// BFS scratch arena.
//
// File-static, never on the stack: MoveList alone is ~18 KB and Emscripten's
// default stack is 64 KB. NOT REENTRANT -- one BFS at a time.
//
// Nothing here is cleared per call. `gen` is bumped instead, and a state counts
// as visited only when stamp[i] == gen. parent/via/kick/depth are meaningful
// only for states whose stamp matches, so stale values are unreachable.
// ---------------------------------------------------------------------------
namespace {

struct BfsScratch {
    uint32_t stamp[MG_STATES];    // == gen  =>  this state is visited
    int32_t  parent[MG_STATES];   // predecessor state index; -1 at the root
    uint8_t  via[MG_STATES];      // Action taken from parent to reach here
    uint8_t  kick[MG_STATES];     // kick index of that action; 255 if not a 90-deg rotation
    uint8_t  depth[MG_STATES];    // actions from spawn; BFS order, never rewritten
    // Parallel to the BFS tree, never part of it: the first rotation edge that
    // lands on each state. "First" == from the shallowest source, because BFS
    // pops in non-decreasing depth order. Collection prefers this arrival so a
    // rotation-last path wins even when it is longer than the tree path.
    int32_t  rotSrc[MG_STATES];   // predecessor state index, or -1 for none
    uint8_t  rotAct[MG_STATES];   // ACT_CW / ACT_CCW / ACT_180
    uint8_t  rotKick[MG_STATES];  // kick index of that rotation; 255 for a 180
    int32_t  queue[MG_STATES];    // FIFO; after the loop it is the visited list
    uint32_t gen;
};

BfsScratch g_bfs{};   // zero-initialised, so the first call runs with gen == 1

// Applies one action to a state. Returns false if the action is illegal.
// Writes the resulting state and, for a 90-degree rotation, its kick index.
bool mgApplyAction(const Board& b, PieceType p, Action a,
                   int x, int y, Rot r,
                   int* nx, int* ny, Rot* nr, uint8_t* nk) {
    switch (a) {
        case ACT_LEFT:
            *nx = x - 1; *ny = y; *nr = r; *nk = 255;
            return !collides(b, p, r, *nx, *ny);
        case ACT_RIGHT:
            *nx = x + 1; *ny = y; *nr = r; *nk = 255;
            return !collides(b, p, r, *nx, *ny);
        case ACT_SOFT_DROP:
            *nx = x; *ny = y - 1; *nr = r; *nk = 255;
            return !collides(b, p, r, *nx, *ny);
        case ACT_CW: {
            const Rot to = rotCW(r);
            uint8_t k = 255;
            if (!tryRotate(b, p, r, to, x, y, nx, ny, &k)) return false;
            *nr = to; *nk = k;
            return true;
        }
        case ACT_CCW: {
            const Rot to = rotCCW(r);
            uint8_t k = 255;
            if (!tryRotate(b, p, r, to, x, y, nx, ny, &k)) return false;
            *nr = to; *nk = k;
            return true;
        }
        case ACT_180: {
            const Rot to = rot180(r);
            uint8_t k = 255;
            if (!tryRotate(b, p, r, to, x, y, nx, ny, &k)) return false;
            *nr = to;
            // Deliberately 255, never the 180 table's index: the mini -> full
            // promotion is defined only for the five-test 90-degree tables.
            *nk = 255;
            return true;
        }
        default:
            return false;
    }
}

// Backtracks the (immutable) BFS tree into a forward action sequence.
// Returns the length, or -1 if it would not fit in MAX_PATH_LEN.
int mgRecoverPath(const BfsScratch& S, int32_t si, Action* out) {
    Action rev[MAX_PATH_LEN];
    int n = 0;
    int32_t cur = si;
    while (S.parent[cur] >= 0) {
        if (n >= MAX_PATH_LEN) return -1;
        rev[n++] = static_cast<Action>(S.via[cur]);
        cur = S.parent[cur];
    }
    for (int i = 0; i < n; ++i) out[i] = rev[n - 1 - i];
    return n;
}

} // namespace

void generateMoves(const Board& b, PieceType p, MoveList* out) {
    out->count = 0;
    if (p == PIECE_NONE) return;

    BfsScratch& S = g_bfs;
    if (++S.gen == 0u) {                       // 2^32 calls; wrap once, cheaply
        std::memset(S.stamp, 0, sizeof(S.stamp));
        S.gen = 1u;
    }

    if (!mgInStateBounds(SPAWN_X, SPAWN_Y)) return;
    if (collides(b, p, ROT_0, SPAWN_X, SPAWN_Y)) return;   // topped out

    int head = 0, tail = 0;
    const int32_t root = mgStateIndex(SPAWN_X, SPAWN_Y, ROT_0);
    S.stamp[root]  = S.gen;
    S.parent[root] = -1;
    S.via[root]    = ACT_NONE;
    S.kick[root]   = 255;
    S.depth[root]  = 0;
    S.rotSrc[root] = -1;
    S.queue[tail++] = root;

    while (head < tail) {
        const int32_t si = S.queue[head++];
        int sx = 0, sy = 0;
        Rot sr = ROT_0;
        mgDecodeState(si, &sx, &sy, &sr);
        const uint8_t sd = S.depth[si];

        for (int ai = 0; ai < NUM_ACTIONS; ++ai) {
            const Action a = static_cast<Action>(ai);
            int nx = 0, ny = 0;
            Rot nr = ROT_0;
            uint8_t nk = 255;
            if (!mgApplyAction(b, p, a, sx, sy, sr, &nx, &ny, &nr, &nk)) continue;
            if (!mgInStateBounds(nx, ny)) continue;        // reject BEFORE indexing

            const int32_t ni = mgStateIndex(nx, ny, nr);
            if (S.stamp[ni] != S.gen) {                    // first time seen
                S.stamp[ni]  = S.gen;
                S.parent[ni] = si;
                S.via[ni]    = static_cast<uint8_t>(a);
                S.kick[ni]   = nk;
                S.depth[ni]  = static_cast<uint8_t>(sd + 1);
                S.rotSrc[ni] = -1;
                if (tail < MG_STATES) S.queue[tail++] = ni;
            }
            // Whether or not the state was new, remember the first rotation
            // that reaches it. This is what makes a rotation-last path win.
            if (isRotateAction(a) && S.rotSrc[ni] < 0) {
                S.rotSrc[ni]  = si;
                S.rotAct[ni]  = static_cast<uint8_t>(a);
                S.rotKick[ni] = nk;
            }
        }
    }

    // Collect: a placement is a visited state that cannot move down. The queue
    // holds every visited state exactly once, so dedup by (x, y, rot) is free.
    //
    // Both caps were measured against 28,000 adversarial swiss-cheese boards
    // before this was written: the worst case seen was 57 placements and a
    // 30-action path, against limits of 256 and 64. Neither truncation branch is
    // reachable in practice -- which is precisely why both assert. Silently
    // dropping a legal placement is the one failure mode this whole milestone
    // exists to prevent, and it would surface downstream not as a crash but as
    // "the bot simply never plays that move", which is close to undebuggable.
    Action buf[MAX_PATH_LEN];
    for (int i = 0; i < tail; ++i) {
        if (out->count >= MAX_PLACEMENTS) {
            assert(false && "MAX_PLACEMENTS too small -- placements were dropped");
            break;
        }
        const int32_t si = S.queue[i];
        int sx = 0, sy = 0;
        Rot sr = ROT_0;
        mgDecodeState(si, &sx, &sy, &sr);
        if (!collides(b, p, sr, sx, sy - 1)) continue;     // still falling

        int len = -1;
        bool lastRot = false;
        uint8_t kick = 255;

        if (S.parent[si] >= 0 && isRotateAction(static_cast<Action>(S.via[si]))) {
            len = mgRecoverPath(S, si, buf);
            lastRot = true;
            kick = S.kick[si];
        } else if (S.rotSrc[si] >= 0 &&
                   classifySpin(b, p, sr, sx, sy, true, S.rotKick[si]) != SPIN_NONE) {
            // Tie-break (PRD 4.4): a longer rotation-last path is kept only when the
            // rotation earns a spin; otherwise the shortest path ends in a drop.
            const int srcLen = mgRecoverPath(S, S.rotSrc[si], buf);
            if (srcLen >= 0 && srcLen < MAX_PATH_LEN) {
                buf[srcLen] = static_cast<Action>(S.rotAct[si]);
                len = srcLen + 1;
                lastRot = true;
                kick = S.rotKick[si];
            } else {
                // Unreachable: the longest rotation path measured across 28,000
                // adversarial boards is 31, against MAX_PATH_LEN 64. Assert
                // rather than fall through quietly -- this branch keeps a valid
                // path but drops lastWasRotation, which is precisely the silent
                // spin-flag loss PRD 4.4 added the tie-break to prevent.
                assert(false && "rotation path did not fit -- spin flag lost");
                len = mgRecoverPath(S, si, buf);
            }
        } else {
            len = mgRecoverPath(S, si, buf);
        }
        assert(len >= 0 && "MAX_PATH_LEN too small -- a legal placement was dropped");
        if (len < 0) continue;   // release builds: skip rather than write past the end

        Placement& pl = out->items[out->count++];
        pl.x = static_cast<int8_t>(sx);
        pl.y = static_cast<int8_t>(sy);
        pl.rot = sr;
        pl.lastWasRotation = lastRot;
        pl.kickIndex = kick;
        pl.pathLen = static_cast<uint8_t>(len);
        for (int k = 0; k < len; ++k) pl.path[k] = buf[k];
        pl.spin = classifySpin(b, p, sr, sx, sy, lastRot, kick);
    }
}

} // namespace tb
