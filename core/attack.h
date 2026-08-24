#pragma once
#include <cstdint>

#include "core/types.h"

namespace tb {

struct ClearInfo {
    uint8_t  lines;         // 0..4
    SpinKind spin;          // spin kind of the placement that caused it
    bool     perfectClear;
};

// TETR.IO: a difficult clear is a quad, any spin (mini or full) that clears lines, or an
// All Clear. Returns false for a zero-line placement, which is neutral, not a break -
// the caller must leave the chain untouched in that case.
bool b2bMaintaining(const ClearInfo& c);

// Returns attack in garbage lines.
//   b2bActive  = the chain was live BEFORE this clear.
//   comboCount = number of consecutive prior clears (0 for the first).
//
// Base (TETR.IO multiplayer table): single 0, double 1, triple 2, quad 4; mini spin
// 0/1/2/4 for 1/2/3/4 lines; full T-spin 2/4/6 for 1/2/3 lines (10 for a spin quad).
// Plus 1 when the clear is b2bMaintaining AND b2bActive.
// Then the TETR.IO multiplier combo with c = comboCount: x (1 + 0.25c), and for c >= 2
// at least ln(1 + 1.25c); rounded down. Unbounded.
// Plus 5 for an All Clear, after rounding. Always 0 when no lines cleared.
int computeAttack(const ClearInfo& c, bool b2bActive, int comboCount);

} // namespace tb
