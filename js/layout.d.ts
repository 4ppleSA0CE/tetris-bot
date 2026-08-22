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
