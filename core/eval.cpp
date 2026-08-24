#include "core/eval.h"
#include "core/attack.h"
#include "core/board.h"

namespace tb {

namespace {

// Out of bounds sideways and below the floor count as FILLED; above the board counts as EMPTY.
inline bool occ(const Board& b, int x, int y) {
    if (x < 0 || x >= BOARD_W) return true;
    if (y < 0) return true;
    if (y >= BOARD_H) return false;
    return ((b.rows[y] >> x) & 1u) != 0u;
}

} // namespace

// Counts TSD-shaped slots only: the ROT_2 T with its nub hanging down. T-spin-triple slots and
// side-entry (ROT_R / ROT_L) slots are NOT counted and are therefore unrewarded by the
// evaluator. See the preamble of this task before raising W_T_SLOT_COUNT to compensate.
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
            // S1: the four ROT_2 T cells must be empty
            if (occ(b, cx - 1, cy) || occ(b, cx, cy) || occ(b, cx + 1, cy)) continue;
            if (occ(b, cx, cy - 1)) continue;

            // S2: the nub sits in a notch
            bool dl = occ(b, cx - 1, cy - 1);
            bool dr = occ(b, cx + 1, cy - 1);
            if (!dl || !dr) continue;

            // S3: at least 3 of 4 diagonals filled, i.e. at least one overhang above
            bool ul = occ(b, cx - 1, cy + 1);
            bool ur = occ(b, cx + 1, cy + 1);
            int corners = (dl ? 1 : 0) + (dr ? 1 : 0) + (ul ? 1 : 0) + (ur ? 1 : 0);
            if (corners < 3) continue;

            // S4: not roofed over -- some column of the three is open down to cy
            if (h[cx - 1] > cy && h[cx] > cy && h[cx + 1] > cy) continue;

            ++count;   // S5: one count per distinct (cx, cy)
        }
    }
    return count;
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
    w.b2bActive         = W_B2B_ACTIVE;
    w.attackDealt       = W_ATTACK_DEALT;
    w.b2bCharge         = W_B2B_CHARGE;
    w.rowsWithHoles     = W_ROWS_WITH_HOLES;
    w.overhangs         = W_OVERHANGS;
    w.plainClear        = W_PLAIN_CLEAR;
    w.wastedT           = W_WASTED_T;
    return w;
}

float evaluate(const Board& b, const Weights& w, int b2bCount) {
    const Features f = extractFeatures(b);
    return w.holes             * static_cast<float>(f.holes)
         + w.coveredCells      * static_cast<float>(f.coveredCells)
         + w.bumpiness         * static_cast<float>(f.bumpiness)
         + w.maxHeight         * static_cast<float>(f.maxHeight)
         + w.heightPenalty     * static_cast<float>(f.heightPenalty)
         + w.rowTransitions    * static_cast<float>(f.rowTransitions)
         + w.columnTransitions * static_cast<float>(f.columnTransitions)
         + w.wellDepth         * static_cast<float>(f.wellDepth)
         + w.tSlotCount        * static_cast<float>(f.tSlotCount)
         + w.rowsWithHoles     * static_cast<float>(f.rowsWithHoles)
         + w.overhangs         * static_cast<float>(f.overhangs)
         + w.b2bActive         * (b2bCount > 0 ? 1.0f : 0.0f)
         + w.b2bCharge         * static_cast<float>(surgeCharge(b2bCount));
    // attackDealt, plainClear and wastedT are per-move rewards applied by the search with
    // the gamma discount (PRD 4.5), not properties of the terminal board.
}

Features extractFeatures(const Board& b) {
    Features f{};

    int h[BOARD_W];
    for (int c = 0; c < BOARD_W; ++c) h[c] = columnHeight(b, c);

    for (int c = 0; c < BOARD_W; ++c) if (h[c] > f.maxHeight) f.maxHeight = h[c];

    uint64_t holeRows = 0;   // BOARD_H (40) rows fit
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
        f.heightPenalty = e * e;   // squared: this is the cliff above row 12, not a slope
    }

    for (int y = 0; y < f.maxHeight; ++y) {
        bool prev = true;                       // left wall counts as filled
        for (int x = 0; x < BOARD_W; ++x) {
            bool cur = occ(b, x, y);
            if (cur != prev) ++f.rowTransitions;
            prev = cur;
        }
        if (!prev) ++f.rowTransitions;          // right wall counts as filled
    }

    for (int c = 0; c < BOARD_W; ++c) {
        bool prev = true;                       // the floor counts as filled
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
    { "b2bActive",         &Weights::b2bActive },
    { "attackDealt",       &Weights::attackDealt },
    { "b2bCharge",         &Weights::b2bCharge },
    { "rowsWithHoles",     &Weights::rowsWithHoles },
    { "overhangs",         &Weights::overhangs },
    { "plainClear",        &Weights::plainClear },
    { "wastedT",           &Weights::wastedT },
};
constexpr int kWeightCount = static_cast<int>(sizeof(kWeightTable) / sizeof(kWeightTable[0]));

bool sameName(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return false; ++a; ++b; }
    return *a == '\0' && *b == '\0';
}

} // namespace

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

} // namespace tb
