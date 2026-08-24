#include "core/attack.h"

namespace tb {
namespace {

constexpr int COMBO_BONUS[] = {0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5};
constexpr int COMBO_BONUS_LEN = static_cast<int>(sizeof(COMBO_BONUS) / sizeof(COMBO_BONUS[0]));

// TETR.IO multiplayer garbage table, indexed by lines cleared (1..4).
constexpr int BASE_PLAIN[5] = {0, 0, 1, 2, 4};    // single, double, triple, quad
constexpr int BASE_MINI[5]  = {0, 0, 1, 2, 4};    // mini spin: same as plain, but difficult
constexpr int BASE_FULL[5]  = {0, 2, 4, 6, 10};   // full T-spin single/double/triple, spin quad

int baseAttack(const ClearInfo& c) {
    const int n = c.lines > 4 ? 4 : c.lines;
    if (c.spin == SPIN_FULL) return BASE_FULL[n];
    if (c.spin == SPIN_MINI) return BASE_MINI[n];
    return BASE_PLAIN[n];
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
