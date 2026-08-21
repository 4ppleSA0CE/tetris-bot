#pragma once
#include <cstdint>

namespace tb {

enum PieceType : int8_t {
    PIECE_I = 0, PIECE_J = 1, PIECE_L = 2, PIECE_O = 3,
    PIECE_S = 4, PIECE_T = 5, PIECE_Z = 6,
    PIECE_NONE = -1,
};
constexpr int NUM_PIECES = 7;

enum Rot : int8_t { ROT_0 = 0, ROT_R = 1, ROT_2 = 2, ROT_L = 3 };

enum Action : uint8_t {
    ACT_LEFT = 0, ACT_RIGHT = 1, ACT_CW = 2, ACT_CCW = 3,
    ACT_180 = 4, ACT_SOFT_DROP = 5, ACT_NONE = 255,
};
constexpr int NUM_ACTIONS = 6;

enum SpinKind : uint8_t { SPIN_NONE = 0, SPIN_MINI = 1, SPIN_FULL = 2 };

constexpr int BOARD_W = 10;
constexpr int BOARD_H = 40;
constexpr int VISIBLE_H = 20;
constexpr uint16_t FULL_ROW = 0x3FF;
constexpr int PREVIEW_LEN = 5;

// bit i of rows[y] = column i occupied. rows[0] = bottom row.
struct Board {
    uint16_t rows[BOARD_H];
};

} // namespace tb
