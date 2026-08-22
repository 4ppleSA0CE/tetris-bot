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
