#pragma once
#include <cstdint>

#include "core/types.h"

namespace tb {

struct ClearInfo {
    uint8_t  lines;         // 0..4
    SpinKind spin;          // spin kind of the placement that caused it
    bool     perfectClear;
};

// AMENDMENT to PRD section 4.7: back-to-back is maintained by ANY T-spin that
// clears lines, mini OR full (guideline behavior), plus tetrises. It is broken
// by any other line clear.
//
// Returns false for a zero-line placement. That is NOT the same as "breaks the
// chain": a placement that clears nothing is neutral and the caller must leave
// the flag untouched. The caller's rule is:
//     if (lines == 0)              -> leave b2bActive alone
//     else if (b2bMaintaining(c))  -> b2bActive = true
//     else                         -> b2bActive = false
bool b2bMaintaining(const ClearInfo& c);

// Returns attack in garbage lines.
//   b2bActive  = the chain was live BEFORE this clear.
//   comboCount = number of consecutive prior clears (0 for the first).
//
// Base (TETR.IO multiplayer table): single 0, double 1, triple 2, quad 4; mini spin
// 0/1/2/4 for 1/2/3/4 lines; full T-spin 2/4/6 for 1/2/3 lines (10 for a spin quad).
// Plus 1 when the clear is b2bMaintaining AND b2bActive.
// Plus the combo bonus {0,0,1,1,1,2,2,3,3,4,4,4,5}, clamped at the last entry.
// Plus 10 for a perfect clear. Always 0 when no lines cleared.
int computeAttack(const ClearInfo& c, bool b2bActive, int comboCount);

} // namespace tb
