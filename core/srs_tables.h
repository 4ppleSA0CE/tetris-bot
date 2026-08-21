#pragma once
#include <cstdint>

namespace tb {

// ---------------------------------------------------------------------------
// SRS wall kick data. Transcribed from https://harddrop.com/wiki/SRS and
// https://tetris.wiki/Super_Rotation_System (cross-checked: byte-identical).
//
// COORDINATE CONVENTION FOR THIS PROJECT: x increases RIGHT, y increases UP.
// Board row 0 = bottom.
//
// The Tetris Wiki tables use "a convention of positive x rightwards, positive
// y upwards" (quoted verbatim from both pages). That matches this project
// exactly, so NO SIGN FLIP WAS APPLIED -- these are the wiki values as printed.
//
// Offsets are added to the piece's position: a successful test at index k
// moves the piece to (x + KICKS[t][k].dx, y + KICKS[t][k].dy).
// The 5 offsets are tried IN ORDER; the first that does not collide wins.
// If all 5 collide, the rotation is REJECTED (piece does not move or turn).
// Index 0 is always {0,0} -- i.e. plain rotation with no kick.
//
// Rotation states: 0 = spawn, R = one CW from spawn, 2 = 180 from spawn,
//                  L = one CCW from spawn.
//
// TRANSITION INDEX MAPPING (index into the first dimension):
//   [0] = 0->R    [1] = R->0    [2] = R->2    [3] = 2->R
//   [4] = 2->L    [5] = L->2    [6] = L->0    [7] = 0->L
//
// Each table has only 4 DISTINCT rows, each appearing twice -- but JLSTZ and I
// pair different transitions. JLSTZ collapses because its state 0 and state 2
// offsets are both all-zero; I collapses via a cross-sum symmetry instead,
// because the I piece's pivot is not at its geometric center. Do not "simplify"
// either table by assuming they share a pairing -- tests assert both structures.
//
// The O tetromino NEVER KICKS. It has no table: its four rotation states are
// the same four cells, so rotation is a no-op that always succeeds. Do not
// route O through either table.
// ---------------------------------------------------------------------------

struct Kick { int8_t dx, dy; };

// J, L, S, T, Z -- all five share this table.
constexpr Kick KICKS_JLSTZ[8][5] = {
    {{ 0, 0}, {-1, 0}, {-1, 1}, { 0,-2}, {-1,-2}}, // 0->R
    {{ 0, 0}, { 1, 0}, { 1,-1}, { 0, 2}, { 1, 2}}, // R->0
    {{ 0, 0}, { 1, 0}, { 1,-1}, { 0, 2}, { 1, 2}}, // R->2
    {{ 0, 0}, {-1, 0}, {-1, 1}, { 0,-2}, {-1,-2}}, // 2->R
    {{ 0, 0}, { 1, 0}, { 1, 1}, { 0,-2}, { 1,-2}}, // 2->L
    {{ 0, 0}, {-1, 0}, {-1,-1}, { 0, 2}, {-1, 2}}, // L->2
    {{ 0, 0}, {-1, 0}, {-1,-1}, { 0, 2}, {-1, 2}}, // L->0
    {{ 0, 0}, { 1, 0}, { 1, 1}, { 0,-2}, { 1,-2}}, // 0->L
};

// I tetromino -- its own table, with its own duplicate-row pairing.
constexpr Kick KICKS_I[8][5] = {
    {{ 0, 0}, {-2, 0}, { 1, 0}, {-2,-1}, { 1, 2}}, // 0->R
    {{ 0, 0}, { 2, 0}, {-1, 0}, { 2, 1}, {-1,-2}}, // R->0
    {{ 0, 0}, {-1, 0}, { 2, 0}, {-1, 2}, { 2,-1}}, // R->2
    {{ 0, 0}, { 1, 0}, {-2, 0}, { 1,-2}, {-2, 1}}, // 2->R
    {{ 0, 0}, { 2, 0}, {-1, 0}, { 2, 1}, {-1,-2}}, // 2->L
    {{ 0, 0}, {-2, 0}, { 1, 0}, {-2,-1}, { 1, 2}}, // L->2
    {{ 0, 0}, { 1, 0}, {-2, 0}, { 1,-2}, {-2, 1}}, // L->0
    {{ 0, 0}, {-1, 0}, { 2, 0}, {-1, 2}, { 2,-1}}, // 0->L
};

// ---------------------------------------------------------------------------
// 180-degree kick table. NOT part of Guideline SRS -- no authoritative spec
// exists. This is TETR.IO's table, the de-facto standard, taken from
// Budget-Tetris-Engine (src/kicks/srs_tetrio.zig), whose header states it was
// "taken directly from Tetr.io's source code, https://tetr.io/js/tetrio.js"
// and which already uses this project's x-right/y-up convention, so zero
// conversion was applied. Cross-checked against tetrox (kicks.rs) and
// python-tetris (impl/rotation.py) after converting their row-down conventions.
//
// Same coordinate convention as above: x right, y UP. K = 6 tests.
// Tried in order, first non-colliding wins, all-fail => rotation rejected.
//
// Transition index order: 0->2, R->L, 2->0, L->R -- which is exactly the
// `from` rotation's numeric value.
// ---------------------------------------------------------------------------
constexpr Kick KICKS_180[4][6] = {
    {{ 0, 0}, { 0, 1}, { 1, 1}, {-1, 1}, { 1, 0}, {-1, 0}}, // 0->2
    {{ 0, 0}, { 1, 0}, { 1, 2}, { 1, 1}, { 0, 2}, { 0, 1}}, // R->L
    {{ 0, 0}, { 0,-1}, {-1,-1}, { 1,-1}, {-1, 0}, { 1, 0}}, // 2->0
    {{ 0, 0}, {-1, 0}, {-1, 2}, {-1, 1}, { 0, 2}, { 0, 1}}, // L->R
};

// TETR.IO applies KICKS_180 to J, L, S, T, Z only.
// I gets a single test -- {0,0} -- i.e. the I piece does NOT kick on a 180.
// O does not kick at all.
constexpr Kick KICKS_180_I[4][1] = { {{0, 0}}, {{0, 0}}, {{0, 0}}, {{0, 0}} };

} // namespace tb
