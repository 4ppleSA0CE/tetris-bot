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
export function computeStructSize(layout: SnapshotLayout, align: number): number {
  let max = 0;
  for (const [name, f] of Object.entries(layout)) {
    if (name.startsWith('event.')) continue;
    const end = f.offset + f.size * f.count;
    if (end > max) max = end;
  }
  return Math.ceil(max / align) * align;
}

// --- piece cell table ------------------------------------------------------
// [piece][rotation] -> flat [dx0,dy0,dx1,dy1,dx2,dy2,dx3,dy3], from C++
// pieceCells(). Held here so the renderer never duplicates the shape table.

const EMPTY_CELLS: readonly number[] = [];
let pieceCellTable: number[][][] | null = null;

export function setPieceCells(table: number[][][]): void {
  pieceCellTable = table;
}

export function getPieceCells(piece: number, rot: number): readonly number[] {
  if (pieceCellTable === null) {
    throw new Error('piece cell table not loaded — await createTetrisBot() first');
  }
  const p = pieceCellTable[piece];
  if (p === undefined) return EMPTY_CELLS;
  return p[rot & 3] ?? EMPTY_CELLS;
}

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
export class SnapshotView implements Snapshot {
  private buf: ArrayBufferLike;
  private dv!: DataView;
  private rowsView!: Uint16Array;
  private queueView!: Int8Array;
  private cellPieceView!: Uint8Array;
  private readonly eventBuf: SnapshotEvent[] = [];
  /** Diagnostics: how many times the views have been rebuilt. Starts at 1. */
  rebinds = 0;

  constructor(
    private readonly heap: HeapHost,
    readonly ptr: number,
    readonly L: SnapshotLayout,
    readonly structSize: number,
  ) {
    if (ptr === 0) throw new Error('SnapshotView: null snapshot pointer (destroyed handle?)');
    this.buf = heap.HEAPU8.buffer;
    this.rebind();
  }

  private rebind(): void {
    // ArrayBufferLike, not ArrayBuffer: TypedArray.buffer may be a SharedArrayBuffer
    // and TS 5.7+ enforces that. Only ever compared by identity below.
    const b: ArrayBufferLike = this.heap.HEAPU8.buffer;
    this.buf = b;
    this.dv = new DataView(b, this.ptr, this.structSize);
    this.rowsView = new Uint16Array(b, this.ptr + this.L.rows!.offset, this.L.rows!.count);
    this.queueView = new Int8Array(b, this.ptr + this.L.queue!.offset, this.L.queue!.count);
    this.cellPieceView = new Uint8Array(b, this.ptr + this.L.cellPiece!.offset, this.L.cellPiece!.count);
    this.rebinds++;
  }

  /** Revalidate against heap growth. Call before every read. */
  sync(): this {
    if (this.buf !== this.heap.HEAPU8.buffer) this.rebind();
    return this;
  }

  // `true` is littleEndian. wasm is always little-endian; being explicit means
  // the code stays correct under a big-endian JS test shim.
  private u8(f: string): number { return this.dv.getUint8(this.L[f]!.offset); }
  private i8(f: string): number { return this.dv.getInt8(this.L[f]!.offset); }
  private u16(f: string): number { return this.dv.getUint16(this.L[f]!.offset, true); }
  private u32(f: string): number { return this.dv.getUint32(this.L[f]!.offset, true); }
  private f32(f: string): number { return this.dv.getFloat32(this.L[f]!.offset, true); }

  get frame(): number { return this.u32('frame'); }
  get rows(): Uint16Array { return this.rowsView; }
  get activePiece(): number { return this.i8('activePiece'); }
  get activeRot(): number { return this.i8('activeRot'); }
  get activeX(): number { return this.i8('activeX'); }
  get activeY(): number { return this.i8('activeY'); }
  get ghostY(): number { return this.i8('ghostY'); }
  get pendingSpin(): number { return this.u8('pendingSpin'); }
  get pathProgress(): number { return this.u8('pathProgress'); }
  get holdPiece(): number { return this.i8('holdPiece'); }
  get queue(): Int8Array { return this.queueView; }
  get cellPiece(): Uint8Array { return this.cellPieceView; }
  get piecesPlaced(): number { return this.u32('piecesPlaced'); }
  get linesCleared(): number { return this.u32('linesCleared'); }
  get attackSent(): number { return this.u32('attackSent'); }
  get b2bCount(): number { return this.u16('b2bCount'); }
  get comboCount(): number { return this.u16('comboCount'); }
  get pps(): number { return this.f32('pps'); }
  get state(): number { return this.u8('state'); }

  /** Events written this tick. The array is reused; copy what you keep. */
  get events(): readonly SnapshotEvent[] {
    const n = this.u8('eventCount');
    const base = this.L.events!.offset;
    const stride = this.L.events!.size;
    const tOff = this.L['event.type']!.offset;
    const pOff = this.L['event.param']!.offset;
    const fOff = this.L['event.frame']!.offset;
    this.eventBuf.length = 0;
    for (let i = 0; i < n; i++) {
      const at = base + i * stride;
      this.eventBuf.push({
        type: this.dv.getUint8(at + tOff),
        param: this.dv.getUint8(at + pOff),
        frame: this.dv.getUint16(at + fOff, true),
      });
    }
    return this.eventBuf;
  }
}
