export function computeStructSize(layout, align) {
    let max = 0;
    for (const [name, f] of Object.entries(layout)) {
        if (name.startsWith('event.'))
            continue;
        const end = f.offset + f.size * f.count;
        if (end > max)
            max = end;
    }
    return Math.ceil(max / align) * align;
}
const EMPTY_CELLS = [];
let pieceCellTable = null;
export function setPieceCells(table) {
    pieceCellTable = table;
}
export function getPieceCells(piece, rot) {
    if (pieceCellTable === null) {
        throw new Error('piece cell table not loaded — await createTetrisBot() first');
    }
    const p = pieceCellTable[piece];
    if (p === undefined)
        return EMPTY_CELLS;
    return p[rot & 3] ?? EMPTY_CELLS;
}
export class SnapshotView {
    heap;
    ptr;
    L;
    structSize;
    buf;
    dv;
    rowsView;
    queueView;
    cellPieceView;
    eventBuf = [];
    rebinds = 0;
    constructor(heap, ptr, L, structSize) {
        this.heap = heap;
        this.ptr = ptr;
        this.L = L;
        this.structSize = structSize;
        if (ptr === 0)
            throw new Error('SnapshotView: null snapshot pointer (destroyed handle?)');
        this.buf = heap.HEAPU8.buffer;
        this.rebind();
    }
    rebind() {
        const b = this.heap.HEAPU8.buffer;
        this.buf = b;
        this.dv = new DataView(b, this.ptr, this.structSize);
        this.rowsView = new Uint16Array(b, this.ptr + this.L.rows.offset, this.L.rows.count);
        this.queueView = new Int8Array(b, this.ptr + this.L.queue.offset, this.L.queue.count);
        this.cellPieceView = new Uint8Array(b, this.ptr + this.L.cellPiece.offset, this.L.cellPiece.count);
        this.rebinds++;
    }
    sync() {
        if (this.buf !== this.heap.HEAPU8.buffer)
            this.rebind();
        return this;
    }
    u8(f) { return this.dv.getUint8(this.L[f].offset); }
    i8(f) { return this.dv.getInt8(this.L[f].offset); }
    u16(f) { return this.dv.getUint16(this.L[f].offset, true); }
    u32(f) { return this.dv.getUint32(this.L[f].offset, true); }
    f32(f) { return this.dv.getFloat32(this.L[f].offset, true); }
    get frame() { return this.u32('frame'); }
    get rows() { return this.rowsView; }
    get activePiece() { return this.i8('activePiece'); }
    get activeRot() { return this.i8('activeRot'); }
    get activeX() { return this.i8('activeX'); }
    get activeY() { return this.i8('activeY'); }
    get ghostY() { return this.i8('ghostY'); }
    get pendingSpin() { return this.u8('pendingSpin'); }
    get pathProgress() { return this.u8('pathProgress'); }
    get holdPiece() { return this.i8('holdPiece'); }
    get queue() { return this.queueView; }
    get cellPiece() { return this.cellPieceView; }
    get piecesPlaced() { return this.u32('piecesPlaced'); }
    get linesCleared() { return this.u32('linesCleared'); }
    get attackSent() { return this.u32('attackSent'); }
    get b2bCount() { return this.u16('b2bCount'); }
    get comboCount() { return this.u16('comboCount'); }
    get pps() { return this.f32('pps'); }
    get state() { return this.u8('state'); }
    get pendingGarbage() { return this.u8('pendingGarbage'); }
    get events() {
        const n = this.u8('eventCount');
        const base = this.L.events.offset;
        const stride = this.L.events.size;
        const tOff = this.L['event.type'].offset;
        const pOff = this.L['event.param'].offset;
        const fOff = this.L['event.frame'].offset;
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
