# tetris-bot

A T-spin-capable Tetris bot written in C++20, compiled to WebAssembly, with a
TypeScript canvas renderer. It plays solo, continuously, and is scored on the
attack it *would* send in a versus match — which is what makes it build T-slots
and chain back-to-backs instead of stacking flat and playing dull.

## Build

```
make          # builds build/tetris_bot and build/tb_tests
make test     # builds and runs the assert-based test binary
make clean    # removes build/
```

Requires CMake >= 3.20 and a C++20 compiler. ninja is not required.

## CLI

```
./build/tetris_bot --seed 42 --pieces 50 --print --random
```

## Layout

| Path | Contents |
|---|---|
| `core/` | Engine. Zero web awareness. |
| `native/` | CLI harness — runs the core natively, no browser. |
| `tests/` | `tests.cpp`, a single assert-based test binary. |
