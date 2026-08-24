#include "core/game.h"
#include "core/board.h"
#include "core/attack.h"
#include <cassert>
#include <chrono>

namespace tb {

Game::Game(uint32_t seed, const SearchConfig& cfg) : bag_(seed), cfg_(cfg) {
    reset(seed);
}

void Game::reset(uint32_t seed) {
    for (int y = 0; y < BOARD_H; ++y) board_.rows[y] = 0;
    bag_.reset(seed);
    hold_    = PIECE_NONE;
    current_ = bag_.next();
    for (int i = 0; i < PREVIEW_LEN; ++i) queue_[i] = bag_.next();
    lastPlacement_ = Placement{};
    lastUsedHold_  = false;
    b2bActive_     = false;
    toppedOut_     = false;
    piecesPlaced_  = 0;
    linesCleared_  = 0;
    attackSent_    = 0;
    tSpinCount_    = 0;
    b2bCount_      = 0;
    comboCount_    = 0;
    maxB2b_        = 0;
    lastSearchMs_  = 0.0f;
    eventCount_    = 0;
}

void Game::pushEvent(uint8_t type, uint8_t param) {
    if (eventCount_ >= MAX_GAME_EVENTS) return;
    events_[eventCount_].type  = type;
    events_[eventCount_].param = param;
    ++eventCount_;
}

void Game::shiftQueue() {
    for (int i = 0; i + 1 < PREVIEW_LEN; ++i) queue_[i] = queue_[i + 1];
    queue_[PREVIEW_LEN - 1] = bag_.next();
}

void Game::stepPiece() {
    if (toppedOut_) return;
    eventCount_ = 0;

    const auto t0 = std::chrono::steady_clock::now();
    const SearchResult r = search(board_, current_, hold_, queue_, PREVIEW_LEN,
                                  b2bActive_, (int)comboCount_, cfg_);
    lastSearchMs_ = (float)std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - t0).count();

    if (!r.valid) {
        toppedOut_ = true;
        pushEvent(GEV_TOPOUT, 0);
        return;
    }

    // Consume pieces exactly the way search's root branches assumed (PRD 4.5).
    PieceType placed;
    if (!r.useHold) {
        placed   = current_;
        current_ = queue_[0];
        shiftQueue();
    } else if (hold_ == PIECE_NONE) {
        placed   = queue_[0];
        hold_    = current_;
        shiftQueue();
        current_ = queue_[0];
        shiftQueue();
    } else {
        placed   = hold_;
        hold_    = current_;
        current_ = queue_[0];
        shiftQueue();
    }

    lastPlacement_ = r.placement;
    lastUsedHold_  = r.useHold;

    lockPiece(board_, placed, r.placement.rot, r.placement.x, r.placement.y);
    const int lines = clearLines(board_);

    ClearInfo ci;
    ci.lines        = (uint8_t)lines;
    ci.spin         = r.placement.spin;
    ci.perfectClear = (lines > 0) && isEmpty(board_);
    const int atk = computeAttack(ci, /*b2bActive=*/b2bActive_, /*comboCount=*/(int)comboCount_);

    ++piecesPlaced_;
    linesCleared_ += (uint32_t)lines;
    attackSent_   += (uint32_t)atk;
    pushEvent(GEV_PIECE_LOCK, (uint8_t)placed);

    if (lines > 0) {
        pushEvent(GEV_LINE_CLEAR, (uint8_t)lines);
        if (r.placement.spin == SPIN_MINI) {
            pushEvent(GEV_TSPIN_MINI, (uint8_t)lines);
        } else if (r.placement.spin == SPIN_FULL) {
            if (lines == 1)      pushEvent(GEV_TSPIN_SINGLE, 1);
            else if (lines == 2) pushEvent(GEV_TSPIN_DOUBLE, 2);
            else                 pushEvent(GEV_TSPIN_TRIPLE, 3);
        } else if (lines == 4) {
            pushEvent(GEV_TETRIS, 4);
        }

        // A spin that clears nothing is worth nothing, so only clearing spins are counted.
        if (r.placement.spin != SPIN_NONE) ++tSpinCount_;

        comboCount_ = (uint16_t)(comboCount_ + 1);

        if (b2bMaintaining(ci)) {
            b2bCount_  = b2bActive_ ? (uint16_t)(b2bCount_ + 1) : (uint16_t)1;
            b2bActive_ = true;
            if (b2bCount_ > maxB2b_) maxB2b_ = b2bCount_;
            pushEvent(GEV_B2B_EXTEND, (uint8_t)(b2bCount_ > 255 ? 255 : b2bCount_));
        } else {
            if (b2bActive_) pushEvent(GEV_B2B_BREAK, 0);
            b2bActive_ = false;
            b2bCount_  = 0;
        }

        if (ci.perfectClear) pushEvent(GEV_PERFECT_CLEAR, 0);
    } else {
        comboCount_ = 0;
    }

    // Lock-out: anything left at or above the top of the visible field ends the run.
    for (int y = VISIBLE_H; y < BOARD_H; ++y) {
        if (board_.rows[y] != 0) {
            toppedOut_ = true;
            pushEvent(GEV_TOPOUT, 0);
            break;
        }
    }
}

} // namespace tb
