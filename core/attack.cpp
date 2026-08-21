#include "core/attack.h"

namespace tb {
namespace {

constexpr int COMBO_BONUS[] = {0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5};
constexpr int COMBO_BONUS_LEN = static_cast<int>(sizeof(COMBO_BONUS) / sizeof(COMBO_BONUS[0]));

int baseAttack(const ClearInfo& c) {
    if (c.lines == 0) return 0;
    if (c.spin == SPIN_MINI) return 0;          // mini pays nothing, whatever it clears
    if (c.spin == SPIN_FULL) {
        if (c.lines == 1) return 2;
        if (c.lines == 2) return 4;
        return 6;                                // a full T-spin clears at most 3
    }
    if (c.lines == 1) return 0;
    if (c.lines == 2) return 1;
    if (c.lines == 3) return 2;
    return 4;                                    // tetris
}

} // namespace

bool b2bMaintaining(const ClearInfo& c) {
    if (c.lines == 0) return false;              // neutral, not breaking
    if (c.lines >= 4) return true;               // tetris
    return c.spin != SPIN_NONE;                  // mini OR full -- amended rule
}

int computeAttack(const ClearInfo& c, bool b2bActive, int comboCount) {
    if (c.lines == 0) return 0;

    int attack = baseAttack(c);
    if (b2bActive && b2bMaintaining(c)) attack += 1;

    int combo = comboCount;
    if (combo < 0) combo = 0;
    if (combo >= COMBO_BONUS_LEN) combo = COMBO_BONUS_LEN - 1;
    attack += COMBO_BONUS[combo];

    if (c.perfectClear) attack += 10;
    return attack;
}

} // namespace tb
