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

/**
 * Ticks used to settle a reduced-motion board. Each carries a full second of
 * simulated time, so at the default 5 PPS this places roughly 40 pieces before
 * the bot goes permanently static.
 */
const REDUCED_MOTION_TICKS = 8;
const REDUCED_MOTION_STEP_MS = 1000;

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

/**
 * PRD section 6: prefers-reduced-motion -> render a single static board.
 * Safe to call in Node and in a Worker; returns false where matchMedia is absent.
 */
export function prefersReducedMotion(): boolean {
  if (typeof window === 'undefined' || typeof window.matchMedia !== 'function') return false;
  return window.matchMedia('(prefers-reduced-motion: reduce)').matches;
}

/**
 * PRD section 6: "viewport below mobile breakpoint -> caller decides; wrapper
 * exposes the check." This is that check. The wrapper takes no action on it.
 */
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

  // config.ignoreReducedMotion is a deliberate per-instance opt-out; see js/types.ts.
  const reducedMotion = prefersReducedMotion() && config.ignoreReducedMotion !== true;
  if (reducedMotion) {
    // One static board, settled up front, then no motion ever.
    for (let i = 1; i <= REDUCED_MOTION_TICKS; i++) {
      mod.botTick(handle, i * REDUCED_MOTION_STEP_MS);
    }
  }

  let destroyed = false;

  // The core is handed a clock with all hidden time removed, so a tab that was
  // backgrounded for ten minutes resumes exactly where it paused instead of
  // trying to simulate ten minutes of Tetris on one frame.
  //
  // The subtracted span is measured from hiddenAtMs - the last timestamp the CORE
  // actually saw - and not from the last timestamp tick() was called with. Those
  // differ whenever ticks keep arriving while hidden (a host on setInterval, or the
  // handful of frames a browser still delivers before it throttles): those ticks
  // return early without advancing the core, so counting from them would leave the
  // whole hidden span unsubtracted and hand the core a jump forward on resume -
  // exactly the catch-up burst this code exists to prevent.
  let hiddenOffsetMs = 0;
  let hiddenAtMs = -1;
  let lastVisibleNowMs = 0;
  let pendingResume = false;

  const onVisibilityChange = (): void => {
    if (typeof document === 'undefined') return;
    if (document.visibilityState === 'hidden') {
      if (hiddenAtMs < 0) hiddenAtMs = lastVisibleNowMs;
    } else if (hiddenAtMs >= 0) {
      // The gap is folded in on the next tick, once we see the new timestamp.
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
