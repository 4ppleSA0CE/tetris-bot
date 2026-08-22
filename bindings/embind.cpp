#include <emscripten/bind.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "bindings/snapshot.h"
#include "bindings/bot_instance.h"
#include "core/piece.h"
#include "core/eval.h"

using namespace emscripten;

namespace {

// --- layout export ---------------------------------------------------------
// C++ is the single source of truth for layout. TypeScript learns it at runtime.
// Change a field in snapshot.h, rebuild, and TS follows with no TS edit.
#define LAY(name, ty, cnt)                                             \
    "\"" #name "\":{\"offset\":" + std::to_string(offsetof(tb::Snapshot, name)) + \
    ",\"size\":" + std::to_string(sizeof(tb::Snapshot::name) / (cnt)) +           \
    ",\"count\":" + std::to_string(cnt) + ",\"type\":\"" ty "\"}"

#define ELAY(name, ty)                                                 \
    "\"event." #name "\":{\"offset\":" + std::to_string(offsetof(tb::Event, name)) + \
    ",\"size\":" + std::to_string(sizeof(tb::Event::name)) +                          \
    ",\"count\":1,\"type\":\"" ty "\"}"

std::string getSnapshotLayout() {
    std::string s = "{";
    s += LAY(frame,        "u32", 1);   s += ",";
    s += LAY(rows,         "u16", 40);  s += ",";
    s += LAY(activePiece,  "i8",  1);   s += ",";
    s += LAY(activeRot,    "i8",  1);   s += ",";
    s += LAY(activeX,      "i8",  1);   s += ",";
    s += LAY(activeY,      "i8",  1);   s += ",";
    s += LAY(ghostY,       "i8",  1);   s += ",";
    s += LAY(pendingSpin,  "u8",  1);   s += ",";
    s += LAY(pathProgress, "u8",  1);   s += ",";
    s += LAY(holdPiece,    "i8",  1);   s += ",";
    s += LAY(queue,        "i8",  5);   s += ",";
    s += LAY(eventCount,   "u8",  1);   s += ",";
    s += LAY(events,       "struct", 8); s += ",";
    s += LAY(piecesPlaced, "u32", 1);   s += ",";
    s += LAY(linesCleared, "u32", 1);   s += ",";
    s += LAY(attackSent,   "u32", 1);   s += ",";
    s += LAY(b2bCount,     "u16", 1);   s += ",";
    s += LAY(comboCount,   "u16", 1);   s += ",";
    s += LAY(pps,          "f32", 1);   s += ",";
    s += LAY(state,        "u8",  1);   s += ",";
    s += ELAY(type,  "u8");  s += ",";
    s += ELAY(param, "u8");  s += ",";
    s += ELAY(frame, "u16");
    return s + "}";
}
#undef LAY
#undef ELAY

int32_t getSnapshotSize()  { return static_cast<int32_t>(sizeof(tb::Snapshot)); }
int32_t getSnapshotAlign() { return static_cast<int32_t>(alignof(tb::Snapshot)); }

// Names, count, and order come from core/eval.h. bindings only supplies the
// read-by-index (bindingsWeightSlot) that core/eval.h does not declare.
std::string getWeightsInfo() {
    tb::Weights w = tb::defaultWeights();
    std::string s = "[";
    for (int i = 0; i < tb::weightNameCount(); ++i) {
        if (i) s += ",";
        s += "{\"name\":\"";
        s += tb::weightName(i);
        s += "\",\"index\":" + std::to_string(i);
        s += ",\"default\":" + std::to_string(*tb::bindingsWeightSlot(w, i)) + "}";
    }
    return s + "]";
}

// [piece][rotation] -> flat [dx0,dy0,dx1,dy1,dx2,dy2,dx3,dy3].
// The renderer draws hold/queue previews and the active piece from this, so the
// shape table is never duplicated in TypeScript.
std::string getPieceCells() {
    std::string s = "[";
    for (int p = 0; p < tb::NUM_PIECES; ++p) {
        if (p) s += ",";
        s += "[";
        for (int r = 0; r < 4; ++r) {
            if (r) s += ",";
            const tb::Cell* c = tb::pieceCells(static_cast<tb::PieceType>(p),
                                               static_cast<tb::Rot>(r));
            s += "[";
            for (int i = 0; i < 4; ++i) {
                if (i) s += ",";
                s += std::to_string(static_cast<int>(c[i].dx)) + "," +
                     std::to_string(static_cast<int>(c[i].dy));
            }
            s += "]";
        }
        s += "]";
    }
    return s + "]";
}


// --- handle table ----------------------------------------------------------
// Monotonic handles: a stale handle is always a clean miss, never a different
// live bot. unique_ptr: the BotInstance never moves, so botSnapshotPtr() stays
// valid for its whole life.
std::unordered_map<int32_t, std::unique_ptr<tb::BotInstance>> g_bots;
int32_t g_nextHandle = 1;   // 0 is reserved as "invalid"

tb::BotInstance* look(int32_t h) {
    auto it = g_bots.find(h);
    return it == g_bots.end() ? nullptr : it->second.get();
}

int32_t botCreate(int32_t seed, float pps, int32_t searchDepth, int32_t beamWidth) {
    const int32_t h = g_nextHandle++;
    g_bots.emplace(h, std::make_unique<tb::BotInstance>(
        static_cast<uint32_t>(seed), pps, searchDepth, beamWidth));
    return h;
}

bool botTick(int32_t h, double nowMs) {
    if (auto* b = look(h)) { b->tick(nowMs); return true; }
    return false;
}

// A wasm linear-memory byte offset. The ADDRESS survives memory growth; the JS
// VIEW built over it does not. See SnapshotView.sync() in js/layout.ts.
uintptr_t botSnapshotPtr(int32_t h) {
    auto* b = look(h);
    return b ? reinterpret_cast<uintptr_t>(b->snapshotPtr()) : 0;
}

bool botSetPPS(int32_t h, float pps) {
    if (auto* b = look(h)) { b->setPPS(pps); return true; }
    return false;
}

bool botSetWeight(int32_t h, int32_t index, float value) {
    if (auto* b = look(h)) { b->setWeight(index, value); return true; }
    return false;
}

// seed < 0 means "reuse the seed this instance already has".
bool botReset(int32_t h, int32_t seed) {
    if (auto* b = look(h)) {
        b->reset(seed < 0 ? b->seed() : static_cast<uint32_t>(seed));
        return true;
    }
    return false;
}

bool    botDestroy(int32_t h) { return g_bots.erase(h) > 0; }
int32_t botLiveCount()        { return static_cast<int32_t>(g_bots.size()); }

}  // namespace

EMSCRIPTEN_BINDINGS(tetris_bot_layout) {
    function("getSnapshotLayout", &getSnapshotLayout);
    function("getSnapshotSize",   &getSnapshotSize);
    function("getSnapshotAlign",  &getSnapshotAlign);
    function("getWeightsInfo",    &getWeightsInfo);
    function("getPieceCells",     &getPieceCells);

    function("botCreate",      &botCreate);
    function("botTick",        &botTick);
    function("botSnapshotPtr", &botSnapshotPtr);
    function("botSetPPS",      &botSetPPS);
    function("botSetWeight",   &botSetWeight);
    function("botReset",       &botReset);
    function("botDestroy",     &botDestroy);
    function("botLiveCount",   &botLiveCount);
}
