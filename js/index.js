import createBotModule from '../dist/bot.js';
import { SnapshotView, computeStructSize, setPieceCells } from './layout.js';
export { EventType, PieceLetter, SpinKind } from './types.js';
const DEFAULT_SEED = 0;
const DEFAULT_PPS = 5;
const DEFAULT_DEPTH = 7;
const DEFAULT_WIDTH = 30;
const REDUCED_MOTION_TICKS = 8;
const REDUCED_MOTION_STEP_MS = 1000;
let loading = null;
export function loadBotModule() {
    loading ??= createBotModule().then((mod) => {
        const layout = JSON.parse(mod.getSnapshotLayout());
        const align = mod.getSnapshotAlign();
        const structSize = computeStructSize(layout, align);
        const declared = mod.getSnapshotSize();
        if (structSize !== declared) {
            throw new Error(`snapshot layout mismatch: JS computed ${structSize} bytes, C++ sizeof(Snapshot) is ${declared}`);
        }
        const info = JSON.parse(mod.getWeightsInfo());
        const weightIndex = new Map(info.map((w) => [w.name, w.index]));
        const defaultWeights = Object.fromEntries(info.map((w) => [w.name, w.default]));
        setPieceCells(JSON.parse(mod.getPieceCells()));
        return { mod, layout, structSize, weightIndex, defaultWeights };
    });
    return loading;
}
function clampPPS(pps) {
    if (!Number.isFinite(pps))
        return DEFAULT_PPS;
    return Math.min(20, Math.max(1, pps));
}
export function prefersReducedMotion() {
    if (typeof window === 'undefined' || typeof window.matchMedia !== 'function')
        return false;
    return window.matchMedia('(prefers-reduced-motion: reduce)').matches;
}
export function isViewportBelow(minWidthPx) {
    if (typeof window === 'undefined')
        return false;
    return window.innerWidth < minWidthPx;
}
export async function createTetrisBot(config = {}) {
    const { mod, layout, structSize, weightIndex } = await loadBotModule();
    const handle = mod.botCreate(config.seed ?? DEFAULT_SEED, clampPPS(config.pps ?? DEFAULT_PPS), config.searchDepth ?? DEFAULT_DEPTH, config.beamWidth ?? DEFAULT_WIDTH);
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
            mod.botSetWeight(handle, index, value);
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
    const onVisibilityChange = () => {
        if (typeof document === 'undefined')
            return;
        if (document.visibilityState === 'hidden') {
            if (hiddenAtMs < 0)
                hiddenAtMs = lastVisibleNowMs;
        }
        else if (hiddenAtMs >= 0) {
            pendingResume = true;
        }
    };
    if (typeof document !== 'undefined' && typeof document.addEventListener === 'function') {
        document.addEventListener('visibilitychange', onVisibilityChange);
    }
    const bot = {
        tick(nowMs) {
            if (destroyed || reducedMotion)
                return;
            if (typeof document !== 'undefined' && document.visibilityState === 'hidden')
                return;
            if (pendingResume) {
                pendingResume = false;
                hiddenOffsetMs += nowMs - hiddenAtMs;
                hiddenAtMs = -1;
            }
            lastVisibleNowMs = nowMs;
            mod.botTick(handle, nowMs - hiddenOffsetMs);
        },
        snapshot() {
            if (destroyed)
                throw new Error('snapshot() called after destroy()');
            return view.sync();
        },
        setPPS(pps) {
            if (!destroyed)
                mod.botSetPPS(handle, clampPPS(pps));
        },
        queueGarbage(lines) {
            if (!destroyed)
                mod.botQueueGarbage(handle, lines | 0);
        },
        reset(seed) {
            if (!destroyed)
                mod.botReset(handle, seed ?? -1);
        },
        destroy() {
            if (destroyed)
                return;
            destroyed = true;
            if (typeof document !== 'undefined' && typeof document.removeEventListener === 'function') {
                document.removeEventListener('visibilitychange', onVisibilityChange);
            }
            mod.botDestroy(handle);
        },
    };
    return bot;
}
