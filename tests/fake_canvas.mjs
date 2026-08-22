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
};

/** A hand-built Snapshot-shaped object; the renderer only reads, never mutates. */
export function fakeSnapshot(overrides = {}) {
  const rows = new Uint16Array(40);
  rows[0] = 0b0111111111;   // bottom row, one gap at column 9
  rows[1] = 0b0000000011;
  return {
    frame: 1, rows,
    activePiece: 5, activeRot: 0, activeX: 4, activeY: 12, ghostY: 3,
    pendingSpin: 0, pathProgress: 0, holdPiece: 3,
    queue: Int8Array.from([0, 1, 2, 4, 6]),
    events: [],
    piecesPlaced: 7, linesCleared: 2, attackSent: 5,
    b2bCount: 1, comboCount: 0, pps: 4.9, state: 1,
    ...overrides,
  };
}
