/** Mirrors tb::EventType in bindings/snapshot.h. */
export const EventType = {
    PIECE_LOCK: 0,
    LINE_CLEAR: 1,
    TETRIS: 2,
    TSPIN_MINI: 3,
    TSPIN_SINGLE: 4,
    TSPIN_DOUBLE: 5,
    TSPIN_TRIPLE: 6,
    B2B_EXTEND: 7,
    B2B_BREAK: 8,
    PERFECT_CLEAR: 9,
    TOPOUT: 10,
};
/** Spin kind of the placement currently being animated. */
export const SpinKind = { NONE: 0, MINI: 1, FULL: 2 };
/** Piece indices as the core numbers them. -1 means none. */
export const PieceLetter = ['I', 'J', 'L', 'O', 'S', 'T', 'Z'];
