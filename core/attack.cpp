#include "core/attack.h"

#include <algorithm>
#include <cmath>

namespace tb {
namespace {

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

    double g = baseAttack(c);
    if (b2bActive && b2bMaintaining(c)) g += 1.0;

    // TETR.IO "Multiplier" combo: the whole (base + b2b) scales with the combo, and a
    // base-0 clear is lifted by a log floor from the third consecutive clear on.
    const int combo = comboCount < 0 ? 0 : comboCount;
    g *= 1.0 + 0.25 * combo;
    if (combo >= 2) g = std::max(g, std::log1p(1.25 * combo));

    int attack = static_cast<int>(std::floor(g));   // TETR.IO "DOWN" rounding
    if (c.perfectClear) attack += 10;
    return attack;
}

} // namespace tb
