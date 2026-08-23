// A recording Canvas 2D context, so the renderer can be tested in Node.
// Every drawing call is pushed to canvas.ops with the style state it ran under.

export function installDom() {
  globalThis.devicePixelRatio = 1;
  globalThis.getComputedStyle = (el) => ({
    getPropertyValue: (name) => (el.__vars ?? {})[name] ?? '',
  });
}

export function makeCanvas(width, height, vars) {
  const ops = [];
  const state = {
    fillStyle: '', strokeStyle: '', font: '', globalAlpha: 1,
    lineWidth: 1, textAlign: '', textBaseline: '',
  };
  const rec = (op, ...args) => {
    ops.push({
      op, args,
      fillStyle: state.fillStyle,
      strokeStyle: state.strokeStyle,
      globalAlpha: state.globalAlpha,
      font: state.font,
    });
  };

  const ctx = {
    get fillStyle() { return state.fillStyle; },
    set fillStyle(v) { state.fillStyle = v; },
    get strokeStyle() { return state.strokeStyle; },
    set strokeStyle(v) { state.strokeStyle = v; },
    get font() { return state.font; },
    set font(v) { state.font = v; },
    get globalAlpha() { return state.globalAlpha; },
    set globalAlpha(v) { state.globalAlpha = v; },
    get lineWidth() { return state.lineWidth; },
    set lineWidth(v) { state.lineWidth = v; },
    get textAlign() { return state.textAlign; },
    set textAlign(v) { state.textAlign = v; },
    get textBaseline() { return state.textBaseline; },
    set textBaseline(v) { state.textBaseline = v; },
    save: () => rec('save'),
    restore: () => rec('restore'),
    translate: (x, y) => rec('translate', x, y),
    rotate: (a) => rec('rotate', a),
    scale: (x, y) => rec('scale', x, y),
    setTransform: (...a) => rec('setTransform', ...a),
    clearRect: (...a) => rec('clearRect', ...a),
    fillRect: (...a) => rec('fillRect', ...a),
    strokeRect: (...a) => rec('strokeRect', ...a),
    beginPath: () => rec('beginPath'),
    rect: (...a) => rec('rect', ...a),
    stroke: () => rec('stroke'),
    fill: () => rec('fill'),
    fillText: (...a) => rec('fillText', ...a),
    measureText: (t) => ({ width: t.length * 6 }),
  };

  return {
    width, height, ops,
    __vars: vars,
    parentElement: null,
    getContext: () => ctx,
    getBoundingClientRect: () => ({
      width, height, left: 0, top: 0, right: width, bottom: height,
    }),
  };
}

/** The eight properties from PRD section 7.1, as distinguishable sentinels. */
export const TEST_VARS = {
  '--bot-bg': 'VAR_BG',
  '--bot-grid': 'VAR_GRID',
  '--bot-cell': 'VAR_CELL',
  '--bot-cell-active': 'VAR_CELL_ACTIVE',
  '--bot-cell-ghost': 'VAR_CELL_GHOST',
  '--bot-text': 'VAR_TEXT',
  '--bot-text-dim': 'VAR_TEXT_DIM',
  '--bot-accent': 'VAR_ACCENT',
  '--bot-piece-i': 'VAR_PIECE_I',
  '--bot-piece-j': 'VAR_PIECE_J',
  '--bot-piece-l': 'VAR_PIECE_L',
  '--bot-piece-o': 'VAR_PIECE_O',
  '--bot-piece-s': 'VAR_PIECE_S',
  '--bot-piece-t': 'VAR_PIECE_T',
  '--bot-piece-z': 'VAR_PIECE_Z',
};

/** A hand-built Snapshot-shaped object; the renderer only reads, never mutates. */
export function fakeSnapshot(overrides = {}) {
  const rows = new Uint16Array(40);
  rows[0] = 0b0111111111;   // bottom row, one gap at column 9
  rows[1] = 0b0000000011;
  // Which piece locked into each cell, parallel to rows. Two distinct pieces so a
  // test can tell the board is painted per piece and not with one flat colour.
  const cellPiece = new Uint8Array(400).fill(255);
  for (let x = 0; x < 9; x++) cellPiece[0 * 10 + x] = 0;   // I
  for (let x = 0; x < 2; x++) cellPiece[1 * 10 + x] = 6;   // Z
  return {
    frame: 1, rows, cellPiece,
    activePiece: 5, activeRot: 0, activeX: 4, activeY: 12, ghostY: 3,
    pendingSpin: 0, pathProgress: 0, holdPiece: 3,
    queue: Int8Array.from([0, 1, 2, 4, 6]),
    events: [],
    piecesPlaced: 7, linesCleared: 2, attackSent: 5,
    b2bCount: 1, comboCount: 0, pps: 4.9, state: 1,
    ...overrides,
  };
}
