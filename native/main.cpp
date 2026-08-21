// native/main.cpp -- development CLI harness. Runs the core natively, no browser.
#include <cstdio>

#include "core/types.h"

int main() {
    std::printf("tetris_bot: board %dx%d, %d rows allocated\n",
                tb::BOARD_W, tb::VISIBLE_H, tb::BOARD_H);
    return 0;
}
