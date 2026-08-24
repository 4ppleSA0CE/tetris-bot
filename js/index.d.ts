import type { BotConfig, TetrisBot, Weights } from './types.js';
import type { SnapshotLayout } from './layout.js';
export type { BotConfig, Snapshot, SnapshotEvent, TetrisBot, Weights } from './types.js';
export { EventType, PieceLetter, SpinKind } from './types.js';
/** The flat C API exposed by bindings/embind.cpp. */
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
/**
 * Instantiate the wasm module once per page. Memoized: a second instantiation
 * would create a second heap and a second, disjoint handle space.
 *
 * NOT part of the PRD section 6 surface. That surface is createTetrisBot,
 * prefersReducedMotion, isViewportBelow and the types - "deliberately small.
 * Every additional function is friction at embed time." This is exported only
 * so tests/*.mjs and the renderer tests can reach the piece-cell table and the
 * live-instance count without a second module instance. Hosts do not call it,
 * and nothing here may become load-bearing for an embed.
 */
export declare function loadBotModule(): Promise<LoadedModule>;
/**
 * PRD section 6: prefers-reduced-motion -> render a single static board.
 * Safe to call in Node and in a Worker; returns false where matchMedia is absent.
 */
export declare function prefersReducedMotion(): boolean;
/**
 * PRD section 6: "viewport below mobile breakpoint -> caller decides; wrapper
 * exposes the check." This is that check. The wrapper takes no action on it.
 */
export declare function isViewportBelow(minWidthPx: number): boolean;
export declare function createTetrisBot(config?: BotConfig): Promise<TetrisBot>;
