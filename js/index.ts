import createBotModule from '../dist/bot.js';
import type { BotConfig, Snapshot, TetrisBot, Weights } from './types.js';
import type { SnapshotLayout } from './layout.js';
import { SnapshotView, computeStructSize, setPieceCells } from './layout.js';

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

const DEFAULT_SEED = 0;
const DEFAULT_PPS = 5;
const DEFAULT_DEPTH = 7;
const DEFAULT_WIDTH = 30;

const REDUCED_MOTION_TICKS = 8;
const REDUCED_MOTION_STEP_MS = 1000;

let loading: Promise<LoadedModule> | null = null;

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

export function prefersReducedMotion(): boolean {
  if (typeof window === 'undefined' || typeof window.matchMedia !== 'function') return false;
  return window.matchMedia('(prefers-reduced-motion: reduce)').matches;
}

export function isViewportBelow(minWidthPx: number): boolean {
  if (typeof window === 'undefined') return false;
  return window.innerWidth < minWidthPx;
}

export async function createTetrisBot(config: BotConfig = {}): Promise<TetrisBot> {
  const { mod, layout, structSize, weightIndex } = await loadBotModule();

  const handle = mod.botCreate(
    config.seed ?? DEFAULT_SEED,
    clampPPS(config.pps ?? DEFAULT_PPS),
    config.searchDepth ?? DEFAULT_DEPTH,
    config.beamWidth ?? DEFAULT_WIDTH,
  );

  if (config.searchBudgetMs !== undefined && config.searchBudgetMs > 0) {
    mod.botSetTimeBudget(handle, config.searchBudgetMs);
  }

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

  const reducedMotion = prefersReducedMotion() && config.ignoreReducedMotion !== true;
  if (reducedMotion) {

    for (let i = 1; i <= REDUCED_MOTION_TICKS; i++) {
      mod.botTick(handle, i * REDUCED_MOTION_STEP_MS);
    }
  }

  let destroyed = false;

  let hiddenOffsetMs = 0;
  let hiddenAtMs = -1;
  let lastVisibleNowMs = 0;
  let pendingResume = false;

  const onVisibilityChange = (): void => {
    if (typeof document === 'undefined') return;
    if (document.visibilityState === 'hidden') {
      if (hiddenAtMs < 0) hiddenAtMs = lastVisibleNowMs;
    } else if (hiddenAtMs >= 0) {

      pendingResume = true;
    }
  };

  if (typeof document !== 'undefined' && typeof document.addEventListener === 'function') {
    document.addEventListener('visibilitychange', onVisibilityChange);
  }

  const bot: TetrisBot = {
    tick(nowMs: number): void {
      if (destroyed || reducedMotion) return;
      if (typeof document !== 'undefined' && document.visibilityState === 'hidden') return;
      if (pendingResume) {
        pendingResume = false;
        hiddenOffsetMs += nowMs - hiddenAtMs;
        hiddenAtMs = -1;
      }
      lastVisibleNowMs = nowMs;
      mod.botTick(handle, nowMs - hiddenOffsetMs);
    },
    snapshot(): Snapshot {
      if (destroyed) throw new Error('snapshot() called after destroy()');
      return view.sync();
    },
    setPPS(pps: number): void {
      if (!destroyed) mod.botSetPPS(handle, clampPPS(pps));
    },
    queueGarbage(lines: number): void {
      if (!destroyed) mod.botQueueGarbage(handle, lines | 0);
    },
    reset(seed?: number): void {
      if (!destroyed) mod.botReset(handle, seed ?? -1);
    },
    destroy(): void {
      if (destroyed) return;
      destroyed = true;
      if (typeof document !== 'undefined' && typeof document.removeEventListener === 'function') {
        document.removeEventListener('visibilitychange', onVisibilityChange);
      }
      mod.botDestroy(handle);
    },
  };

  return bot;
}
