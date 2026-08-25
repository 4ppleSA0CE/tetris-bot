#pragma once
#include <cassert>
#include <cstdint>

#include "core/types.h"
#include "core/board.h"

namespace tb {

constexpr int MAX_PLACEMENTS = 256;
constexpr int MAX_PATH_LEN   = 64;

constexpr int SPAWN_X = 4;
constexpr int SPAWN_Y = 21;

constexpr int MG_X_MIN = -3;
constexpr int MG_X_MAX = 10;
constexpr int MG_Y_MIN = -3;
constexpr int MG_Y_MAX = 40;
constexpr int MG_XS    = MG_X_MAX - MG_X_MIN + 1;
constexpr int MG_YS    = MG_Y_MAX - MG_Y_MIN + 1;
constexpr int MG_STATES = MG_XS * MG_YS * 4;

inline bool mgInStateBounds(int x, int y) {
    return x >= MG_X_MIN && x <= MG_X_MAX && y >= MG_Y_MIN && y <= MG_Y_MAX;
}

inline int mgStateIndex(int x, int y, Rot r) {
    assert(mgInStateBounds(x, y));
    return (((y - MG_Y_MIN) * MG_XS) + (x - MG_X_MIN)) * 4 + static_cast<int>(r);
}

inline void mgDecodeState(int idx, int* x, int* y, Rot* r) {
    *r = static_cast<Rot>(idx & 3);
    const int cell = idx >> 2;
    *x = (cell % MG_XS) + MG_X_MIN;
    *y = (cell / MG_XS) + MG_Y_MIN;
}

inline Rot rotCW (Rot r) { return static_cast<Rot>((static_cast<int>(r) + 1) & 3); }
inline Rot rotCCW(Rot r) { return static_cast<Rot>((static_cast<int>(r) + 3) & 3); }
inline Rot rot180(Rot r) { return static_cast<Rot>((static_cast<int>(r) + 2) & 3); }

inline bool isRotateAction(Action a) {
    return a == ACT_CW || a == ACT_CCW || a == ACT_180;
}

struct Placement {
    int8_t   x, y;
    Rot      rot;
    SpinKind spin;
    bool     lastWasRotation;

    uint8_t  kickIndex;
    uint8_t  pathLen;
    Action   path[MAX_PATH_LEN];
};

static_assert(sizeof(Placement) == 71, "Placement must stay packed at 71 bytes");
static_assert(alignof(Placement) == 1, "Placement must stay alignment-1");

struct MoveList {
    int       count;
    Placement items[MAX_PLACEMENTS];
};

void generateMoves(const Board& b, PieceType p, MoveList* out, bool withPaths = true);

void mgForceClassicBfs(bool on);
bool mgApplyActionForTest(const Board& b, PieceType p, Action a, int x, int y, Rot r,
                          int* nx, int* ny, Rot* nr, uint8_t* nk);

SpinKind classifyTSpin(const Board& b, Rot r, int x, int y,
                       bool lastWasRotation, uint8_t kickIndex);

SpinKind classifySpin(const Board& b, PieceType p, Rot r, int x, int y,
                      bool lastWasRotation, uint8_t kickIndex);

bool isImmobile(const Board& b, PieceType p, Rot r, int x, int y);

bool mgCellOccupied(const Board& b, int x, int y);

}
