import type { BotConfig, TetrisBot, Weights } from './types.js';
import type { SnapshotLayout } from './layout.js';
export type { BotConfig, Snapshot, SnapshotEvent, TetrisBot, Weights } from './types.js';
export { EventType, PieceLetter, SpinKind } from './types.js';
export interface BotModule {
    HEAPU8: Uint8Array;
    _malloc(bytes: number): number;
    _free(ptr: number): void;
    _sbrk(delta: number): number;
    getSnapshotLayout(): string;
    getSnapshotSize(): number;
    getSnapshotAlign(): number;
    getWeightsInfo(): string;
    getPieceCells(): string;
    botCreate(seed: number, pps: number, searchDepth: number, beamWidth: number): number;
    botTick(handle: number, nowMs: number): boolean;
    botSnapshotPtr(handle: number): number;
    botSetPPS(handle: number, pps: number): boolean;
    botQueueGarbage(handle: number, lines: number): boolean;
    botSetWeight(handle: number, index: number, value: number): boolean;
    botSetTimeBudget(handle: number, ms: number): boolean;
    botReset(handle: number, seed: number): boolean;
    botDestroy(handle: number): boolean;
    botLiveCount(): number;
}
export interface LoadedModule {
    mod: BotModule;
    layout: SnapshotLayout;
    structSize: number;
    weightIndex: ReadonlyMap<string, number>;
    defaultWeights: Weights;
}
export declare function loadBotModule(): Promise<LoadedModule>;
export declare function prefersReducedMotion(): boolean;
export declare function isViewportBelow(minWidthPx: number): boolean;
export declare function createTetrisBot(config?: BotConfig): Promise<TetrisBot>;
