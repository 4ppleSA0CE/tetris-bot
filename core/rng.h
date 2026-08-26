#pragma once
#include <cstdint>

#include "core/types.h"

namespace tb {

uint32_t xorshift32(uint32_t& state);

class Bag {
public:
    explicit Bag(uint32_t seed);
    PieceType next();
    void reset(uint32_t seed);
    uint8_t remainingMask() const;

private:
    void refill();

    uint32_t  state_;
    int       index_;
    PieceType pieces_[NUM_PIECES];
};

}
