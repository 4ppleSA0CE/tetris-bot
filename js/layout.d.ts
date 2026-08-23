import type { Snapshot, SnapshotEvent } from './types.js';
/** One entry of getSnapshotLayout(). Offsets and sizes come from C++ offsetof. */
export interface FieldLayout {
    offset: number;
    size: number;
    count: number;
    type: string;
}
/**
 * The parsed getSnapshotLayout() JSON. Keys are Snapshot field names, plus three
 * "event.*" keys whose offsets are relative to one Event, not to Snapshot.
 */
export type SnapshotLayout = Record<string, FieldLayout>;
/**
 * Reproduce sizeof(Snapshot) from the field records: the highest byte any field
 * touches, rounded up to the struct's alignment. Compared against the C++
 * sizeof at init — if they disagree, something moved and JS is about to read
 * garbage (PRD section 12).
 */
export declare function computeStructSize(layout: SnapshotLayout, align: number): number;
export declare function setPieceCells(table: number[][][]): void;
export declare function getPieceCells(piece: number, rot: number): readonly number[];
/** The only thing SnapshotView needs from the Emscripten module. */
export interface HeapHost {
    HEAPU8: Uint8Array;
}
/**
 * A live window onto a C++ tb::Snapshot in wasm linear memory. Zero copy.
 *
 * ALLOW_MEMORY_GROWTH detaches the backing ArrayBuffer when the heap grows: a
 * captured DataView then throws, and a captured typed array silently reads
 * `undefined`. The pointer itself stays valid. sync() compares buffer identity
 * and rebuilds the views when they differ — one reference compare per frame.
 */
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
    /** Diagnostics: how many times the views have been rebuilt. Starts at 1. */
    rebinds: number;
    constructor(heap: HeapHost, ptr: number, L: SnapshotLayout, structSize: number);
    private rebind;
    /** Revalidate against heap growth. Call before every read. */
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
    /** Events written this tick. The array is reused; copy what you keep. */
    get events(): readonly SnapshotEvent[];
}
