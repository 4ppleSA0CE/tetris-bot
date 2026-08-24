#pragma once
#include "core/types.h"
#include "core/weights.h"

namespace tb {

// Raw integer board features (PRD 4.8). Every one of these is an exact integer on a given
// board -- that is what makes them testable against hand-built ASCII fixtures.
struct Features {
    int holes;              // empty cells with at least one filled cell above, same column
    int coveredCells;       // filled cells with at least one empty cell below, same column
    int bumpiness;          // sum over adjacent column pairs of |height difference|
    int maxHeight;          // tallest column height
    int heightPenalty;      // e*e where e = max(0, maxHeight - HEIGHT_PENALTY_THRESHOLD)
    int rowTransitions;     // filled/empty alternations along each row, walls count as filled
    int columnTransitions;  // filled/empty alternations up each column, floor counts as filled
    int wellDepth;          // sum over columns of d*(d+1)/2 for a well of depth d
    int tSlotCount;         // notches a T could rotate into (see countTSlots)
};

Features extractFeatures(const Board& b);

struct Weights {
    float holes;
    float coveredCells;
    float bumpiness;
    float maxHeight;
    float heightPenalty;      // applies to height above HEIGHT_PENALTY_THRESHOLD (12)
    float rowTransitions;
    float columnTransitions;
    float wellDepth;
    float tSlotCount;
    float b2bActive;
    float attackDealt;
    float b2bCharge;          // per line of Surge the live chain holds (surgeCharge)
};

Weights defaultWeights();

// Terminal board evaluation. Weights carry their own sign, so this is a plain dot product.
// attackDealt is NOT applied here: it is applied per node by the search, discounted by gamma
// to the depth of the placement that earned it (PRD 4.5).
float evaluate(const Board& b, const Weights& w, int b2bCount);

int countTSlots(const Board& b);

// Runtime weight override by field name. Names are exactly the Weights field names, e.g.
// "tSlotCount", "maxHeight", "attackDealt". Returns false for an unknown name so the CLI can
// report a typo instead of silently ignoring the flag.
//
// THIS IS THE ONLY WEIGHT-NAME TABLE IN THE PROJECT. The CLI's --weights flag and the WASM
// binding layer both read it from here. Do not declare a second one in bindings/ -- two
// definitions of tb::weightName is a link error in tb_tests and in dist/bot.js.
bool        setWeightByName(Weights& w, const char* name, float value);
int         weightNameCount();                 // 12, one per Weights field
const char* weightName(int i);                 // "" when i is out of range, never nullptr

} // namespace tb
