// TypeScript bindings for emscripten-generated code.  Automatically generated at compile time.
declare namespace RuntimeExports {
    let HEAPU8: Uint8Array;
}
interface WasmModule {
  _malloc(_0: number): number;
  _sbrk(_0: number): number;
  _free(_0: number): void;
}

interface EmbindModule {
  getSnapshotSize(): number;
  getSnapshotAlign(): number;
  botReset(_0: number, _1: number): boolean;
  botQueueGarbage(_0: number, _1: number): boolean;
  botDestroy(_0: number): boolean;
  botLiveCount(): number;
  botSnapshotPtr(_0: number): number;
  botCreate(_0: number, _1: number, _2: number, _3: number): number;
  botSetPPS(_0: number, _1: number): boolean;
  botSetTimeBudget(_0: number, _1: number): boolean;
  botSetWeight(_0: number, _1: number, _2: number): boolean;
  botTick(_0: number, _1: number): boolean;
  getSnapshotLayout(): string;
  getWeightsInfo(): string;
  getPieceCells(): string;
}

export type MainModule = WasmModule & typeof RuntimeExports & EmbindModule;
export default function MainModuleFactory (options?: unknown): Promise<MainModule>;
