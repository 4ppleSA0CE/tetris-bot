#pragma once
#include "core/types.h"
#include "core/weights.h"

namespace tb {

struct Features {
    int holes;
    int coveredCells;
    int bumpiness;
    int maxHeight;
    int heightPenalty;
    int rowTransitions;
    int columnTransitions;
    int wellDepth;
    int tSlotCount;
    int tslotLines1;
    int tslotLines2;
    int rowsWithHoles;
    int overhangs;
};

Features extractFeatures(const Board& b);

struct Weights {
    float holes;
    float coveredCells;
    float bumpiness;
    float maxHeight;
    float heightPenalty;
    float rowTransitions;
    float columnTransitions;
    float wellDepth;
    float tSlotCount;
    float tslot1;
    float tslot2;
    float b2bActive;
    float b2bLevel;
    float attackDealt;
    float b2bCharge;
    float rowsWithHoles;
    float overhangs;

    float plainClear;
    float b2bBreak;
    float wastedT;
    float incomingRisk;

    float pcNext;
};

Weights defaultWeights();

float evaluate(const Board& b, const Weights& w, int b2bCount, int pendingRise = 0,
               int tAvail = 0);

int countTSlots(const Board& b);

int cutoutTSlots(Board* b, int maxCuts, int* lines1, int* lines2);

bool        setWeightByName(Weights& w, const char* name, float value);
int         weightNameCount();
const char* weightName(int i);
float       weightValue(const Weights& w, int i);

}
