import { createTetrisBot } from '../js/index.ts';
import { createRenderer } from '../renderers/canvas-mono.ts';

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
