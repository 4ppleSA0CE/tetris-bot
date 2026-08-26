#include "core/eval.h"
#include "core/attack.h"
#include "core/board.h"

namespace tb {

namespace {

inline bool occ(const Board& b, int x, int y) {
    if (x < 0 || x >= BOARD_W) return true;
    if (y < 0) return true;
    if (y >= BOARD_H) return false;
    return ((b.rows[y] >> x) & 1u) != 0u;
}

}

int countTSlots(const Board& b) {
    int h[BOARD_W];
    int maxH = 0;
    for (int c = 0; c < BOARD_W; ++c) {
        h[c] = columnHeight(b, c);
        if (h[c] > maxH) maxH = h[c];
    }

    int count = 0;
    for (int cx = 1; cx <= BOARD_W - 2; ++cx) {
        for (int cy = 1; cy <= maxH; ++cy) {

            if (occ(b, cx - 1, cy) || occ(b, cx, cy) || occ(b, cx + 1, cy)) continue;
            if (occ(b, cx, cy - 1)) continue;

            bool dl = occ(b, cx - 1, cy - 1);
            bool dr = occ(b, cx + 1, cy - 1);
            if (!dl || !dr) continue;

            bool ul = occ(b, cx - 1, cy + 1);
            bool ur = occ(b, cx + 1, cy + 1);
            int corners = (dl ? 1 : 0) + (dr ? 1 : 0) + (ul ? 1 : 0) + (ur ? 1 : 0);
            if (corners < 3) continue;

            if (h[cx - 1] > cy && h[cx] > cy && h[cx + 1] > cy) continue;

            ++count;
        }
    }
    return count;
}

int cutoutTSlots(Board* b, int maxCuts, int* lines1, int* lines2) {
    int cuts = 0;
    if (maxCuts > 2) maxCuts = 2;
    while (cuts < maxCuts) {
        int h[BOARD_W];
        int maxH = 0;
        for (int c = 0; c < BOARD_W; ++c) {
            h[c] = columnHeight(*b, c);
            if (h[c] > maxH) maxH = h[c];
        }
        int bestLines = 0, bestX = -1, bestY = -1;
        for (int cy = 1; cy <= maxH && bestLines < 2; ++cy) {
            for (int cx = 1; cx <= BOARD_W - 2; ++cx) {

                if (occ(*b, cx - 1, cy) || occ(*b, cx, cy) || occ(*b, cx + 1, cy)) continue;
                if (occ(*b, cx, cy - 1)) continue;
                const bool dl = occ(*b, cx - 1, cy - 1);
                const bool dr = occ(*b, cx + 1, cy - 1);
                if (!dl || !dr) continue;
                const bool ul = occ(*b, cx - 1, cy + 1);
                const bool ur = occ(*b, cx + 1, cy + 1);
                if ((dl ? 1 : 0) + (dr ? 1 : 0) + (ul ? 1 : 0) + (ur ? 1 : 0) < 3) continue;
                if (h[cx - 1] > cy && h[cx] > cy && h[cx + 1] > cy) continue;

                const uint16_t topNeed = (uint16_t)(FULL_ROW & ~(7u << (cx - 1)));
                const uint16_t nubNeed = (uint16_t)(FULL_ROW & ~(1u << cx));
                const int lines = ((b->rows[cy] & topNeed) == topNeed ? 1 : 0)
                                + ((b->rows[cy - 1] & nubNeed) == nubNeed ? 1 : 0);
                if (lines > bestLines) { bestLines = lines; bestX = cx; bestY = cy; }
                if (bestLines == 2) break;
            }
        }
        if (bestLines < 1) break;
        b->rows[bestY]     |= (uint16_t)(7u << (bestX - 1));
        b->rows[bestY - 1] |= (uint16_t)(1u << bestX);
        clearLines(*b);
        if (bestLines == 1) ++*lines1; else ++*lines2;
        ++cuts;
    }
    return cuts;
}

Weights defaultWeights() {
    Weights w;
    w.holes             = W_HOLES;
    w.coveredCells      = W_COVERED_CELLS;
    w.bumpiness         = W_BUMPINESS;
    w.maxHeight         = W_MAX_HEIGHT;
    w.heightPenalty     = W_HEIGHT_PENALTY;
    w.rowTransitions    = W_ROW_TRANSITIONS;
    w.columnTransitions = W_COLUMN_TRANSITIONS;
    w.wellDepth         = W_WELL_DEPTH;
    w.tSlotCount        = W_T_SLOT_COUNT;
    w.tslot1            = W_TSLOT_1;
    w.tslot2            = W_TSLOT_2;
    w.b2bActive         = W_B2B_ACTIVE;
    w.b2bLevel          = W_B2B_LEVEL;
    w.attackDealt       = W_ATTACK_DEALT;
    w.b2bCharge         = W_B2B_CHARGE;
    w.rowsWithHoles     = W_ROWS_WITH_HOLES;
    w.overhangs         = W_OVERHANGS;
    w.plainClear        = W_PLAIN_CLEAR;
    w.b2bBreak          = W_B2B_BREAK;
    w.wastedT           = W_WASTED_T;
    w.incomingRisk      = W_INCOMING_RISK;
    w.pcNext            = W_PC_NEXT;
    return w;
}

float evaluate(const Board& b, const Weights& w, int b2bCount, int pendingRise,
               int tAvail) {
    Features f;

    if (tAvail > 0 && (w.tslot1 != 0.0f || w.tslot2 != 0.0f)) {
        Board cut = b;
        int l1 = 0, l2 = 0;
        cutoutTSlots(&cut, tAvail, &l1, &l2);
        f = extractFeatures(cut);
        f.tslotLines1 = l1;
        f.tslotLines2 = l2;
    } else {
        f = extractFeatures(b);
    }
    int er = f.maxHeight + pendingRise - HEIGHT_PENALTY_THRESHOLD;
    if (er < 0) er = 0;
    const int extra = er * er - f.heightPenalty;
    return w.holes             * static_cast<float>(f.holes)
         + w.coveredCells      * static_cast<float>(f.coveredCells)
         + w.bumpiness         * static_cast<float>(f.bumpiness)
         + w.maxHeight         * static_cast<float>(f.maxHeight)
         + w.heightPenalty     * static_cast<float>(f.heightPenalty)
         + w.incomingRisk      * static_cast<float>(extra)
         + w.rowTransitions    * static_cast<float>(f.rowTransitions)
         + w.columnTransitions * static_cast<float>(f.columnTransitions)
         + w.wellDepth         * static_cast<float>(f.wellDepth)
         + w.tSlotCount        * static_cast<float>(f.tSlotCount)
         + w.tslot1            * static_cast<float>(f.tslotLines1)
         + w.tslot2            * static_cast<float>(f.tslotLines2)
         + w.rowsWithHoles     * static_cast<float>(f.rowsWithHoles)
         + w.overhangs         * static_cast<float>(f.overhangs)
         + w.b2bActive         * (b2bCount > 0 ? 1.0f : 0.0f)
         + w.b2bLevel          * static_cast<float>(b2bCount < 8 ? b2bCount : 8)
         + w.b2bCharge         * static_cast<float>(surgeCharge(b2bCount));

}

Features extractFeatures(const Board& b) {
    Features f{};

    int h[BOARD_W];
    for (int c = 0; c < BOARD_W; ++c) h[c] = columnHeight(b, c);

    for (int c = 0; c < BOARD_W; ++c) if (h[c] > f.maxHeight) f.maxHeight = h[c];

    uint64_t holeRows = 0;
    for (int c = 0; c < BOARD_W; ++c) {
        for (int y = 0; y < h[c] - 1; ++y) {
            if (occ(b, c, y)) continue;
            ++f.holes;
            holeRows |= 1ull << y;
            if ((c > 0 && h[c - 1] <= y) || (c + 1 < BOARD_W && h[c + 1] <= y)) ++f.overhangs;
        }
    }
    for (int y = 0; y < BOARD_H; ++y) f.rowsWithHoles += static_cast<int>((holeRows >> y) & 1u);

    for (int c = 0; c < BOARD_W; ++c) {
        bool sawEmpty = false;
        for (int y = 0; y < h[c]; ++y) {
            if (!occ(b, c, y))      sawEmpty = true;
            else if (sawEmpty)      ++f.coveredCells;
        }
    }

    for (int c = 0; c + 1 < BOARD_W; ++c) {
        int d = h[c] - h[c + 1];
        f.bumpiness += (d < 0) ? -d : d;
    }

    {
        int e = f.maxHeight - HEIGHT_PENALTY_THRESHOLD;
        if (e < 0) e = 0;
        f.heightPenalty = e * e;
    }

    for (int y = 0; y < f.maxHeight; ++y) {
        bool prev = true;
        for (int x = 0; x < BOARD_W; ++x) {
            bool cur = occ(b, x, y);
            if (cur != prev) ++f.rowTransitions;
            prev = cur;
        }
        if (!prev) ++f.rowTransitions;
    }

    for (int c = 0; c < BOARD_W; ++c) {
        bool prev = true;
        for (int y = 0; y < f.maxHeight; ++y) {
            bool cur = occ(b, c, y);
            if (cur != prev) ++f.columnTransitions;
            prev = cur;
        }
    }

    for (int c = 0; c < BOARD_W; ++c) {
        int d = 0;
        for (int y = h[c]; y < BOARD_H; ++y) {
            if (!occ(b, c - 1, y) || !occ(b, c + 1, y)) break;
            ++d;
        }
        f.wellDepth += d * (d + 1) / 2;
    }

    f.tSlotCount = countTSlots(b);

    return f;
}

namespace {

struct WeightEntry { const char* name; float Weights::* field; };

const WeightEntry kWeightTable[] = {
    { "holes",             &Weights::holes },
    { "coveredCells",      &Weights::coveredCells },
    { "bumpiness",         &Weights::bumpiness },
    { "maxHeight",         &Weights::maxHeight },
    { "heightPenalty",     &Weights::heightPenalty },
    { "rowTransitions",    &Weights::rowTransitions },
    { "columnTransitions", &Weights::columnTransitions },
    { "wellDepth",         &Weights::wellDepth },
    { "tSlotCount",        &Weights::tSlotCount },
    { "tslot1",            &Weights::tslot1 },
    { "tslot2",            &Weights::tslot2 },
    { "b2bActive",         &Weights::b2bActive },
    { "b2bLevel",          &Weights::b2bLevel },
    { "attackDealt",       &Weights::attackDealt },
    { "b2bCharge",         &Weights::b2bCharge },
    { "rowsWithHoles",     &Weights::rowsWithHoles },
    { "overhangs",         &Weights::overhangs },
    { "plainClear",        &Weights::plainClear },
    { "b2bBreak",          &Weights::b2bBreak },
    { "wastedT",           &Weights::wastedT },
    { "incomingRisk",      &Weights::incomingRisk },
    { "pcNext",            &Weights::pcNext },
};
constexpr int kWeightCount = static_cast<int>(sizeof(kWeightTable) / sizeof(kWeightTable[0]));

bool sameName(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return false; ++a; ++b; }
    return *a == '\0' && *b == '\0';
}

}

bool setWeightByName(Weights& w, const char* name, float value) {
    if (name == nullptr || name[0] == '\0') return false;
    for (int i = 0; i < kWeightCount; ++i) {
        if (sameName(kWeightTable[i].name, name)) {
            w.*(kWeightTable[i].field) = value;
            return true;
        }
    }
    return false;
}

int weightNameCount() { return kWeightCount; }

const char* weightName(int i) {
    if (i < 0 || i >= kWeightCount) return "";
    return kWeightTable[i].name;
}

float weightValue(const Weights& w, int i) {
    if (i < 0 || i >= kWeightCount) return 0.0f;
    return w.*(kWeightTable[i].field);
}

}
