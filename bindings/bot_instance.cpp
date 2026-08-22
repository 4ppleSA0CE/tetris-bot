#include "bindings/bot_instance.h"

namespace tb {

// Read a weight by core/eval.h's index. The NAMES and the ORDER are owned by
// core/eval.cpp's kWeightTable - this switch only has to agree with it, and
// test_weight_table() in tests/tests.cpp asserts that it does by writing through
// setWeightByName(w, weightName(i), ...) and reading back through this.
float* bindingsWeightSlot(Weights& w, int index) {
    if (index < 0 || index >= weightNameCount()) return nullptr;
    switch (index) {
        case 0:  return &w.holes;
        case 1:  return &w.coveredCells;
        case 2:  return &w.bumpiness;
        case 3:  return &w.maxHeight;
        case 4:  return &w.heightPenalty;
        case 5:  return &w.rowTransitions;
        case 6:  return &w.columnTransitions;
        case 7:  return &w.wellDepth;
        case 8:  return &w.tSlotCount;
        case 9:  return &w.b2bActive;
        case 10: return &w.attackDealt;
        default: return nullptr;
    }
}

} // namespace tb
