#pragma once
#include <cstdint>

#include "core/types.h"

namespace tb {

struct ClearInfo {
    uint8_t  lines;
    SpinKind spin;
    bool     perfectClear;
};

bool b2bMaintaining(const ClearInfo& c);

int b2bAfterClear(const ClearInfo& c, int b2bCount);

int surgeCharge(int b2bCount);

int computeAttack(const ClearInfo& c, int b2bCount, int comboCount);

}
