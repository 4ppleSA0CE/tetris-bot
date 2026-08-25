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

  type: number;

  param: number;

  frame: number;
}

export const SpinKind = { NONE: 0, MINI: 1, FULL: 2 } as const;

export const PieceLetter = ['I', 'J', 'L', 'O', 'S', 'T', 'Z'] as const;

export interface Snapshot {
  readonly frame: number;

  readonly rows: Uint16Array;
  readonly activePiece: number;
  readonly activeRot: number;
  readonly activeX: number;
  readonly activeY: number;
  readonly ghostY: number;

  readonly pendingSpin: number;

  readonly pathProgress: number;

  readonly cellPiece: Uint8Array;
  readonly holdPiece: number;
  readonly queue: Int8Array;

  readonly events: readonly SnapshotEvent[];
  readonly piecesPlaced: number;
  readonly linesCleared: number;
  readonly attackSent: number;
  readonly b2bCount: number;
  readonly comboCount: number;

  readonly pps: number;

  readonly state: number;

  readonly pendingGarbage: number;
}

export interface BotConfig {
  seed?: number;

  pps?: number;

  searchDepth?: number;

  beamWidth?: number;
  weights?: Partial<Weights>;

  searchBudgetMs?: number;

  ignoreReducedMotion?: boolean;
}

export interface TetrisBot {

  tick(nowMs: number): void;

  snapshot(): Snapshot;
  setPPS(pps: number): void;

  queueGarbage(lines: number): void;
  reset(seed?: number): void;
  destroy(): void;
}
