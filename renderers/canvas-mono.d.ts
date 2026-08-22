import type { Snapshot } from '../js/types.js';
export interface RendererOptions {
    canvas: HTMLCanvasElement;
    layout: 'demo' | 'sidebar';
    chrome: 'full' | 'minimal' | 'none';
}
export interface Renderer {
    draw(snapshot: Snapshot): void;
    resize(): void;
    destroy(): void;
}
export declare function createRenderer(opts: RendererOptions): Renderer;
