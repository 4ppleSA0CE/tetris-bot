#include "core/game.h"
#include "core/board.h"
#include "core/attack.h"
#include "core/pc.h"
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
    toppedOut_     = false;
    piecesPlaced_  = 0;
    linesCleared_  = 0;
    attackSent_    = 0;
    surgeSent_     = 0;
    tSpinCount_    = 0;
    pcCount_       = 0;
    b2bCount_      = 0;
    comboCount_    = 0;
    maxB2b_        = 0;
    lastSearchMs_  = 0.0f;
    eventCount_    = 0;
    pendingGarbage_   = 0;
    garbageReceived_  = 0;
    garbageRng_       = (seed ^ 0x5DEECE66u) == 0u ? 0x9E3779B9u : (seed ^ 0x5DEECE66u);
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
    SearchResult r{};
    if (!pcPlan(board_, current_, hold_, queue_, PREVIEW_LEN, bag_.remainingMask(),
                pendingGarbage_, cfg_, &r))
        r = search(board_, current_, hold_, queue_, PREVIEW_LEN,
                   (int)b2bCount_, (int)comboCount_, cfg_, pendingGarbage_);
    lastSearchMs_ = (float)std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - t0).count();
    lastSearchNodes_ = r.nodes;
    lastSearchDupes_ = r.dupes;
    lastSearchBeamSlots_ = r.beamSlots;

    if (!r.valid) {
        toppedOut_ = true;
        pushEvent(GEV_TOPOUT, 0);
        return;
    }

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
    const int atk = computeAttack(ci, (int)b2bCount_, (int)comboCount_);

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

        if (r.placement.spin != SPIN_NONE) ++tSpinCount_;

        comboCount_ = (uint16_t)(comboCount_ + 1);

        if (b2bMaintaining(ci)) {
            if (b2bCount_ < 0xFFFF) ++b2bCount_;
            if (b2bCount_ > maxB2b_) maxB2b_ = b2bCount_;
            pushEvent(GEV_B2B_EXTEND, (uint8_t)(b2bCount_ > 255 ? 255 : b2bCount_));
        } else {
            const int surge = surgeCharge((int)b2bCount_);
            surgeSent_ += (uint32_t)surge;
            if (b2bCount_ > 0) pushEvent(GEV_B2B_BREAK, (uint8_t)(surge > 255 ? 255 : surge));
            b2bCount_ = 0;
        }

        if (ci.perfectClear) { ++pcCount_; pushEvent(GEV_PERFECT_CLEAR, 0); }
    } else {
        comboCount_ = 0;
    }

    if (pendingGarbage_ > 0) {
        pendingGarbage_ -= atk;
        if (pendingGarbage_ < 0) pendingGarbage_ = 0;
    }
    if (pendingGarbage_ > 0) {
        const uint32_t redraw = (uint32_t)(messiness_ * 10000.0f);
        int hole = (int)(xorshift32(garbageRng_) % (uint32_t)BOARD_W);
        for (int i = 0; i < pendingGarbage_; ++i) {
            if (i > 0 && xorshift32(garbageRng_) % 10000u < redraw)
                hole = (int)(xorshift32(garbageRng_) % (uint32_t)BOARD_W);
            addGarbage(board_, 1, hole);
        }
        garbageReceived_ += (uint32_t)pendingGarbage_;
        pendingGarbage_ = 0;
    }

    for (int y = VISIBLE_H; y < BOARD_H; ++y) {
        if (board_.rows[y] != 0) {
            toppedOut_ = true;
            pushEvent(GEV_TOPOUT, 0);
            break;
        }
    }
}

}
