#include "bindings/bot_instance.h"

#include <algorithm>
#include <cassert>
#include <cstring>

#include "core/board.h"
#include "core/piece.h"
#include "core/srs.h"
#include "core/attack.h"
#include "core/pc.h"

namespace tb {

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
        case 12: return &w.b2bLevel;
        case 13: return &w.attackDealt;
        case 14: return &w.b2bCharge;
        case 15: return &w.rowsWithHoles;
        case 16: return &w.overhangs;
        case 17: return &w.plainClear;
        case 18: return &w.b2bBreak;
        case 19: return &w.wastedT;
        case 20: return &w.incomingRisk;
        case 21: return &w.pcNext;
        default: return nullptr;
    }
}

namespace {

constexpr int MAX_PIECES_PER_TICK = 64;
}

BotInstance::BotInstance(uint32_t seed, float pps, int searchDepth, int beamWidth)
    : bag_(seed), seed_(seed), pps_(pps) {
    cfg_.depth     = searchDepth;
    cfg_.beamWidth = beamWidth;

    cfg_.nodeBudget   = 4500;
    cfg_.timeBudgetMs = 1.0e9f;
    reset(seed);
}

void BotInstance::setPPS(float pps) {
    if (pps < 1.0f) pps = 1.0f;
    if (pps > 20.0f) pps = 20.0f;
    pps_ = pps;
    if (planValid_) recomputeTempo();
}

void BotInstance::setWeight(int index, float value) {
    setWeightByName(cfg_.weights, weightName(index), value);
}

void BotInstance::setTimeBudget(float ms) { cfg_.timeBudgetMs = ms; }
void BotInstance::setPcConfig(const PcConfig& pc) { cfg_.pc = pc; }
void BotInstance::setNodeBudget(long n) { cfg_.nodeBudget = n; }

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
    if (snap_.eventCount >= SNAPSHOT_MAX_EVENTS) return;
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

    pathFloor_[pathSteps_] = plan_.y;
    for (int i = pathSteps_ - 1; i >= 0; --i) {
        const int8_t own = static_cast<int8_t>(
            dropY(board_, current_, static_cast<Rot>(pathR_[i]), pathX_[i], pathY_[i]));
        pathFloor_[i] = own > pathFloor_[i + 1] ? own : pathFloor_[i + 1];
    }
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

    SearchResult r{};
    if (!pcPlan(board_, current_, hold_, queue_, PREVIEW_LEN, bag_.remainingMask(),
                pendingGarbage_, cfg_, &r)) {
        const long pcNodes = r.nodes;
        r = search(board_, current_, hold_, queue_, PREVIEW_LEN,
                   b2bCount_, comboCount_, cfg_, pendingGarbage_);
        r.nodes += pcNodes;
    }
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
            const int nb = b2bAfterClear(ci, (int)b2bCount_);
            b2bCount_ = (uint16_t)(nb > 0xFFFF ? 0xFFFF : nb);
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

    for (int y = VISIBLE_H; y < BOARD_H; ++y) {
        if (board_.rows[y] != 0) { topOut(); return; }
    }
}

void BotInstance::writeActive(double nowMs) {

    constexpr double SETTLE_AT = 0.90;
    constexpr double GRAV_MS = 50.0;
    constexpr double ROT_MS  = 30.0;
    constexpr double TAP_MS  = 40.0;
    constexpr double DAS_MS  = 90.0;
    constexpr double ARR_MS  = 15.0;
    constexpr double SOFT_MS = 25.0;
    constexpr double HARD_MS = 10.0;

    double u = pieceMs_ > 0.0 ? (nowMs - pieceStartMs_) / pieceMs_ : 1.0;
    u = std::clamp(u, 0.0, 1.0);
    snap_.pathProgress = static_cast<uint8_t>(u * 255.0 + 0.5);

    double dur[MAX_PATH_LEN + 1];
    double total = 0.0;
    int    horizRun = 0;
    for (int i = 0; i < pathSteps_; ++i) {
        const int dx = pathX_[i + 1] - pathX_[i];
        const int dy = pathY_[i] - pathY_[i + 1];
        const bool rotated = pathR_[i + 1] != pathR_[i];
        double d;
        if (dx != 0 && !rotated) {
            horizRun += 1;
            d = horizRun == 1 ? TAP_MS : horizRun == 2 ? DAS_MS : ARR_MS;
        } else {
            horizRun = 0;
            if (rotated)      d = ROT_MS;
            else if (dy == 1) d = SOFT_MS;
            else if (dy > 1)  d = dy * HARD_MS;
            else              d = 0.0;
        }
        dur[i] = d;
        total += d;
    }

    const double window = SETTLE_AT * pieceMs_;
    const double scale  = (total > window && total > 0.0) ? window / total : 1.0;

    const double elapsed = u * pieceMs_;
    double acc = 0.0;
    int k = 0;
    while (k < pathSteps_ && elapsed >= acc + dur[k] * scale) {
        acc += dur[k] * scale;
        k += 1;
    }

    snap_.activePiece = static_cast<int8_t>(current_);
    snap_.activeX     = pathX_[k];
    snap_.activeY     = pathY_[k];
    snap_.activeRot   = pathR_[k];
    if (k < pathSteps_) {

        const int fall = pathY_[k] - pathY_[k + 1];
        if (fall > 1 && dur[k] * scale > 0.0) {
            const double frac = (elapsed - acc) / (dur[k] * scale);
            const int cells = static_cast<int>(frac * static_cast<double>(fall));
            snap_.activeY = static_cast<int8_t>(pathY_[k] - std::clamp(cells, 0, fall));
        }
    }

    {
        const int gravCell = static_cast<int>(SPAWN_Y) -
                             static_cast<int>(elapsed / GRAV_MS);
        int driftY = gravCell > pathFloor_[k] ? gravCell : pathFloor_[k];
        if (driftY < snap_.activeY) snap_.activeY = static_cast<int8_t>(driftY);
    }
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
    snap_.eventCount = 0;
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

}
