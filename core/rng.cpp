#include "core/rng.h"

namespace tb {

uint32_t xorshift32(uint32_t& state) {
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

Bag::Bag(uint32_t seed) : state_(0), index_(0), pieces_{} {
    reset(seed);
}

void Bag::reset(uint32_t seed) {
    state_ = (seed == 0u) ? 0x9E3779B9u : seed;
    index_ = NUM_PIECES;
    for (int i = 0; i < NUM_PIECES; ++i)
        pieces_[i] = static_cast<PieceType>(i);
}

void Bag::refill() {
    for (int i = 0; i < NUM_PIECES; ++i)
        pieces_[i] = static_cast<PieceType>(i);

    for (int i = NUM_PIECES - 1; i > 0; --i) {
        const uint32_t j = xorshift32(state_) % static_cast<uint32_t>(i + 1);
        const PieceType tmp = pieces_[i];
        pieces_[i] = pieces_[j];
        pieces_[j] = tmp;
    }
}

PieceType Bag::next() {
    if (index_ >= NUM_PIECES) {
        refill();
        index_ = 0;
    }
    return pieces_[index_++];
}

}
