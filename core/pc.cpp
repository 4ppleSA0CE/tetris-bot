#include "core/pc.h"
#include "core/board.h"
#include "core/piece.h"

namespace tb {

// necessary, not sufficient: full columns split the window into independently tileable segments
bool pcRegionsOk(const Board& b, int height) {
    int segEmpty = 0;
    for (int c = 0; c < BOARD_W; ++c) {
        int colEmpty = 0;
        for (int y = 0; y < height; ++y)
            if (!(b.rows[y] & static_cast<uint16_t>(1u << c))) ++colEmpty;
        if (colEmpty == 0) {
            if (segEmpty % 4 != 0) return false;
            segEmpty = 0;
        } else {
            segEmpty += colEmpty;
        }
    }
    return segEmpty % 4 == 0;
}

PcResult pcSolveHeight(const Board&, int, PieceType, PieceType,
                       const PieceType*, int, uint8_t, const PcConfig&) {
    return PcResult{};
}

PcResult pcSolve(const Board&, PieceType, PieceType,
                 const PieceType*, int, uint8_t, const PcConfig&) {
    return PcResult{};
}

bool pcPlan(const Board&, PieceType, PieceType, const PieceType*, int,
            uint8_t, int, const SearchConfig&, SearchResult*) {
    return false;
}

}
