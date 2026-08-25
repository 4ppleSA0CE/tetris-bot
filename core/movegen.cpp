#include "core/movegen.h"

#include <cstring>

#include "core/board.h"
#include "core/piece.h"
#include "core/srs.h"

namespace tb {

constexpr int T_CENTER_DX = 1;
constexpr int T_CENTER_DY = 1;

bool mgCellOccupied(const Board& b, int x, int y) {
    if (x < 0 || x >= BOARD_W) return true;
    if (y < 0) return true;
    if (y >= BOARD_H) return false;
    return ((b.rows[y] >> x) & 1u) != 0u;
}

bool isImmobile(const Board& b, PieceType p, Rot r, int x, int y) {
    return collides(b, p, r, x - 1, y)
        && collides(b, p, r, x + 1, y)
        && collides(b, p, r, x, y + 1)
        && collides(b, p, r, x, y - 1);
}

SpinKind classifyTSpin(const Board& b, Rot r, int x, int y,
                       bool lastWasRotation, uint8_t kickIndex) {

    if (!lastWasRotation) return SPIN_NONE;

    const int cx = x + T_CENTER_DX;
    const int cy = y + T_CENTER_DY;

    static const int8_t CORNER_DX[4] = { -1, +1, -1, +1 };
    static const int8_t CORNER_DY[4] = { +1, +1, -1, -1 };

    static const uint8_t FRONT[4][2] = {
        { 0, 1 },
        { 1, 3 },
        { 2, 3 },
        { 0, 2 },
    };

    bool filled[4];
    int occupied = 0;
    for (int i = 0; i < 4; ++i) {
        filled[i] = mgCellOccupied(b, cx + CORNER_DX[i], cy + CORNER_DY[i]);
        if (filled[i]) ++occupied;
    }

    if (occupied < 3) return isImmobile(b, PIECE_T, r, x, y) ? SPIN_MINI : SPIN_NONE;

    const int ri = static_cast<int>(r) & 3;
    const int front = (filled[FRONT[ri][0]] ? 1 : 0) + (filled[FRONT[ri][1]] ? 1 : 0);
    if (front == 2) return SPIN_FULL;

    if (kickIndex == 4) return SPIN_FULL;

    return SPIN_MINI;
}

SpinKind classifySpin(const Board& b, PieceType p, Rot r, int x, int y,
                      bool lastWasRotation, uint8_t kickIndex) {
    if (p == PIECE_T) return classifyTSpin(b, r, x, y, lastWasRotation, kickIndex);
    if (!lastWasRotation) return SPIN_NONE;
    return isImmobile(b, p, r, x, y) ? SPIN_MINI : SPIN_NONE;
}

namespace {

struct BfsScratch {
    uint32_t stamp[MG_STATES];
    int32_t  parent[MG_STATES];
    uint8_t  via[MG_STATES];
    uint8_t  kick[MG_STATES];
    uint8_t  depth[MG_STATES];

    int32_t  rotSrc[MG_STATES];
    uint8_t  rotAct[MG_STATES];
    uint8_t  rotKick[MG_STATES];
    int32_t  queue[MG_STATES];
    uint32_t gen;
};

BfsScratch g_bfs{};

bool g_forceClassic = false;
int  g_seedY = -1;

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

            *nk = 255;
            return true;
        }
        default:
            return false;
    }
}

int mgRecoverPath(const BfsScratch& S, int32_t si, Action* out) {
    Action rev[MAX_PATH_LEN];
    int n = 0;
    int32_t cur = si;
    while (S.parent[cur] >= 0) {
        if (n >= MAX_PATH_LEN) return -1;
        rev[n++] = static_cast<Action>(S.via[cur]);
        cur = S.parent[cur];
    }
    int pn = 0;
    Action pre[32];
    if (g_seedY >= 0) {

        int rx = 0, ry = 0;
        Rot rr = ROT_0;
        mgDecodeState(cur, &rx, &ry, &rr);
        if (rr == ROT_R)      pre[pn++] = ACT_CW;
        else if (rr == ROT_2) pre[pn++] = ACT_180;
        else if (rr == ROT_L) pre[pn++] = ACT_CCW;
        for (int i = 0; i < rx - SPAWN_X; ++i) pre[pn++] = ACT_RIGHT;
        for (int i = 0; i < SPAWN_X - rx; ++i) pre[pn++] = ACT_LEFT;
        for (int i = 0; i < SPAWN_Y - ry; ++i) pre[pn++] = ACT_SOFT_DROP;
        if (n + pn > MAX_PATH_LEN) return -1;
    }
    for (int i = 0; i < pn; ++i) out[i] = pre[i];
    for (int i = 0; i < n; ++i) out[pn + i] = rev[n - 1 - i];
    return pn + n;
}

}

void mgForceClassicBfs(bool on) { g_forceClassic = on; }

bool mgApplyActionForTest(const Board& b, PieceType p, Action a, int x, int y, Rot r,
                          int* nx, int* ny, Rot* nr, uint8_t* nk) {
    return mgApplyAction(b, p, a, x, y, r, nx, ny, nr, nk);
}

void generateMoves(const Board& b, PieceType p, MoveList* out, bool withPaths) {
    out->count = 0;
    if (p == PIECE_NONE) return;

    BfsScratch& S = g_bfs;
    if (++S.gen == 0u) {
        std::memset(S.stamp, 0, sizeof(S.stamp));
        S.gen = 1u;
    }

    if (!mgInStateBounds(SPAWN_X, SPAWN_Y)) return;
    if (collides(b, p, ROT_0, SPAWN_X, SPAWN_Y)) return;

    int stackTop = 0;
    for (int y = BOARD_H - 1; y >= 0; --y) {
        if (b.rows[y] != 0u) { stackTop = y + 1; break; }
    }
    const bool seedIt = !g_forceClassic && stackTop < SPAWN_Y;
    g_seedY = seedIt ? stackTop : -1;

    int head = 0, tail = 0;
    if (seedIt) {
        for (int r = 0; r < 4; ++r) {
            for (int x = MG_X_MIN; x <= MG_X_MAX; ++x) {
                if (collides(b, p, static_cast<Rot>(r), x, stackTop)) continue;
                const int32_t si = mgStateIndex(x, stackTop, static_cast<Rot>(r));
                S.stamp[si]  = S.gen;
                S.parent[si] = -1;
                S.via[si]    = ACT_NONE;
                S.kick[si]   = 255;
                S.depth[si]  = 0;
                S.rotSrc[si] = -1;
                S.queue[tail++] = si;
            }
        }
    } else {
        const int32_t root = mgStateIndex(SPAWN_X, SPAWN_Y, ROT_0);
        S.stamp[root]  = S.gen;
        S.parent[root] = -1;
        S.via[root]    = ACT_NONE;
        S.kick[root]   = 255;
        S.depth[root]  = 0;
        S.rotSrc[root] = -1;
        S.queue[tail++] = root;
    }

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
            if (!mgInStateBounds(nx, ny)) continue;

            const int32_t ni = mgStateIndex(nx, ny, nr);
            if (S.stamp[ni] != S.gen) {
                S.stamp[ni]  = S.gen;
                S.parent[ni] = si;
                S.via[ni]    = static_cast<uint8_t>(a);
                S.kick[ni]   = nk;
                S.depth[ni]  = static_cast<uint8_t>(sd + 1);
                S.rotSrc[ni] = -1;
                if (tail < MG_STATES) S.queue[tail++] = ni;
            }

            if (isRotateAction(a) && S.rotSrc[ni] < 0) {
                S.rotSrc[ni]  = si;
                S.rotAct[ni]  = static_cast<uint8_t>(a);
                S.rotKick[ni] = nk;
            }
        }
    }

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
        if (!collides(b, p, sr, sx, sy - 1)) continue;

        int len = 0;
        bool lastRot = false;
        uint8_t kick = 255;

        if (S.parent[si] >= 0 && isRotateAction(static_cast<Action>(S.via[si]))) {
            lastRot = true;
            kick = S.kick[si];
            if (withPaths) len = mgRecoverPath(S, si, buf);
        } else if (S.rotSrc[si] >= 0 &&
                   classifySpin(b, p, sr, sx, sy, true, S.rotKick[si]) != SPIN_NONE) {

            lastRot = true;
            kick = S.rotKick[si];
            if (withPaths) {
                const int srcLen = mgRecoverPath(S, S.rotSrc[si], buf);
                if (srcLen >= 0 && srcLen < MAX_PATH_LEN) {
                    buf[srcLen] = static_cast<Action>(S.rotAct[si]);
                    len = srcLen + 1;
                } else {

                    assert(false && "rotation path did not fit -- spin flag lost");
                    len = mgRecoverPath(S, si, buf);
                    lastRot = S.parent[si] >= 0 &&
                              isRotateAction(static_cast<Action>(S.via[si]));
                    kick = lastRot ? S.kick[si] : 255;
                }
            }
        } else {
            if (withPaths) len = mgRecoverPath(S, si, buf);
        }
        assert(len >= 0 && "MAX_PATH_LEN too small -- a legal placement was dropped");
        if (len < 0) continue;

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

}
