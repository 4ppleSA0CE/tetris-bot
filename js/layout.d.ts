import type { Snapshot, SnapshotEvent } from './types.js';
export interface FieldLayout {
    offset: number;
    size: number;
    count: number;
    type: string;
}
export type SnapshotLayout = Record<string, FieldLayout>;
export declare function computeStructSize(layout: SnapshotLayout, align: number): number;
export declare function setPieceCells(table: number[][][]): void;
export declare function getPieceCells(piece: number, rot: number): readonly number[];
export interface HeapHost {
    HEAPU8: Uint8Array;
}
export declare class SnapshotView implements Snapshot {
    private readonly heap;
    readonly ptr: number;
    readonly L: SnapshotLayout;
    readonly structSize: number;
    private buf;
    private dv;
    private rowsView;
    private queueView;
    private cellPieceView;
    private readonly eventBuf;
    rebinds: number;
    constructor(heap: HeapHost, ptr: number, L: SnapshotLayout, structSize: number);
    private rebind;
    sync(): this;
    private u8;
    private i8;
    private u16;
    private u32;
    private f32;
    get frame(): number;
    get rows(): Uint16Array;
    get activePiece(): number;
    get activeRot(): number;
    get activeX(): number;
    get activeY(): number;
    get ghostY(): number;
    get pendingSpin(): number;
    get pathProgress(): number;
    get holdPiece(): number;
    get queue(): Int8Array;
    get cellPiece(): Uint8Array;
    get piecesPlaced(): number;
    get linesCleared(): number;
    get attackSent(): number;
    get b2bCount(): number;
    get comboCount(): number;
    get pps(): number;
    get state(): number;
    get pendingGarbage(): number;
    get events(): readonly SnapshotEvent[];
}
