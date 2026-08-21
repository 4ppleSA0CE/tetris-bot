#pragma once
#include <cstdint>

#include "core/types.h"

namespace tb {

// xorshift32. Advances `state` in place and returns the new value.
// `state` must never be 0 -- a zero state is a fixed point that returns 0
// forever. Bag::reset() remaps seed 0 to 0x9E3779B9 for exactly this reason.
uint32_t xorshift32(uint32_t& state);

// 7-bag randomizer: emits every piece once per 7 draws, order shuffled with
// Fisher-Yates. Seeded and fully deterministic.
class Bag {
public:
    explicit Bag(uint32_t seed);
    PieceType next();
    void reset(uint32_t seed);

private:
    void refill();

    uint32_t  state_;
    int       index_;
    PieceType pieces_[NUM_PIECES];
};

} // namespace tb
