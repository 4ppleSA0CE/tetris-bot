/** Evaluator weights (core/eval.h). Every field is overridable via BotConfig. */
export interface Weights {
  holes: number;
  coveredCells: number;
  bumpiness: number;
  maxHeight: number;
  heightPenalty: number;
  rowTransitions: number;
  columnTransitions: number;
  wellDepth: number;
  tSlotCount: number;
  b2bActive: number;
  attackDealt: number;
  b2bCharge: number;
}

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
} as const;

export type EventTypeValue = (typeof EventType)[keyof typeof EventType];

export interface SnapshotEvent {
  /** One of EventType. */
  type: number;
  /**
   * PieceType (0-6) for PIECE_LOCK, lines cleared for a clear event, chain
   * length for B2B_EXTEND, Surge lines released for B2B_BREAK, 0 otherwise.
   * Exactly one of LINE_CLEAR / TETRIS / TSPIN_* fires per line-clearing
   * placement; TSPIN_* covers every piece under all-mini+.
   */
  param: number;
  /** The tick this event fired on, low 16 bits of Snapshot.frame. */
  frame: number;
}

/** Spin kind of the placement currently being animated. */
export const SpinKind = { NONE: 0, MINI: 1, FULL: 2 } as const;

/** Piece indices as the core numbers them. -1 means none. */
export const PieceLetter = ['I', 'J', 'L', 'O', 'S', 'T', 'Z'] as const;

/**
 * A live, zero-copy window onto the C++ tb::Snapshot in wasm linear memory.
 * Valid until the next tick(). `rows` and `queue` are views over the heap — do
 * not retain them across a tick, and never mutate them.
 */
export interface Snapshot {
  readonly frame: number;
  /** 40 rows of bitmask, row 0 = bottom, bit i = column i occupied. */
  readonly rows: Uint16Array;
  readonly activePiece: number;
  readonly activeRot: number;
  readonly activeX: number;
  readonly activeY: number;
  readonly ghostY: number;
  /** 0 none, 1 mini, 2 full — known before the piece locks. */
  readonly pendingSpin: number;
  /** 0-255 along the current movement path. */
  readonly pathProgress: number;
  /**
   * Which piece filled each cell, for per-piece colouring. 0-6, or 255 for empty.
   * Indexed [y * 10 + x]; y = 0 is the bottom row, same as `rows`.
   * A view over the heap - do not retain it across a tick.
   */
  readonly cellPiece: Uint8Array;
  readonly holdPiece: number;
  readonly queue: Int8Array;
  /** Events written this tick. Drained by the renderer every frame. */
  readonly events: readonly SnapshotEvent[];
  readonly piecesPlaced: number;
  readonly linesCleared: number;
  readonly attackSent: number;
  readonly b2bCount: number;
  readonly comboCount: number;
  /** Measured, not configured. */
  readonly pps: number;
  /** 0 idle, 1 playing, 2 topped out. */
  readonly state: number;
  /** Incoming garbage lines queued but not yet risen. */
  readonly pendingGarbage: number;
}

export interface BotConfig {
  seed?: number;
  /** 1-20, default 5. */
  pps?: number;
  /** default 5 */
  searchDepth?: number;
  /** default 100 */
  beamWidth?: number;
  weights?: Partial<Weights>;
  /**
   * Wall-clock planning budget per piece in ms, default 4.5. The default keeps a
   * 60 fps frame intact; raising it buys search depth at the cost of an occasional
   * long frame while a piece is being planned.
   */
  searchBudgetMs?: number;
  /**
   * Animate even when the OS asks for reduced motion. Default false, i.e. the OS
   * preference wins and the bot settles one static board and then holds still.
   *
   * ACCESSIBILITY: this is an opt-out for a page AUTHOR who wants to watch or
   * screen-record the bot on their own machine without changing a system-wide
   * accessibility setting - the demo page exposes it as `?motion=1`. Do not set it
   * on a page you ship to other people. `prefers-reduced-motion: reduce` is a
   * stated user need, and an endlessly looping animation is precisely what it asks
   * you to suppress; honouring it is why the default is what it is.
   */
  ignoreReducedMotion?: boolean;
}

export interface TetrisBot {
  /** Advance the simulation to this timestamp (host rAF time). */
  tick(nowMs: number): void;
  /** Live view; valid until the next tick(). */
  snapshot(): Snapshot;
  setPPS(pps: number): void;
  /** Queue incoming garbage lines (1-20); rises after the next lock, attack cancels first. */
  queueGarbage(lines: number): void;
  reset(seed?: number): void;
  destroy(): void;
}
