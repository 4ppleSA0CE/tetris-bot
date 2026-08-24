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
    int tslotLines1;        // virtual T-spin cutouts that cleared 1 line (see cutoutTSlots)
    int tslotLines2;        // virtual T-spin cutouts that cleared 2 lines
    int rowsWithHoles;      // rows containing at least one hole
    int overhangs;          // holes with a neighbouring column open down to that row
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
    float tslot1;             // per virtual cutout clearing 1 line
    float tslot2;             // per virtual cutout clearing 2 lines (a ready TSD)
    float b2bActive;
    float attackDealt;
    float b2bCharge;          // per line of Surge the live chain holds (surgeCharge)
    float rowsWithHoles;
    float overhangs;          // positive: a side-reachable hole is cheaper than an enclosed one
    // Move terms, applied per placement by the search like attackDealt, never by evaluate().
    float plainClear;         // per clear that is not b2bMaintaining (breaks or wastes the chain)
    float wastedT;            // per T placed without a spin
    float incomingRisk;       // times the EXTRA cliff area pending garbage adds (see evaluate)
};

Weights defaultWeights();

// Terminal board evaluation. Weights carry their own sign, so this is a plain dot product.
// attackDealt, plainClear and wastedT are NOT applied here: they are applied per node by the
// search, discounted by gamma to the depth of the placement that earned them (PRD 4.5).
// pendingRise = incoming garbage lines not yet cancelled on this path. It is charged through
// its OWN weight, incomingRisk, times the extra cliff area: (e_with_rise^2 - e^2) where
// e = max(0, maxHeight - threshold). heightPenalty keeps its solo meaning untouched, and a
// rise of 0 contributes exactly nothing; never the positive maxHeight build reward.
// tAvail = T pieces actually reachable (visible queue + hold), capped at 2 inside. When
// > 0, ready TSD slots are VIRTUALLY EXECUTED before feature extraction (Cold Clear's
// cutout): the T is locked, its lines cleared, tslot1/tslot2 rewarded, and every other
// feature measured on the resulting board -- so a covered slot stops reading as holes and
// the eval can see the chain it is holding. Default 0 keeps old call sites byte-identical.
float evaluate(const Board& b, const Weights& w, int b2bCount, int pendingRise = 0,
               int tAvail = 0);

int countTSlots(const Board& b);

// Virtually execute up to maxCuts ready TSD slots on *b (best slot first: most lines,
// then lowest scan order). Only slots clearing >= 1 line cut out. Returns cuts made and
// tallies them into *lines1 / *lines2.
int cutoutTSlots(Board* b, int maxCuts, int* lines1, int* lines2);

// Runtime weight override by field name. Names are exactly the Weights field names, e.g.
// "tSlotCount", "maxHeight", "attackDealt". Returns false for an unknown name so the CLI can
// report a typo instead of silently ignoring the flag.
//
// THIS IS THE ONLY WEIGHT-NAME TABLE IN THE PROJECT. The CLI's --weights flag and the WASM
// binding layer both read it from here. Do not declare a second one in bindings/ -- two
// definitions of tb::weightName is a link error in tb_tests and in dist/bot.js.
bool        setWeightByName(Weights& w, const char* name, float value);
int         weightNameCount();                 // one per Weights field
const char* weightName(int i);                 // "" when i is out of range, never nullptr
float       weightValue(const Weights& w, int i);  // 0 when i is out of range

} // namespace tb
