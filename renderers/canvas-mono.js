import { PieceLetter } from '../js/types.js';
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
const FONT_STACK = 'ui-monospace, SFMono-Regular, Menlo, monospace';
/** cellPiece entry for a cell nothing has locked into (bindings/snapshot.h). */
const CELL_EMPTY = 255;
/**
 * Piece order is the core's: I J L O S T Z. Read from custom properties like every
 * other paint in this file, so a host restyles the whole board without touching TS.
 */
const PIECE_VARS = [
    '--bot-piece-i', '--bot-piece-j', '--bot-piece-l', '--bot-piece-o',
    '--bot-piece-s', '--bot-piece-t', '--bot-piece-z',
];
/** PRD section 7.3: brief scale-in, fade over ~800ms. */
const CALLOUT_LIFE_MS = 800;
const CALLOUT_POP_MS = 120;
const CALLOUT_SCALE_FROM = 0.72;
const MAX_CALLOUTS = 4;
/**
 * Not a color. `currentColor` resolves to the host element's own `color`, and
 * exists solely so a host that forgot to set the custom properties renders
 * something instead of `undefined`. PRD section 7.1: ship no hardcoded colors.
 */
const FALLBACK = 'currentColor';
/**
 * The seven non-accent properties. The accent is deliberately NOT read here: it
 * is fetched by exactly one function and spent in exactly one place, the callout
 * pass at the bottom of this file. tests/renderer_discipline.mjs enforces that by
 * counting occurrences, so do not name the accent property anywhere else - not
 * even in a comment.
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
        piece: PIECE_VARS.map(v),
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
/**
 * A piece's paint. `fallback` is what an empty or unknown cell uses, which is how
 * --bot-cell / --bot-cell-active / --bot-cell-ghost stay meaningful now that the
 * board is coloured per piece: they are the neutral default for each context.
 */
function pieceFill(th, piece, fallback) {
    return (piece >= 0 && piece < 7 ? th.piece[piece] : undefined) ?? fallback;
}
const cellLeft = (g, x) => g.wellX + x * g.cell;
const cellTop = (g, y) => g.wellY + (VISIBLE_ROWS - 1 - y) * g.cell;
function drawWell(ctx, th, g) {
    ctx.strokeStyle = th.grid;
    ctx.lineWidth = 1;
    ctx.strokeRect(g.wellX + 0.5, g.wellY + 0.5, g.wellW, g.wellH);
}
function drawLockedCells(ctx, s, th, g) {
    const size = g.cell - CELL_GAP;
    for (let y = 0; y < VISIBLE_ROWS; y++) {
        const bits = s.rows[y] ?? 0;
        if (bits === 0)
            continue;
        for (let x = 0; x < BOARD_COLS; x++) {
            if (((bits >> x) & 1) === 0)
                continue;
            ctx.fillStyle = pieceFill(th, s.cellPiece[y * BOARD_COLS + x] ?? CELL_EMPTY, th.cell);
            ctx.fillRect(cellLeft(g, x), cellTop(g, y), size, size);
        }
    }
}
function drawGhost(ctx, s, th, g) {
    if (s.activePiece < 0)
        return;
    const cells = getPieceCells(s.activePiece, s.activeRot);
    // Outlined in the piece's own colour, not a flat neutral.
    ctx.strokeStyle = pieceFill(th, s.activePiece, th.cellGhost);
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
function nowMs() {
    return typeof performance !== 'undefined' ? performance.now() : Date.now();
}
/**
 * DISCRETE, cell-snapped, no interpolation - jstris / TETR.IO handling rather than
 * an eased slide. The core already advances the piece one path step at a time along
 * the route the BFS found, so the piece still visibly walks its route and performs
 * its kicks; it just does it on cell boundaries instead of being smoothed between
 * them. There is deliberately no per-frame animation state: what the core reports is
 * exactly what gets painted, which is also why the position is testable.
 */
function drawActivePiece(ctx, s, th, g) {
    if (s.activePiece < 0)
        return;
    const cells = getPieceCells(s.activePiece, s.activeRot);
    const size = g.cell - CELL_GAP;
    ctx.fillStyle = pieceFill(th, s.activePiece, th.cellActive);
    for (let i = 0; i < cells.length; i += 2) {
        const x = s.activeX + (cells[i] ?? 0);
        const y = s.activeY + (cells[i + 1] ?? 0);
        // The piece spawns at row 21, above the field. Without this it paints outside
        // the well's top border. tests/renderer_board.mjs pins it.
        if (y < 0 || y >= VISIBLE_ROWS)
            continue;
        ctx.fillRect(cellLeft(g, x), cellTop(g, y), size, size);
    }
}
function drawMiniPiece(ctx, th, piece, px, py, unit) {
    if (piece < 0)
        return;
    ctx.fillStyle = pieceFill(th, piece, th.cell);
    const cells = getPieceCells(piece, 0);
    let top = 0;
    for (let i = 1; i < cells.length; i += 2)
        top = Math.max(top, cells[i] ?? 0);
    for (let i = 0; i < cells.length; i += 2) {
        const dx = cells[i] ?? 0;
        const dy = (cells[i + 1] ?? 0) + 2 - top; // top-align: box rows differ per piece
        ctx.fillRect(px + dx * unit, py - dy * unit, unit - CELL_GAP, unit - CELL_GAP);
    }
}
function drawSide(ctx, s, th, g) {
    const unit = Math.max(2, Math.round(g.cell * 0.55));
    drawMiniPiece(ctx, th, s.holdPiece, g.sideX + unit, g.wellY + unit * 2, unit);
    for (let i = 0; i < s.queue.length; i++) {
        drawMiniPiece(ctx, th, s.queue[i] ?? -1, g.sideX + unit, g.wellY + unit * (6 + i * 3), unit);
    }
}
function drawHud(ctx, s, th, g) {
    const size = Math.max(9, Math.round(g.cell * 0.42));
    ctx.font = `${size}px ${FONT_STACK}`;
    ctx.fillStyle = th.textDim;
    ctx.textAlign = g.calloutAlign === 'right' ? 'right' : 'left';
    ctx.textBaseline = 'alphabetic';
    const line = `${s.pps.toFixed(1)} PPS   ${s.piecesPlaced} PIECES   ` +
        `${s.linesCleared} LINES   ${s.attackSent} ATK   B2B ${s.b2bCount}`;
    ctx.fillText(line, g.calloutAlign === 'right' ? g.wellX + g.wellW : g.wellX, g.hudY);
}
/**
 * DISCIPLINE (PRD section 7.1): the accent property is read here and nowhere
 * else, and this function is called from drawCallouts() and nowhere else. A
 * single accent that occurs nowhere else lands disproportionately hard; do not
 * spend it on the active piece, the HUD, or the well outline.
 */
function readAccent(el) {
    return getComputedStyle(el).getPropertyValue('--bot-accent').trim() || FALLBACK;
}
function calloutText(type, param, piece) {
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
        default: return null; // PIECE_LOCK, LINE_CLEAR are not shouted about
    }
}
function collectCallouts(queue, s, t) {
    let piece = -1;
    for (const ev of s.events) {
        if (ev.type === 0)
            piece = ev.param;
        const text = calloutText(ev.type, ev.param, piece);
        if (text === null)
            continue;
        queue.push({ text, born: t });
        if (queue.length > MAX_CALLOUTS)
            queue.shift();
    }
}
function drawCallouts(ctx, queue, g, host, t) {
    // Retire expired entries BEFORE touching any paint state. Setting the accent
    // fill on a frame that then draws nothing leaves it as the context's residual
    // fillStyle, and the next frame's first op inherits it - which reads as the
    // accent escaping the callouts even though no accent pixel was ever painted.
    for (let i = queue.length - 1; i >= 0; i--) {
        const c = queue[i];
        if (c === undefined || t - c.born >= CALLOUT_LIFE_MS)
            queue.splice(i, 1);
    }
    if (queue.length === 0)
        return;
    ctx.fillStyle = readAccent(host);
    ctx.textAlign = g.calloutAlign;
    ctx.textBaseline = 'middle';
    const base = Math.max(10, Math.round(g.cell * 0.5));
    for (let i = queue.length - 1; i >= 0; i--) {
        const c = queue[i];
        if (c === undefined)
            continue;
        const age = t - c.born;
        const pop = Math.min(1, age / CALLOUT_POP_MS);
        const eased = 1 - (1 - pop) * (1 - pop); // easeOutQuad
        const scale = CALLOUT_SCALE_FROM + (1 - CALLOUT_SCALE_FROM) * eased;
        const life = age / CALLOUT_LIFE_MS;
        ctx.globalAlpha = 1 - life * life;
        ctx.font = `${Math.round(base * scale)}px ${FONT_STACK}`;
        ctx.fillText(c.text, g.calloutX, g.calloutY + i * base * 1.6);
    }
    ctx.globalAlpha = 1;
}
export function createRenderer(opts) {
    const canvas = opts.canvas;
    const ctx = canvas.getContext('2d');
    if (ctx === null)
        throw new Error('createRenderer: canvas has no 2d context');
    let geo = computeGeometry(canvas, opts.layout, opts.chrome);
    let observer = null;
    const callouts = [];
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
        const t = nowMs();
        ctx.setTransform(g.dpr, 0, 0, g.dpr, 0, 0);
        ctx.fillStyle = th.bg;
        ctx.fillRect(0, 0, g.w, g.h);
        drawWell(ctx, th, g);
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
    const destroy = () => {
        if (observer !== null) {
            observer.disconnect();
            observer = null;
        }
    };
    return { draw, resize, destroy };
}
