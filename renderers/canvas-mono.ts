import type { Snapshot } from '../js/types.js';
import { getPieceCells } from '../js/layout.js';

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

// --- constants -------------------------------------------------------------
const VISIBLE_ROWS = 20;
const BOARD_COLS = 10;
/** Width of the hold/queue column, in cells. */
const SIDE_CELLS = 4.5;
/** Fraction of the demo layout's width reserved for that column. */
const SIDE_FRACTION = 0.3;
const HUD_HEIGHT = 22;
const CELL_GAP = 1;

/**
 * Not a color. `currentColor` resolves to the host element's own `color`, and
 * exists solely so a host that forgot to set the custom properties renders
 * something instead of `undefined`. PRD section 7.1: ship no hardcoded colors.
 */
const FALLBACK = 'currentColor';

interface Theme {
  bg: string;
  grid: string;
  cell: string;
  cellActive: string;
  cellGhost: string;
  text: string;
  textDim: string;
}

interface Geometry {
  dpr: number;
  w: number;
  h: number;
  cell: number;
  wellX: number;
  wellY: number;
  wellW: number;
  wellH: number;
  sideX: number;
  hudY: number;
  calloutX: number;
  calloutY: number;
  calloutAlign: CanvasTextAlign;
}

/**
 * The seven non-accent properties. --bot-accent is deliberately absent: it is
 * read only by readAccent(), only from drawCallouts().
 */
function readTheme(el: HTMLElement): Theme {
  const cs = getComputedStyle(el);
  const v = (name: string): string => cs.getPropertyValue(name).trim() || FALLBACK;
  return {
    bg: v('--bot-bg'),
    grid: v('--bot-grid'),
    cell: v('--bot-cell'),
    cellActive: v('--bot-cell-active'),
    cellGhost: v('--bot-cell-ghost'),
    text: v('--bot-text'),
    textDim: v('--bot-text-dim'),
  };
}

function computeGeometry(
  canvas: HTMLCanvasElement,
  layout: 'demo' | 'sidebar',
  chrome: 'full' | 'minimal' | 'none',
): Geometry {
  const rect = canvas.getBoundingClientRect();
  const w = Math.max(1, Math.round(rect.width || canvas.width || 300));
  const h = Math.max(1, Math.round(rect.height || canvas.height || 660));
  const dpr = typeof devicePixelRatio === 'number' && devicePixelRatio > 0 ? devicePixelRatio : 1;

  const hudH = chrome === 'full' ? HUD_HEIGHT : 0;
  const sideFrac = chrome === 'none' ? 0 : SIDE_FRACTION;

  // sidebar: PRD section 7.5 fixes cell size at viewport height / 22.
  const cell = layout === 'sidebar'
    ? Math.max(2, Math.floor(h / 22))
    : Math.max(2, Math.floor(Math.min(
        (h - hudH - 8) / VISIBLE_ROWS,
        ((w - 8) * (1 - sideFrac)) / BOARD_COLS,
      )));

  const wellW = cell * BOARD_COLS;
  const wellH = cell * VISIBLE_ROWS;

  let wellX: number;
  let wellY: number;
  let sideX: number;
  let calloutX: number;
  let calloutAlign: CanvasTextAlign;

  if (layout === 'sidebar') {
    wellX = w - wellW - 4;
    wellY = Math.round((h - wellH) / 2);
    sideX = wellX - cell * SIDE_CELLS;
    calloutX = wellX - cell * 0.75;
    calloutAlign = 'right';
  } else {
    const groupW = wellW + (sideFrac > 0 ? cell * SIDE_CELLS : 0);
    wellX = Math.round((w - groupW) / 2);
    wellY = Math.round((h - hudH - wellH) / 2);
    sideX = wellX + wellW + cell * 0.75;
    calloutX = sideX;
    calloutAlign = 'left';
  }

  return {
    dpr, w, h, cell, wellX, wellY, wellW, wellH, sideX,
    hudY: wellY + wellH + HUD_HEIGHT * 0.7,
    calloutX,
    calloutY: wellY + cell * 2,
    calloutAlign,
  };
}

const cellLeft = (g: Geometry, x: number): number => g.wellX + x * g.cell;
const cellTop = (g: Geometry, y: number): number =>
  g.wellY + (VISIBLE_ROWS - 1 - y) * g.cell;

function drawWell(ctx: CanvasRenderingContext2D, th: Theme, g: Geometry): void {
  ctx.strokeStyle = th.grid;
  ctx.lineWidth = 1;
  ctx.strokeRect(g.wellX + 0.5, g.wellY + 0.5, g.wellW, g.wellH);
}

function drawLockedCells(
  ctx: CanvasRenderingContext2D, s: Snapshot, th: Theme, g: Geometry,
): void {
  ctx.fillStyle = th.cell;
  const size = g.cell - CELL_GAP;
  for (let y = 0; y < VISIBLE_ROWS; y++) {
    const bits = s.rows[y] ?? 0;
    if (bits === 0) continue;
    for (let x = 0; x < BOARD_COLS; x++) {
      if ((bits >> x) & 1) ctx.fillRect(cellLeft(g, x), cellTop(g, y), size, size);
    }
  }
}

function drawGhost(
  ctx: CanvasRenderingContext2D, s: Snapshot, th: Theme, g: Geometry,
): void {
  if (s.activePiece < 0) return;
  const cells = getPieceCells(s.activePiece, s.activeRot);
  ctx.strokeStyle = th.cellGhost;
  ctx.lineWidth = 1;
  const size = g.cell - CELL_GAP;
  for (let i = 0; i < cells.length; i += 2) {
    const x = s.activeX + (cells[i] ?? 0);
    const y = s.ghostY + (cells[i + 1] ?? 0);
    if (y < 0 || y >= VISIBLE_ROWS) continue;
    ctx.strokeRect(cellLeft(g, x) + 0.5, cellTop(g, y) + 0.5, size, size);
  }
}

function drawActivePiece(
  ctx: CanvasRenderingContext2D, s: Snapshot, th: Theme, g: Geometry,
): void {
  if (s.activePiece < 0) return;
  const cells = getPieceCells(s.activePiece, s.activeRot);
  ctx.fillStyle = th.cellActive;
  const size = g.cell - CELL_GAP;
  for (let i = 0; i < cells.length; i += 2) {
    const x = s.activeX + (cells[i] ?? 0);
    const y = s.activeY + (cells[i + 1] ?? 0);
    if (y < 0 || y >= VISIBLE_ROWS) continue;
    ctx.fillRect(cellLeft(g, x), cellTop(g, y), size, size);
  }
}

export function createRenderer(opts: RendererOptions): Renderer {
  const canvas = opts.canvas;
  const ctx = canvas.getContext('2d');
  if (ctx === null) throw new Error('createRenderer: canvas has no 2d context');

  let geo = computeGeometry(canvas, opts.layout, opts.chrome);
  let observer: ResizeObserver | null = null;

  const resize = (): void => {
    geo = computeGeometry(canvas, opts.layout, opts.chrome);
    canvas.width = Math.round(geo.w * geo.dpr);
    canvas.height = Math.round(geo.h * geo.dpr);
  };

  if (typeof ResizeObserver !== 'undefined') {
    observer = new ResizeObserver(() => resize());
    observer.observe(canvas);
  }

  const draw = (s: Snapshot): void => {
    const th = readTheme(canvas);
    const g = geo;
    ctx.setTransform(g.dpr, 0, 0, g.dpr, 0, 0);
    ctx.fillStyle = th.bg;
    ctx.fillRect(0, 0, g.w, g.h);
    drawWell(ctx, th, g);
    drawLockedCells(ctx, s, th, g);
    drawGhost(ctx, s, th, g);
    drawActivePiece(ctx, s, th, g);
  };

  const destroy = (): void => {
    if (observer !== null) {
      observer.disconnect();
      observer = null;
    }
  };

  return { draw, resize, destroy };
}
