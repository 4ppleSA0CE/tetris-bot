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
export declare const EventType: {
    readonly PIECE_LOCK: 0;
    readonly LINE_CLEAR: 1;
    readonly TETRIS: 2;
    readonly TSPIN_MINI: 3;
    readonly TSPIN_SINGLE: 4;
    readonly TSPIN_DOUBLE: 5;
    readonly TSPIN_TRIPLE: 6;
    readonly B2B_EXTEND: 7;
    readonly B2B_BREAK: 8;
    readonly PERFECT_CLEAR: 9;
    readonly TOPOUT: 10;
};
export type EventTypeValue = (typeof EventType)[keyof typeof EventType];
export interface SnapshotEvent {
    type: number;
    param: number;
    frame: number;
}
export declare const SpinKind: {
    readonly NONE: 0;
    readonly MINI: 1;
    readonly FULL: 2;
};
export declare const PieceLetter: readonly ["I", "J", "L", "O", "S", "T", "Z"];
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
