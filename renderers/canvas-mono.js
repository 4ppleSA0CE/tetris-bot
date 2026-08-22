import { getPieceCells } from '../js/layout.js';
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
/**
 * The seven non-accent properties. --bot-accent is deliberately absent: it is
 * read only by readAccent(), only from drawCallouts().
 */
function readTheme(el) {
    const cs = getComputedStyle(el);
    const v = (name) => cs.getPropertyValue(name).trim() || FALLBACK;
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
function computeGeometry(canvas, layout, chrome) {
    const rect = canvas.getBoundingClientRect();
    const w = Math.max(1, Math.round(rect.width || canvas.width || 300));
    const h = Math.max(1, Math.round(rect.height || canvas.height || 660));
    const dpr = typeof devicePixelRatio === 'number' && devicePixelRatio > 0 ? devicePixelRatio : 1;
    const hudH = chrome === 'full' ? HUD_HEIGHT : 0;
    const sideFrac = chrome === 'none' ? 0 : SIDE_FRACTION;
    // sidebar: PRD section 7.5 fixes cell size at viewport height / 22.
    const cell = layout === 'sidebar'
        ? Math.max(2, Math.floor(h / 22))
        : Math.max(2, Math.floor(Math.min((h - hudH - 8) / VISIBLE_ROWS, ((w - 8) * (1 - sideFrac)) / BOARD_COLS)));
    const wellW = cell * BOARD_COLS;
    const wellH = cell * VISIBLE_ROWS;
    let wellX;
    let wellY;
    let sideX;
    let calloutX;
    let calloutAlign;
    if (layout === 'sidebar') {
        wellX = w - wellW - 4;
        wellY = Math.round((h - wellH) / 2);
        sideX = wellX - cell * SIDE_CELLS;
        calloutX = wellX - cell * 0.75;
        calloutAlign = 'right';
    }
    else {
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
const cellLeft = (g, x) => g.wellX + x * g.cell;
const cellTop = (g, y) => g.wellY + (VISIBLE_ROWS - 1 - y) * g.cell;
function drawWell(ctx, th, g) {
    ctx.strokeStyle = th.grid;
    ctx.lineWidth = 1;
    ctx.strokeRect(g.wellX + 0.5, g.wellY + 0.5, g.wellW, g.wellH);
}
function drawLockedCells(ctx, s, th, g) {
    ctx.fillStyle = th.cell;
    const size = g.cell - CELL_GAP;
    for (let y = 0; y < VISIBLE_ROWS; y++) {
        const bits = s.rows[y] ?? 0;
        if (bits === 0)
            continue;
        for (let x = 0; x < BOARD_COLS; x++) {
            if ((bits >> x) & 1)
                ctx.fillRect(cellLeft(g, x), cellTop(g, y), size, size);
        }
    }
}
function drawGhost(ctx, s, th, g) {
    if (s.activePiece < 0)
        return;
    const cells = getPieceCells(s.activePiece, s.activeRot);
    ctx.strokeStyle = th.cellGhost;
    ctx.lineWidth = 1;
    const size = g.cell - CELL_GAP;
    for (let i = 0; i < cells.length; i += 2) {
        const x = s.activeX + (cells[i] ?? 0);
        const y = s.ghostY + (cells[i + 1] ?? 0);
        if (y < 0 || y >= VISIBLE_ROWS)
            continue;
        ctx.strokeRect(cellLeft(g, x) + 0.5, cellTop(g, y) + 0.5, size, size);
    }
}
function drawActivePiece(ctx, s, th, g) {
    if (s.activePiece < 0)
        return;
    const cells = getPieceCells(s.activePiece, s.activeRot);
    ctx.fillStyle = th.cellActive;
    const size = g.cell - CELL_GAP;
    for (let i = 0; i < cells.length; i += 2) {
        const x = s.activeX + (cells[i] ?? 0);
        const y = s.activeY + (cells[i + 1] ?? 0);
        if (y < 0 || y >= VISIBLE_ROWS)
            continue;
        ctx.fillRect(cellLeft(g, x), cellTop(g, y), size, size);
    }
}
export function createRenderer(opts) {
    const canvas = opts.canvas;
    const ctx = canvas.getContext('2d');
    if (ctx === null)
        throw new Error('createRenderer: canvas has no 2d context');
    let geo = computeGeometry(canvas, opts.layout, opts.chrome);
    let observer = null;
    const resize = () => {
        geo = computeGeometry(canvas, opts.layout, opts.chrome);
        canvas.width = Math.round(geo.w * geo.dpr);
        canvas.height = Math.round(geo.h * geo.dpr);
    };
    if (typeof ResizeObserver !== 'undefined') {
        observer = new ResizeObserver(() => resize());
        observer.observe(canvas);
    }
    const draw = (s) => {
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
    const destroy = () => {
        if (observer !== null) {
            observer.disconnect();
            observer = null;
        }
    };
    return { draw, resize, destroy };
}
