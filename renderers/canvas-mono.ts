import type { Snapshot } from '../js/types.js';
import { PieceLetter } from '../js/types.js';
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

const VISIBLE_ROWS = 20;
const BOARD_COLS = 10;

const SIDE_CELLS = 4.5;
const HOLD_CELLS = 3;

const HUD_HEIGHT = 22;
const CELL_GAP = 1;
const FONT_STACK = 'ui-monospace, SFMono-Regular, Menlo, monospace';

const CELL_EMPTY = 255;

const PIECE_VARS = [
  '--bot-piece-i', '--bot-piece-j', '--bot-piece-l', '--bot-piece-o',
  '--bot-piece-s', '--bot-piece-t', '--bot-piece-z',
] as const;

const CALLOUT_LIFE_MS = 800;
const CALLOUT_POP_MS = 120;
const CALLOUT_SCALE_FROM = 0.72;
const MAX_CALLOUTS = 4;

const FALLBACK = 'currentColor';

interface Theme {
  bg: string;
  grid: string;
  cell: string;
  cellActive: string;
  cellGhost: string;
  text: string;
  textDim: string;

  piece: string[];
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
  holdX: number;
  hudY: number;
  calloutX: number;
  calloutY: number;
  calloutAlign: CanvasTextAlign;
}

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
    piece: PIECE_VARS.map(v),
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

  const holdCells = chrome === 'none' ? 0 : HOLD_CELLS;
  const sideCells = chrome === 'none' ? 0 : SIDE_CELLS;

  const cell = layout === 'sidebar'
    ? Math.max(2, Math.floor(h / 22))
    : Math.max(2, Math.floor(Math.min(
        (h - hudH - 8) / VISIBLE_ROWS,
        (w - 8) / (BOARD_COLS + holdCells + sideCells),
      )));

  const wellW = cell * BOARD_COLS;
  const wellH = cell * VISIBLE_ROWS;

  let wellX: number;
  let wellY: number;
  let sideX: number;
  let holdX: number;
  let calloutX: number;
  let calloutAlign: CanvasTextAlign;

  if (layout === 'sidebar') {
    wellX = w - wellW - 4;
    wellY = Math.round((h - wellH) / 2);
    sideX = wellX - cell * SIDE_CELLS;
    holdX = sideX;
    calloutX = wellX - cell * 0.75;
    calloutAlign = 'right';
  } else {
    const groupW = wellW + cell * (holdCells + sideCells);
    wellX = Math.round((w - groupW) / 2) + cell * holdCells;
    wellY = Math.round((h - hudH - wellH) / 2);
    sideX = wellX + wellW + cell * 0.75;
    holdX = wellX - cell * holdCells + cell * 0.25;
    calloutX = sideX;
    calloutAlign = 'left';
  }

  return {
    dpr, w, h, cell, wellX, wellY, wellW, wellH, sideX, holdX,
    hudY: wellY + wellH + HUD_HEIGHT * 0.7,
    calloutX,
    calloutY: wellY + cell * 2,
    calloutAlign,
  };
}

function pieceFill(th: Theme, piece: number, fallback: string): string {
  return (piece >= 0 && piece < 7 ? th.piece[piece] : undefined) ?? fallback;
}

const cellLeft = (g: Geometry, x: number): number => g.wellX + x * g.cell;
const cellTop = (g: Geometry, y: number): number =>
  g.wellY + (VISIBLE_ROWS - 1 - y) * g.cell;

function drawWell(ctx: CanvasRenderingContext2D, th: Theme, g: Geometry): void {
  ctx.strokeStyle = th.grid;
  ctx.lineWidth = 1;
  ctx.strokeRect(g.wellX + 0.5, g.wellY + 0.5, g.wellW, g.wellH);
}

function drawPendingGarbage(
  ctx: CanvasRenderingContext2D, s: Snapshot, th: Theme, g: Geometry,
): void {
  const lines = Math.min(s.pendingGarbage ?? 0, VISIBLE_ROWS);
  if (lines <= 0) return;

  ctx.fillStyle = th.piece[6] ?? th.text;
  ctx.fillRect(g.wellX - 4, g.wellY + (VISIBLE_ROWS - lines) * g.cell, 3, lines * g.cell);
}

function drawLockedCells(
  ctx: CanvasRenderingContext2D, s: Snapshot, th: Theme, g: Geometry,
): void {
  const size = g.cell - CELL_GAP;
  for (let y = 0; y < VISIBLE_ROWS; y++) {
    const bits = s.rows[y] ?? 0;
    if (bits === 0) continue;
    for (let x = 0; x < BOARD_COLS; x++) {
      if (((bits >> x) & 1) === 0) continue;
      ctx.fillStyle = pieceFill(th, s.cellPiece[y * BOARD_COLS + x] ?? CELL_EMPTY, th.cell);
      ctx.fillRect(cellLeft(g, x), cellTop(g, y), size, size);
    }
  }
}

function drawGhost(
  ctx: CanvasRenderingContext2D, s: Snapshot, th: Theme, g: Geometry,
): void {
  if (s.activePiece < 0) return;
  const cells = getPieceCells(s.activePiece, s.activeRot);

  ctx.strokeStyle = pieceFill(th, s.activePiece, th.cellGhost);
  ctx.lineWidth = 1;
  const size = g.cell - CELL_GAP;
  for (let i = 0; i < cells.length; i += 2) {
    const x = s.activeX + (cells[i] ?? 0);
    const y = s.ghostY + (cells[i + 1] ?? 0);
    if (y < 0 || y >= VISIBLE_ROWS) continue;
    ctx.strokeRect(cellLeft(g, x) + 0.5, cellTop(g, y) + 0.5, size, size);
  }
}

function nowMs(): number {
  return typeof performance !== 'undefined' ? performance.now() : Date.now();
}

function drawActivePiece(
  ctx: CanvasRenderingContext2D, s: Snapshot, th: Theme, g: Geometry,
): void {
  if (s.activePiece < 0) return;
  const cells = getPieceCells(s.activePiece, s.activeRot);
  const size = g.cell - CELL_GAP;
  ctx.fillStyle = pieceFill(th, s.activePiece, th.cellActive);
  for (let i = 0; i < cells.length; i += 2) {
    const x = s.activeX + (cells[i] ?? 0);
    const y = s.activeY + (cells[i + 1] ?? 0);

    if (y < 0 || y >= VISIBLE_ROWS) continue;
    ctx.fillRect(cellLeft(g, x), cellTop(g, y), size, size);
  }
}

function drawMiniPiece(
  ctx: CanvasRenderingContext2D, th: Theme, piece: number, px: number, py: number, unit: number,
): void {
  if (piece < 0) return;
  ctx.fillStyle = pieceFill(th, piece, th.cell);
  const cells = getPieceCells(piece, 0);
  let top = 0;
  for (let i = 1; i < cells.length; i += 2) top = Math.max(top, cells[i] ?? 0);
  for (let i = 0; i < cells.length; i += 2) {
    const dx = cells[i] ?? 0;
    const dy = (cells[i + 1] ?? 0) + 2 - top;
    ctx.fillRect(px + dx * unit, py - dy * unit, unit - CELL_GAP, unit - CELL_GAP);
  }
}

function drawSide(
  ctx: CanvasRenderingContext2D, s: Snapshot, th: Theme, g: Geometry,
): void {
  const unit = Math.max(2, Math.round(g.cell * 0.55));
  drawMiniPiece(ctx, th, s.holdPiece, g.holdX, g.wellY + unit * 2, unit);
  for (let i = 0; i < s.queue.length; i++) {
    drawMiniPiece(ctx, th, s.queue[i] ?? -1, g.sideX + unit, g.wellY + unit * (6 + i * 3), unit);
  }
}

function drawHud(
  ctx: CanvasRenderingContext2D, s: Snapshot, th: Theme, g: Geometry,
): void {
  const size = Math.max(9, Math.round(g.cell * 0.42));
  ctx.font = `${size}px ${FONT_STACK}`;
  ctx.fillStyle = th.textDim;
  ctx.textAlign = g.calloutAlign === 'right' ? 'right' : 'left';
  ctx.textBaseline = 'alphabetic';
  const line = `${s.pps.toFixed(1)} PPS   ${s.piecesPlaced} PIECES   ` +
    `${s.linesCleared} LINES   ${s.attackSent} ATK   B2B ${s.b2bCount}`;
  ctx.fillText(line, g.calloutAlign === 'right' ? g.wellX + g.wellW : g.wellX, g.hudY);
}

interface Callout {
  text: string;
  born: number;
}

function readAccent(el: HTMLElement): string {
  return getComputedStyle(el).getPropertyValue('--bot-accent').trim() || FALLBACK;
}

function calloutText(type: number, param: number, piece: number): string | null {
  const spin = PieceLetter[piece] ?? 'T';
  switch (type) {
    case 2: return 'TETRIS';
    case 3: return `${spin}-SPIN MINI`;
    case 4: return `${spin}-SPIN SINGLE`;
    case 5: return `${spin}-SPIN DOUBLE`;
    case 6: return `${spin}-SPIN TRIPLE`;
    case 7: return `BACK-TO-BACK ×${param}`;
    case 8: return param > 0 ? `SURGE +${param}` : null;
    case 9: return 'PERFECT CLEAR';
    case 10: return 'TOP OUT';
    default: return null;
  }
}

function collectCallouts(queue: Callout[], s: Snapshot, t: number): void {
  let piece = -1;
  for (const ev of s.events) {
    if (ev.type === 0) piece = ev.param;
    const text = calloutText(ev.type, ev.param, piece);
    if (text === null) continue;
    queue.push({ text, born: t });
    if (queue.length > MAX_CALLOUTS) queue.shift();
  }
}

function drawCallouts(
  ctx: CanvasRenderingContext2D, queue: Callout[], g: Geometry, host: HTMLElement, t: number,
): void {

  for (let i = queue.length - 1; i >= 0; i--) {
    const c = queue[i];
    if (c === undefined || t - c.born >= CALLOUT_LIFE_MS) queue.splice(i, 1);
  }
  if (queue.length === 0) return;

  ctx.fillStyle = readAccent(host);
  ctx.textAlign = g.calloutAlign;
  ctx.textBaseline = 'middle';
  const base = Math.max(10, Math.round(g.cell * 0.5));
  for (let i = queue.length - 1; i >= 0; i--) {
    const c = queue[i];
    if (c === undefined) continue;
    const age = t - c.born;
    const pop = Math.min(1, age / CALLOUT_POP_MS);
    const eased = 1 - (1 - pop) * (1 - pop);
    const scale = CALLOUT_SCALE_FROM + (1 - CALLOUT_SCALE_FROM) * eased;
    const life = age / CALLOUT_LIFE_MS;
    ctx.globalAlpha = 1 - life * life;
    ctx.font = `${Math.round(base * scale)}px ${FONT_STACK}`;
    ctx.fillText(c.text, g.calloutX, g.calloutY + i * base * 1.6);
  }
  ctx.globalAlpha = 1;
}

export function createRenderer(opts: RendererOptions): Renderer {
  const canvas = opts.canvas;
  const ctx = canvas.getContext('2d');
  if (ctx === null) throw new Error('createRenderer: canvas has no 2d context');

  let geo = computeGeometry(canvas, opts.layout, opts.chrome);
  let observer: ResizeObserver | null = null;

  const callouts: Callout[] = [];

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
    const t = nowMs();
    ctx.setTransform(g.dpr, 0, 0, g.dpr, 0, 0);
    ctx.fillStyle = th.bg;
    ctx.fillRect(0, 0, g.w, g.h);
    drawWell(ctx, th, g);
    drawPendingGarbage(ctx, s, th, g);
    drawLockedCells(ctx, s, th, g);
    drawGhost(ctx, s, th, g);
    drawActivePiece(ctx, s, th, g);
    if (opts.chrome === 'full') {
      drawSide(ctx, s, th, g);
      drawHud(ctx, s, th, g);
    }
    if (opts.chrome !== 'none') {
      collectCallouts(callouts, s, t);
      drawCallouts(ctx, callouts, g, canvas, t);
    }
  };

  const destroy = (): void => {
    if (observer !== null) {
      observer.disconnect();
      observer = null;
    }
  };

  return { draw, resize, destroy };
}
