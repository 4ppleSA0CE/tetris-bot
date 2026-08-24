# tetris-bot

A T-spin-capable Tetris bot. C++ core compiled to WebAssembly, monochrome canvas renderer, one accent color.

```bash
npm install && npm run demo      # http://localhost:5173
```

## What makes it play like that

**It is scored on attack value, not survival or line count.** There is no opponent — the evaluator scores each placement by the garbage it *would* send in a versus match. Under a survival or line-count objective a correct bot stacks flat at three rows and never T-spins, because in solo a T-spin double is strictly worse than a tetris. It would be optimal and completely boring. Under an attack objective the bot builds T-slots, holds back-to-back chains, and clears in bursts, so the stack climbs to 8–12 rows, collapses, and climbs again.

**The rules are TETR.IO Season 2 (All-Mini+, back-to-back charging).** Every piece can spin: T by the three-corner rule, everything else — I and O included — by *immobility*: after a rotation the piece cannot move left, right, up or down. The up check is what matters; a piece dropped into a snug slot with open sky is not a spin, but the classic S/Z notch is, because the piece's own shape catches the stack. Non-T spins are minis and pay 0/1/2/4 for 1/2/3/4 lines, so their value is keeping the chain alive: from a chain of 4 the bot holds a *Surge* equal to the chain length, released by whatever clear finally breaks it, and the evaluator counts that stored garbage (`b2bCharge`) so breaking a long chain never reads as free attack. Attack is the multiplayer table, the multiplier combo (`x(1 + 0.25c)`, log floor for zero-base clears), +1 per back-to-back, All Clear +5. Under these rules a 10,000-piece run sends ~4,500 attack (810 of it Surge) with a longest chain of 45, zero top-outs and a peak stack of 19 rows.

**Move generation is a BFS over the movement graph, not an enumeration of hard drops.** A T-spin placement is by definition one you cannot arrive at by dropping a piece straight down. Enumerating `(rotation, column)` pairs is faster and easier and makes T-spins literally unreachable, no matter how good the evaluator is. The BFS also hands the renderer the exact action sequence the piece took, which is why the on-screen slides, rotations, and wall kicks are real rather than synthesized.

## Architecture

```
core/        C++ engine, movegen, search, eval.   Zero web awareness.
bindings/    Snapshot struct, layout export, embind glue.
dist/        Committed build output: bot.js (single-file WASM) + bot.d.ts.
js/          TypeScript wrapper: createTetrisBot(), snapshot views, host behavior.
renderers/   canvas-mono.ts — Canvas 2D, themed entirely by CSS custom properties.
demo/        Standalone Vite page: board, stat line, callouts, PPS slider. Fresh seed per load; ?seed=42 replays the README game.
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
| `--bot-cell` | Fallback for a cell with no piece identity |
| `--bot-cell-active` | Fallback for the falling piece |
| `--bot-cell-ghost` | Fallback for the drop preview outline |
| `--bot-text` | Reserved for host text |
| `--bot-text-dim` | The HUD stat line |
| `--bot-accent` | **Callouts only** |
| `--bot-piece-i` … `--bot-piece-z` | One per piece: locked cells, the falling piece, the ghost outline, and the hold/queue previews |

Cells are painted by **piece**, not by state. `Snapshot.cellPiece` carries the piece that
locked into each cell (`rows` is only a bitmask and has no piece identity), so a settled S
stays an S. The three `--bot-cell*` properties survive as the per-context fallback for a cell
whose piece is unknown, which is what a monochrome host sets to go back to greyscale.

**Discipline rule:** the accent appears nowhere except T-spin and B2B callouts. It is the only color on screen and it should stay that way — a single accent that occurs nowhere else lands disproportionately hard. `tests/renderer_discipline.mjs` enforces this: the renderer source must contain zero color literals, and `--bot-accent` must be read by exactly one function called from exactly one place.

`layout` is `'demo'` (well centred, HUD beneath) or `'sidebar'` (well on the right at `viewport height / 22` per cell). `chrome` is `'full'` (HUD + previews + callouts), `'minimal'` (callouts only), or `'none'` (board only).

## Movement

**Discrete, cell-snapped, no interpolation** — jstris / TETR.IO handling. The core advances the
piece one step at a time along the exact BFS path it found, and the renderer paints it on that
cell. The piece still visibly walks its route and performs its wall kicks; there is simply no
easing between cells, no rotation swing, and no per-frame animation state. What the core reports
is what gets painted, which is also what makes the position assertable in a test.

**Pacing is uniform.** Every placement occupies exactly one piece interval, `1000/pps` ms,
whether or not it is a spin. An earlier build stretched the tail of a spin to 200 ms to make it
readable at high speed; that was removed, because the handling this follows paces everything
identically and warping the clock is the kind of animation this build deliberately does not do.
`tests/animation.mjs` and `test_uniform_pacing` assert the removal stays removed.

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

`--stats` reports pieces, lines, attack (Surge included), spins per 100 pieces, max back-to-back, total Surge released, top-outs and search-time percentiles; `--heights` adds the stack-height profile; `--json` prints the same as one machine-readable object. `--garbage L/P` queues L lines of TETR.IO-style garbage every P pieces (attack sent cancels it first, `--messiness` moves the hole column, default 0.05), which is how survival under pressure is measured. `--list-weights` prints every weight with its compiled default. `--versus "name=value,..."` plays two bots in one process — A is `--weights`, B is the versus spec on its own `--seed2` bag — exchanging surplus attack as garbage (cancel first, TETR.IO-style); first top-out loses. `--dupes` reports what fraction of surviving beam slots are duplicate states. `--nodes N` replaces the clock with a deterministic budget of N scored children per search (`--stats` prints the average the clock reaches, about 5,200 on an idle M4 performance core), so two runs of the same seed agree exactly and every core gives the same answer.

## Tuning weights

The named constants live in `core/weights.h`, which records what each vector was measured against; `defaultWeights()` in `core/eval.cpp` reads them. They are exported to JS by name via `getWeightsInfo()` and are overridable per instance:

```ts
const bot = await createTetrisBot({ weights: { tSlotCount: 240, attackDealt: 140 } });
```

**Note the scale: these are hundreds, not single digits.** `attackDealt` is the unit — 100 points is one garbage line of expected attack — so every other weight is quoted in hundredths of a garbage line (`holes` is -112, i.e. one hole costs about 1.1 attack). Passing `tSlotCount: 3.5` does not nudge T-spin seeking, it switches it off.

**`maxHeight` is positive, and that is deliberate.** It is the one weight whose sign departs from "board-health terms are negative", and it is the reason the bot stacks at all. With every health term negative, nothing in the evaluator ever rewards building: the measured result was a 2.7-row pancake with zero tetrises and a max back-to-back of 1, because clearing a single pays no attack yet still improves every negative term — the health weights were paying the bot to break its own chain. `maxHeight` is a bounded reward and `heightPenalty` is the cliff that bounds it, applied to `(height - 12)²`. **They are a matched pair and must be changed together:** raising the reward without the cliff stacks into the ceiling, raising the cliff without the reward pins the stack flat again.

The search folds dominated transpositions out of the beam (half of all beam slots were duplicate states reached along different paths; folding them halved the cost of a full-depth search), and it is told about incoming garbage: pending lines are charged through `incomingRisk` as the extra height-cliff area they would add, and attack earned on a path cancels them first. Besides the board terms above, four come from the versus-bot literature (Cold Clear, ZZZ, BCTS): `rowsWithHoles` (a second hole in a holed row is nearly free, a hole in a clean row costs the row), `overhangs` (a positive refund for holes a piece can still slide into from the side), and two per-move terms the search applies like `attackDealt`: `plainClear` (a clear that does not keep back-to-back) and `wastedT` (a T placed without a spin).

Weights are tuned by `tools/tune.py`, a noisy cross-entropy loop: every candidate in a generation plays the same seeds (so candidates are compared, not seeds), once solo and once under `--garbage 4/16`; fitness is fewer top-outs first, then more attack; the elite refits the mean and variance. With `--duel PAIRS` the fitness is versus wins instead: each candidate plays the incumbent mean in paired-orientation games (net attack margin breaks ties), which is what shipped the plan-8 vector after solo-attack fitness proved a bad proxy in both directions. `tools/bench.py` is the gate: the same seeds for a candidate and a baseline, attack per piece with a 95% interval, paired difference, top-outs.

```bash
python3 tools/tune.py --gens 30 --pop 50 --elite 6 --pieces 1500 --workers 10 --nodes 6400
python3 tools/tune.py --duel 5 --gens 20 --pop 40 --elite 6 --pieces 400 --workers 10 --nodes 6400
python3 tools/bench.py --weights "wastedT=-120" --baseline "" --seeds 8 --pieces 3000
python3 tools/bench.py --baseline "" --garbage 4/16      # survival under pressure
```

Tune with `--nodes` (calibrated to the shipped budget; 6400 since the plan-9 throughput round — pathless interior movegen, surface-seeded BFS and insertion dedup lifted the 4.5 ms clock from ~5200 to ~6500 scored children) so common random numbers actually work and every core is usable; gate with `bench.py` on the wall clock, one process at a time, because the clock is what ships. `bench.py` interleaves candidate and baseline per seed so ambient load drift lands on both, and takes `--bin` / `--bin-base` for cross-binary A/B. `tools/duel.py` plays paired head-to-head matches (roles and seeds mirrored, so seat and seed luck cancel) — `--nodes-b` gives the opponent a different node budget, which is how a throughput change is priced in Elo and reports win rate, Elo and LOS — the versus record is the real metric and the mandatory ship gate; solo attack per piece is only a screening proxy, and the plan-8 duel-tuned vector beats the solo-tuned one 56-24 (LOS 1.000) while scoring far lower on the solo bench.

Things worth knowing before you touch anything:

- **The score is noisy.** Attack per piece over a few thousand pieces has a coefficient of variation around 1 across seeds, and the anytime search adds wall-clock noise on top — two runs of the same weights differ. Never trust a single seed; read the interval `bench.py` prints.
- **`holes` is the only weight keeping the bot alive.** Single-weight ablation: zero it and the run tops out at piece 107, while zeroing any *other* weight individually costs no survival at all. If the bot starts topping out, look here first.
- **Tune at the shipped time budget, never at an unlimited one.** The search is anytime, and a taller stack needs deeper search to dig out of, so stack ambition and the 5 ms budget are in direct tension. A vector tuned with `--budget 1000000` met every target and then died 0 for 3 at the real budget. `tune.py` has no `--budget` flag on purpose; `--nodes` is the same horizon without the jitter.

## Non-goals

No versus mode, no second board, no human playability, no perfect-clear finder, no threads, no `SharedArrayBuffer`. Garbage exists only as CLI pressure for tuning; the web bot never receives any.

## License

MIT
