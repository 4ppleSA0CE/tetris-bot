import createBotModule from '../dist/bot.js';
import type { BotConfig, Snapshot, TetrisBot, Weights } from './types.js';
import type { SnapshotLayout } from './layout.js';
import { SnapshotView, computeStructSize, setPieceCells } from './layout.js';

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

const DEFAULT_SEED = 0;
const DEFAULT_PPS = 5;
const DEFAULT_DEPTH = 5;
const DEFAULT_WIDTH = 100;

let loading: Promise<LoadedModule> | null = null;

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
export function loadBotModule(): Promise<LoadedModule> {
  loading ??= (createBotModule() as unknown as Promise<BotModule>).then((mod) => {
    const layout = JSON.parse(mod.getSnapshotLayout()) as SnapshotLayout;
    const align = mod.getSnapshotAlign();
    const structSize = computeStructSize(layout, align);
    const declared = mod.getSnapshotSize();
    if (structSize !== declared) {
      throw new Error(
        `snapshot layout mismatch: JS computed ${structSize} bytes, C++ sizeof(Snapshot) is ${declared}`,
      );
    }
    const info = JSON.parse(mod.getWeightsInfo()) as {
      name: string; index: number; default: number;
    }[];
    const weightIndex = new Map(info.map((w) => [w.name, w.index] as const));
    const defaultWeights = Object.fromEntries(
      info.map((w) => [w.name, w.default]),
    ) as unknown as Weights;
    setPieceCells(JSON.parse(mod.getPieceCells()) as number[][][]);
    return { mod, layout, structSize, weightIndex, defaultWeights };
  });
  return loading;
}

function clampPPS(pps: number): number {
  if (!Number.isFinite(pps)) return DEFAULT_PPS;
  return Math.min(20, Math.max(1, pps));
}

export async function createTetrisBot(config: BotConfig = {}): Promise<TetrisBot> {
  const { mod, layout, structSize, weightIndex } = await loadBotModule();

  const handle = mod.botCreate(
    config.seed ?? DEFAULT_SEED,
    clampPPS(config.pps ?? DEFAULT_PPS),
    config.searchDepth ?? DEFAULT_DEPTH,
    config.beamWidth ?? DEFAULT_WIDTH,
  );

  if (config.weights) {
    for (const [name, value] of Object.entries(config.weights)) {
      const index = weightIndex.get(name);
      if (index === undefined) {
        mod.botDestroy(handle);
        throw new Error(`unknown weight "${name}"`);
      }
      mod.botSetWeight(handle, index, value as number);
    }
  }

  const view = new SnapshotView(mod, mod.botSnapshotPtr(handle), layout, structSize);
  let destroyed = false;

  const bot: TetrisBot = {
    tick(nowMs: number): void {
      if (destroyed) return;
      mod.botTick(handle, nowMs);
    },
    snapshot(): Snapshot {
      if (destroyed) throw new Error('snapshot() called after destroy()');
      return view.sync();
    },
    setPPS(pps: number): void {
      if (!destroyed) mod.botSetPPS(handle, clampPPS(pps));
    },
    reset(seed?: number): void {
      if (!destroyed) mod.botReset(handle, seed ?? -1);
    },
    destroy(): void {
      if (destroyed) return;
      destroyed = true;
      mod.botDestroy(handle);
    },
  };

  return bot;
}
