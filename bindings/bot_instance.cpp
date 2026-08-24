#include "bindings/bot_instance.h"

#include <algorithm>
#include <cassert>
#include <cstring>

#include "core/board.h"
#include "core/piece.h"
#include "core/srs.h"
#include "core/attack.h"

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
        case 9:  return &w.tslot1;
        case 10: return &w.tslot2;
        case 11: return &w.b2bActive;
        case 12: return &w.attackDealt;
        case 13: return &w.b2bCharge;
        case 14: return &w.rowsWithHoles;
        case 15: return &w.overhangs;
        case 16: return &w.plainClear;
        case 17: return &w.wastedT;
        case 18: return &w.incomingRisk;
        default: return nullptr;
    }
}

namespace {
// SPAWN_X and SPAWN_Y are tb::SPAWN_X / tb::SPAWN_Y from core/movegen.h, which
// bot_instance.h already includes. DO NOT redeclare them here - an unnamed
// namespace nested in `tb` is `using`-ed into `tb`, so a local copy makes every
// unqualified use below ambiguous and the file stops compiling.
//
// A single tick can straddle several pieces (a long frame gap, or a high pps).
// Beyond this many pieces in one tick we resync the clock instead of catching
// up - the tab was almost certainly backgrounded.
constexpr int MAX_PIECES_PER_TICK = 64;
}  // namespace

BotInstance::BotInstance(uint32_t seed, float pps, int searchDepth, int beamWidth)
    : bag_(seed), seed_(seed), pps_(pps) {
    cfg_.depth     = searchDepth;
    cfg_.beamWidth = beamWidth;
    reset(seed);
}

void BotInstance::setPPS(float pps) {
    if (pps < 1.0f) pps = 1.0f;
    if (pps > 20.0f) pps = 20.0f;
    pps_ = pps;
    if (planValid_) recomputeTempo();
}

// weightName() returns "" for an out-of-range index and setWeightByName()
// rejects "", so a bad index from JS is a no-op rather than a stray write.
void BotInstance::setWeight(int index, float value) {
    setWeightByName(cfg_.weights, weightName(index), value);
}

void BotInstance::setTimeBudget(float ms) { cfg_.timeBudgetMs = ms; }

void BotInstance::reset(uint32_t seed) {
    seed_ = seed;
    bag_.reset(seed);
    std::memset(&board_, 0, sizeof(board_));
    std::memset(cellPiece_, CELL_EMPTY, sizeof(cellPiece_));
    std::memset(&snap_, 0, sizeof(snap_));
    hold_    = PIECE_NONE;
    garbageRng_ = (seed ^ 0x5DEECE66u) == 0u ? 0x9E3779B9u : (seed ^ 0x5DEECE66u);
    pendingGarbage_ = 0;
    current_ = bag_.next();
    for (int i = 0; i < PREVIEW_LEN; ++i) queue_[i] = bag_.next();
    comboCount_ = 0;
    b2bCount_ = 0;
    piecesPlaced_ = 0;
    linesCleared_ = 0;
    attackSent_ = 0;
    toppedOut_ = false;
    planValid_ = false;
    plannedLines_ = 0;
    pathSteps_ = 0;
    started_ = false;
    pieceStartMs_ = 0.0;
    lastNowMs_ = 0.0;
    ppsWindowStartMs_ = 0.0;
    ppsWindowPieces_ = 0;
    snap_.holdPiece = PIECE_NONE;
    snap_.activePiece = PIECE_NONE;
    snap_.state = 0;
    writeStats();
}

void BotInstance::queueGarbage(int lines) {
    if (lines < 1 || toppedOut_) return;
    if (lines > 20) lines = 20;
    pendingGarbage_ = pendingGarbage_ > 255 - lines ? 255 : pendingGarbage_ + lines;
    if (planValid_) {
        // Rewind plan()'s hold swap (and bag draw), then replan with the new pending
        // lines -- exactly the state core Game searches from, so parity holds.
        hold_    = prePlanHold_;
        current_ = prePlanCurrent_;
        for (int i = 0; i < PREVIEW_LEN; ++i) queue_[i] = prePlanQueue_[i];
        bag_ = prePlanBag_;
        planValid_ = false;
    }
    writeStats();
}

void BotInstance::topOut() {
    if (toppedOut_) return;
    toppedOut_ = true;
    planValid_ = false;
    snap_.state = 2;
    pushEvent(EV_TOPOUT, 0);
}

void BotInstance::pushEvent(uint8_t type, uint8_t param) {
    if (snap_.eventCount >= SNAPSHOT_MAX_EVENTS) return;   // drop, never overwrite
    Event& e = snap_.events[snap_.eventCount++];
    e.type  = type;
    e.param = param;
    e.frame = static_cast<uint16_t>(snap_.frame & 0xFFFFu);
}

void BotInstance::buildPathStates() {
    int x = SPAWN_X;
    int y = SPAWN_Y;
    Rot r = ROT_0;
    pathX_[0] = static_cast<int8_t>(x);
    pathY_[0] = static_cast<int8_t>(y);
    pathR_[0] = static_cast<int8_t>(r);
    // Trailing soft drops are one hard drop: the piece hovers through its shifts and
    // turns, then lands in a single step. Soft drops only survive when a tuck or a
    // spin follows them.
    int tail = plan_.pathLen;
    while (tail > 0 && plan_.path[tail - 1] == ACT_SOFT_DROP) --tail;
    pathSteps_ = tail < plan_.pathLen ? tail + 1 : tail;
    for (int i = 0; i < tail; ++i) {
        switch (plan_.path[i]) {
            case ACT_LEFT:
                if (!collides(board_, current_, r, x - 1, y)) x -= 1;
                break;
            case ACT_RIGHT:
                if (!collides(board_, current_, r, x + 1, y)) x += 1;
                break;
            case ACT_SOFT_DROP:
                if (!collides(board_, current_, r, x, y - 1)) y -= 1;
                break;
            case ACT_CW:
            case ACT_CCW:
            case ACT_180: {
                const Rot to = (plan_.path[i] == ACT_CW)  ? static_cast<Rot>((r + 1) & 3)
                             : (plan_.path[i] == ACT_CCW) ? static_cast<Rot>((r + 3) & 3)
                                                          : static_cast<Rot>((r + 2) & 3);
                int nx = 0, ny = 0; uint8_t ki = 0;
                if (tryRotate(board_, current_, r, to, x, y, &nx, &ny, &ki)) {
                    x = nx; y = ny; r = to;
                }
                break;
            }
            default:
                break;
        }
        pathX_[i + 1] = static_cast<int8_t>(x);
        pathY_[i + 1] = static_cast<int8_t>(y);
        pathR_[i + 1] = static_cast<int8_t>(r);
    }
    pathX_[pathSteps_] = plan_.x;
    pathY_[pathSteps_] = plan_.y;
    pathR_[pathSteps_] = static_cast<int8_t>(plan_.rot);
}

void BotInstance::recomputeTempo() {
    pieceMs_ = 1000.0 / static_cast<double>(pps_);
}

void BotInstance::plan() {
    if (toppedOut_) return;
    if (collides(board_, current_, ROT_0, SPAWN_X, SPAWN_Y)) { topOut(); return; }

    prePlanHold_    = hold_;
    prePlanCurrent_ = current_;
    for (int i = 0; i < PREVIEW_LEN; ++i) prePlanQueue_[i] = queue_[i];
    prePlanBag_ = bag_;

    const SearchResult r = search(board_, current_, hold_, queue_, PREVIEW_LEN,
                                  b2bCount_, comboCount_, cfg_, pendingGarbage_);
    if (!r.valid) { topOut(); return; }

    if (r.useHold) {
        const PieceType prevHold = hold_;
        hold_ = current_;
        if (prevHold == PIECE_NONE) {
            current_ = queue_[0];
            for (int i = 0; i < PREVIEW_LEN - 1; ++i) queue_[i] = queue_[i + 1];
            queue_[PREVIEW_LEN - 1] = bag_.next();
        } else {
            current_ = prevHold;
        }
    }

    plan_ = r.placement;
    planValid_ = true;
    buildPathStates();

    Board tmp = board_;
    lockPiece(tmp, current_, plan_.rot, plan_.x, plan_.y);
    plannedLines_ = clearLines(tmp);

    snap_.pendingSpin = static_cast<uint8_t>(plan_.spin);
    recomputeTempo();
    writeStats();
}

void BotInstance::lockCurrent() {
    // Paint the colour grid before the board changes under it.
    {
        const Cell* cs = pieceCells(current_, plan_.rot);
        for (int i = 0; i < 4; ++i) {
            const int cx = plan_.x + cs[i].dx;
            const int cy = plan_.y + cs[i].dy;
            if (cx >= 0 && cx < BOARD_W && cy >= 0 && cy < BOARD_H) {
                cellPiece_[cy * BOARD_W + cx] = static_cast<uint8_t>(current_);
            }
        }
    }

    lockPiece(board_, current_, plan_.rot, plan_.x, plan_.y);

    // Mirror clearLines' compaction onto the colour grid. This MUST run after
    // lockPiece and before clearLines: clearLines is what destroys the evidence of
    // which rows were full, and the two must agree cell for cell or the board is
    // painted in the wrong colours from here on.
    {
        int write = 0;
        for (int read = 0; read < BOARD_H; ++read) {
            if (board_.rows[read] == FULL_ROW) continue;
            if (write != read) {
                std::memcpy(&cellPiece_[write * BOARD_W], &cellPiece_[read * BOARD_W],
                            static_cast<size_t>(BOARD_W));
            }
            ++write;
        }
        for (int y = write; y < BOARD_H; ++y) {
            std::memset(&cellPiece_[y * BOARD_W], CELL_EMPTY, static_cast<size_t>(BOARD_W));
        }
    }

    const int lines = clearLines(board_);

    ClearInfo ci{};
    ci.lines        = static_cast<uint8_t>(lines);
    ci.spin         = plan_.spin;
    ci.perfectClear = (lines > 0) && isEmpty(board_);

    const int atk = computeAttack(ci, b2bCount_, comboCount_);
    attackSent_   += static_cast<uint32_t>(atk);
    linesCleared_ += static_cast<uint32_t>(lines);
    piecesPlaced_ += 1;

    // EVENT CONTRACT, and core/game.cpp's Game::stepPiece must match it:
    //   EV_PIECE_LOCK.param is the PieceType (0..6) that just locked. It is not
    //     recoverable any other way once current_ advances; the line count is
    //     already on the clear event.
    //   EXACTLY ONE of EV_LINE_CLEAR / EV_TETRIS / EV_TSPIN_* fires per
    //     line-clearing placement - they are mutually exclusive, never stacked.
    //     The renderer's calloutText() returns null for EV_LINE_CLEAR, so a
    //     tetris that also emitted EV_LINE_CLEAR would be a silently swallowed
    //     event, not a visible bug.
    //   EV_B2B_EXTEND fires from the SECOND difficult clear onward, with
    //     param == b2bCount. "BACK-TO-BACK x1" is not a back-to-back.
    pushEvent(EV_PIECE_LOCK, static_cast<uint8_t>(current_));
    if (lines > 0) {
        if (plan_.spin == SPIN_MINI) {
            pushEvent(EV_TSPIN_MINI, static_cast<uint8_t>(lines));
        } else if (plan_.spin == SPIN_FULL) {
            pushEvent(lines == 1 ? EV_TSPIN_SINGLE
                    : lines == 2 ? EV_TSPIN_DOUBLE
                                 : EV_TSPIN_TRIPLE, static_cast<uint8_t>(lines));
        } else if (lines == 4) {
            pushEvent(EV_TETRIS, 4);
        } else {
            pushEvent(EV_LINE_CLEAR, static_cast<uint8_t>(lines));
        }

        if (b2bMaintaining(ci)) {
            if (b2bCount_ < 0xFFFF) ++b2bCount_;
            if (b2bCount_ > 1) {
                pushEvent(EV_B2B_EXTEND,
                          static_cast<uint8_t>(b2bCount_ > 255 ? 255 : b2bCount_));
            }
        } else {
            const int surge = surgeCharge(b2bCount_);
            if (b2bCount_ > 0) {
                pushEvent(EV_B2B_BREAK, static_cast<uint8_t>(surge > 255 ? 255 : surge));
            }
            b2bCount_ = 0;
        }
        if (ci.perfectClear) pushEvent(EV_PERFECT_CLEAR, 0);
        comboCount_ += 1;
    } else {
        comboCount_ = 0;
    }

    current_ = queue_[0];
    for (int i = 0; i < PREVIEW_LEN - 1; ++i) queue_[i] = queue_[i + 1];
    queue_[PREVIEW_LEN - 1] = bag_.next();

    planValid_ = false;
    snap_.pendingSpin = 0;

    // Incoming garbage, core/game.cpp's exact semantics: this piece's attack cancels
    // pending lines first, the remainder rises now, one row at a time, hole redrawn per
    // row with probability 0.05 from the same seed-derived RNG. The colour grid shifts
    // with the board; garbage cells get id 7 (a grey the palette reserves for garbage).
    if (pendingGarbage_ > 0) {
        pendingGarbage_ -= atk;
        if (pendingGarbage_ < 0) pendingGarbage_ = 0;
    }
    if (pendingGarbage_ > 0) {
        const uint32_t redraw = (uint32_t)(0.05f * 10000.0f);
        int hole = (int)(xorshift32(garbageRng_) % (uint32_t)BOARD_W);
        for (int i = 0; i < pendingGarbage_; ++i) {
            if (i > 0 && xorshift32(garbageRng_) % 10000u < redraw)
                hole = (int)(xorshift32(garbageRng_) % (uint32_t)BOARD_W);
            addGarbage(board_, 1, hole);
            std::memmove(&cellPiece_[BOARD_W], &cellPiece_[0],
                         static_cast<size_t>((BOARD_H - 1) * BOARD_W));
            for (int x = 0; x < BOARD_W; ++x) {
                cellPiece_[x] = (x == hole) ? CELL_EMPTY : (uint8_t)7;
            }
        }
        pendingGarbage_ = 0;
    }

    ppsWindowPieces_ += 1;
    const double win = lastNowMs_ - ppsWindowStartMs_;
    if (win >= 1000.0) {
        snap_.pps = static_cast<float>(ppsWindowPieces_ * 1000.0 / win);
        ppsWindowStartMs_ = lastNowMs_;
        ppsWindowPieces_ = 0;
    }
    writeStats();

    // Lock-out, copied verbatim in intent from core/game.cpp's Game::stepPiece.
    // WITHOUT THIS the browser loop and the CLI loop disagree about when a run is
    // over: Game ends it the moment anything survives at or above the top of the
    // visible field, BotInstance would keep playing until the spawn cell itself is
    // blocked (row 21). PRD section 11 criterion 3 is measured on Game, so the
    // browser must not be the more permissive of the two.
    for (int y = VISIBLE_H; y < BOARD_H; ++y) {
        if (board_.rows[y] != 0) { topOut(); return; }
    }
}

void BotInstance::writeActive(double nowMs) {
    // Path states step at a constant rate over the first three quarters of the piece
    // interval; the placed piece rests on the stack for the last quarter.
    constexpr double SETTLE_AT = 0.75;
    double u = pieceMs_ > 0.0 ? (nowMs - pieceStartMs_) / pieceMs_ : 1.0;
    u = std::clamp(u, 0.0, 1.0);

    snap_.pathProgress = static_cast<uint8_t>(u * 255.0 + 0.5);
    int k = u >= SETTLE_AT ? pathSteps_
                           : static_cast<int>(u / SETTLE_AT * static_cast<double>(pathSteps_));
    if (k > pathSteps_) k = pathSteps_;
    if (k < 0) k = 0;

    snap_.activePiece = static_cast<int8_t>(current_);
    snap_.activeX     = pathX_[k];
    snap_.activeY     = pathY_[k];
    snap_.activeRot   = pathR_[k];
    snap_.ghostY      = static_cast<int8_t>(
        dropY(board_, current_, static_cast<Rot>(pathR_[k]), pathX_[k], pathY_[k]));
}

void BotInstance::writeStats() {
    std::memcpy(snap_.rows, board_.rows, sizeof(snap_.rows));
    std::memcpy(snap_.cellPiece, cellPiece_, sizeof(snap_.cellPiece));
    snap_.holdPiece    = static_cast<int8_t>(hold_);
    for (int i = 0; i < PREVIEW_LEN; ++i) snap_.queue[i] = static_cast<int8_t>(queue_[i]);
    snap_.piecesPlaced = piecesPlaced_;
    snap_.linesCleared = linesCleared_;
    snap_.attackSent   = attackSent_;
    snap_.b2bCount     = b2bCount_;
    snap_.comboCount   = static_cast<uint16_t>(comboCount_);
    snap_.pendingGarbage = static_cast<uint8_t>(pendingGarbage_ > 255 ? 255 : pendingGarbage_);
}

void BotInstance::tick(double nowMs) {
    snap_.eventCount = 0;          // events are per-tick; the renderer drains every frame
    snap_.frame += 1;
    lastNowMs_ = nowMs;

    if (!started_) {
        started_ = true;
        pieceStartMs_ = nowMs;
        ppsWindowStartMs_ = nowMs;
    }
    if (toppedOut_) { snap_.state = 2; return; }

    if (!planValid_) {
        plan();
        pieceStartMs_ = nowMs;
        if (toppedOut_) return;
    }

    int advanced = 0;
    while (planValid_) {
        if (nowMs - pieceStartMs_ < pieceMs_) break;
        if (++advanced > MAX_PIECES_PER_TICK) { pieceStartMs_ = nowMs; break; }
        pieceStartMs_ += pieceMs_;
        lockCurrent();
        if (toppedOut_) return;
        plan();
        if (toppedOut_) return;
    }

    writeActive(nowMs);
    snap_.state = 1;
}

} // namespace tb
