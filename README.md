# tetris-bot

A T-spin-capable Tetris bot. C++ core compiled to WebAssembly, monochrome canvas renderer, one accent color.

```bash
npm install && npm run demo      # http://localhost:5173
```

## What makes it play like that

**It is scored on attack value, not survival or line count.** There is no opponent — the evaluator scores each placement by the garbage it *would* send in a versus match. Under a survival or line-count objective a correct bot stacks flat at three rows and never T-spins, because in solo a T-spin double is strictly worse than a tetris. It would be optimal and completely boring. Under an attack objective the bot builds T-slots, holds back-to-back chains, and clears in bursts, so the stack climbs to 8–12 rows, collapses, and climbs again.

**Move generation is a BFS over the movement graph, not an enumeration of hard drops.** A T-spin placement is by definition one you cannot arrive at by dropping a piece straight down. Enumerating `(rotation, column)` pairs is faster and easier and makes T-spins literally unreachable, no matter how good the evaluator is. The BFS also hands the renderer the exact action sequence the piece took, which is why the on-screen slides, rotations, and wall kicks are real rather than synthesized.

## Architecture

```
core/        C++ engine, movegen, search, eval.   Zero web awareness.
bindings/    Snapshot struct, layout export, embind glue.
dist/        Committed build output: bot.js (single-file WASM) + bot.d.ts.
js/          TypeScript wrapper: createTetrisBot(), snapshot views, host behavior.
renderers/   canvas-mono.ts — Canvas 2D, themed entirely by CSS custom properties.
demo/        Standalone Vite page: board, stat line, callouts, PPS slider.
native/      CLI harness — runs the core natively, no browser.
tests/       tests.cpp (C++, assert-based) + *.mjs (the web layer, plain node).
```

| Layer | Owns | Language |
|---|---|---|
| Engine | Board state, SRS rotation, 7-bag, hold, lock, line clears, attack accounting | C++ |
| Move generator | BFS over reachable placements, spin detection, movement paths | C++ |
| Search | Beam search over the preview queue, hold branching | C++ |
| Evaluator | Feature extraction, weighted scoring | C++ |
| Bindings | Snapshot struct, layout export, embind entry points | C++ |
| Wrapper | Instance lifecycle, typed-array views, host-environment behavior | TS |
| Renderer | Canvas drawing, animation timing, theming, callouts | TS |

**Invariant: `core/` never knows a canvas, DOM, or browser exists.** It accepts a config, advances on `tick()`, and writes a state snapshot. Everything visual consumes that snapshot. This is what makes embedding the bot elsewhere an afternoon rather than a refactor.

### How state crosses the WASM boundary

Each bot instance owns one fixed `tb::Snapshot` struct in linear memory (156 bytes: board bitmask, active piece, hold, queue, an event ring, and counters). JS receives the pointer once at creation and holds typed-array views over it. Zero copy, zero allocation, no marshalling on the hot path.

Two things about that are easy to get wrong and are handled explicitly:

- **Byte offsets are never hardcoded in TypeScript.** C++ exports `getSnapshotLayout()`, built from `offsetof`, and the wrapper constructs every view from it at init. It also cross-checks its computed struct size against `sizeof(Snapshot)` and refuses to run if they disagree. Struct padding differs between compilers and hardcoded offsets produce silent, baffling corruption.
- **`ALLOW_MEMORY_GROWTH` detaches views.** When the wasm heap grows, the old `ArrayBuffer` is detached: a captured `DataView` throws, and a captured typed array silently reports length 0 and reads `undefined`. The pointer stays valid; only the JS view dies. `snapshot()` compares buffer identity against the buffer it built with and rebuilds the views when they differ — one reference comparison per frame.

Instances are addressed by integer handle, not by a JS object holding a C++ pointer. React strict mode double-mounts, and a recycled index would let a stale handle silently address a *different* live bot; the handle counter is monotonic, so a stale handle is always a clean miss.

## Quick start

```bash
git clone https://github.com/4ppleSA0CE/tetris-bot
cd tetris-bot
npm install
npm run demo            # http://localhost:5173
```

`dist/bot.js` is committed, so nothing above needs Emscripten.

## Embedding

Under 20 lines of host code, and no build-time toolchain:

```ts
import { createTetrisBot } from 'tetris-bot';
import { createRenderer } from 'tetris-bot/renderer';

const canvas = document.getElementById('bot') as HTMLCanvasElement;
const renderer = createRenderer({ canvas, layout: 'demo', chrome: 'minimal' });
const bot = await createTetrisBot({ seed: 42, pps: 5 });
renderer.resize();

let raf: number;
const loop = (t: number): void => {
  bot.tick(t);
  renderer.draw(bot.snapshot());
  raf = requestAnimationFrame(loop);
};
raf = requestAnimationFrame(loop);

window.addEventListener('pagehide', () => {
  cancelAnimationFrame(raf);
  bot.destroy();
});
```

The wrapper, not the core, owns host-environment behavior:

- `prefers-reduced-motion` → the bot settles one static board at construction and never animates
- `visibilitychange` → ticking pauses on a hidden tab, and hidden time is subtracted from the clock so returning does not trigger a catch-up burst
- `isViewportBelow(px)` is exported so the caller can decide what to do on mobile; the wrapper takes no action on it

### API

```ts
interface BotConfig {
  seed?: number;
  pps?: number;              // 1-20, default 5
  searchDepth?: number;      // default 5
  beamWidth?: number;        // default 100
  weights?: Partial<Weights>;
}

interface TetrisBot {
  tick(nowMs: number): void;   // advance the simulation to this timestamp
  snapshot(): Snapshot;        // live view; valid until the next tick()
  setPPS(pps: number): void;
  reset(seed?: number): void;
  destroy(): void;
}

declare function createTetrisBot(config?: BotConfig): Promise<TetrisBot>;
```

## Theming

The renderer ships no colors. It reads eight CSS custom properties from the canvas:

| Property | Used for |
|---|---|
| `--bot-bg` | Canvas background |
| `--bot-grid` | Well outline |
| `--bot-cell` | Locked cells, hold and queue previews |
| `--bot-cell-active` | The falling piece |
| `--bot-cell-ghost` | Drop preview outline |
| `--bot-text` | Reserved for host text |
| `--bot-text-dim` | The HUD stat line |
| `--bot-accent` | **Callouts only** |

**Discipline rule:** the accent appears nowhere except T-spin and B2B callouts. It is the only color on screen and it should stay that way — a single accent that occurs nowhere else lands disproportionately hard. `tests/renderer_discipline.mjs` enforces this: the renderer source must contain zero color literals, and `--bot-accent` must be read by exactly one function called from exactly one place.

`layout` is `'demo'` (well centred, HUD beneath) or `'sidebar'` (well on the right at `viewport height / 22` per cell). `chrome` is `'full'` (HUD + previews + callouts), `'minimal'` (callouts only), or `'none'` (board only).

## Animation

The core advances the piece along its real BFS path and reports the discrete state plus a `pathProgress` in 0–255. The renderer sub-interpolates with frame-rate-independent exponential smoothing (`alpha = 1 - exp(-dt/tau)`; `tau` is 45 ms for translation, 55 ms for rotation), and rotates the canvas by the residual angle about the piece origin so the piece swings into place yet always lands exactly on the SRS cells the search chose.

**Tempo dilation.** The last 25% of every placement's path normally takes its proportional share of the piece budget. When the placement is a spin, that tail is stretched to 200 ms of wall clock with an `easeOutCubic` deceleration. At 15 PPS a routine placement takes 66.7 ms and a T-spin takes 250 ms. This is how "insane speed" and "look what it just did" coexist instead of cancelling each other — at a flat 15 PPS an undilated T-spin is an unreadable blur. `DILATE_TETRIS` in `bindings/bot_instance.h` extends the same treatment to tetrises; it defaults to off.

## Building from source

```bash
source ~/emsdk/emsdk_env.sh    # emcc 6.0.8, pinned in .emscripten-version
make wasm                      # -> dist/bot.js, dist/bot.d.ts
make js                        # -> js/*.js, renderers/*.js
make test                      # C++ unit tests
make web-test                  # every web-layer test
```

Native development does not need Emscripten:

```bash
make                           # native CLI -> build/tetris_bot
./build/tetris_bot --seed 1 --pieces 10000 --stats
```

## Tuning weights

Weights are hand-tuned against CLI statistics. The named constants live in `core/weights.h`, which documents what each value was measured against and what was rejected on either side of it; `defaultWeights()` in `core/eval.cpp` reads them. They are exported to JS by name via `getWeightsInfo()` and are overridable per instance:

```ts
const bot = await createTetrisBot({ weights: { tSlotCount: 240, attackDealt: 140 } });
```

**Note the scale: these are hundreds, not single digits.** `attackDealt` is the unit — 100 points is one garbage line of expected attack — so every other weight is quoted in hundredths of a garbage line (`holes` is -110, i.e. one hole costs 1.1 attack). Passing `tSlotCount: 3.5` does not nudge T-spin seeking, it switches it off.

**`maxHeight` is positive, and that is deliberate.** It is the one weight whose sign departs from "board-health terms are negative", and it is the reason the bot stacks at all. With every health term negative, nothing in the evaluator ever rewards building: the measured result was a 2.7-row pancake with zero tetrises and a max back-to-back of 1, because clearing a single pays no attack yet still improves every negative term — the health weights were paying the bot to break its own chain. `maxHeight` is a bounded reward and `heightPenalty` is the cliff that bounds it, applied to `(height - 12)²`. **They are a matched pair and must be changed together:** raising the reward without the cliff stacks into the ceiling, raising the cliff without the reward pins the stack flat again.

Two things worth knowing before you sweep anything:

- **`holes` is the only weight keeping the bot alive.** Single-weight ablation: zero it and the run tops out at piece 107, while zeroing any *other* weight individually costs no survival at all. If the bot starts topping out, look here first.
- **Tune at the shipped time budget, never at an unlimited one.** The search is anytime, and a taller stack needs deeper search to dig out of, so stack ambition and the 5 ms budget are in direct tension. A vector tuned with `--budget 1000000` met every target and then died 0 for 3 at the real budget.

## Non-goals

No versus mode, no garbage, no second board, no human playability, no automated weight optimization, no perfect-clear finder, no threads, no `SharedArrayBuffer`.

## License

MIT
