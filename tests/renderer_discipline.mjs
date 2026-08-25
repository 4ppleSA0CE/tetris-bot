import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const path = new URL('../renderers/canvas-mono.ts', import.meta.url);
const src = readFileSync(path, 'utf8');

const COLOR_LITERAL =
  /#[0-9a-fA-F]{3,8}\b|\brgba?\s*\(|\bhsla?\s*\(|\boklch\s*\(|\boklab\s*\(|\bcolor-mix\s*\(|\b(?:red|blue|green|white|black|yellow|orange|purple|pink|brown|gray|grey|cyan|magenta|silver|gold|teal|navy|lime|olive|maroon|aqua|fuchsia|indigo|violet|beige|coral|crimson|khaki|salmon|tan|turquoise|wheat)\b/gi;

const hits = [];
src.split('\n').forEach((line, i) => {
  const m = line.match(COLOR_LITERAL);
  if (m) hits.push(`  ${i + 1}: ${line.trim()}   <- ${[...new Set(m)].join(', ')}`);
});
assert.equal(hits.length, 0,
  `renderers/canvas-mono.ts contains color literals:\n${hits.join('\n')}`);

const accentProp = (src.match(/--bot-accent/g) ?? []).length;
assert.equal(accentProp, 1,
  `"--bot-accent" must appear exactly once (inside readAccent), found ${accentProp}`);

const accentCalls = (src.match(/readAccent\s*\(/g) ?? []).length;
assert.equal(accentCalls, 2,
  `expected 2 occurrences of "readAccent(" — 1 definition + 1 call site — found ${accentCalls}`);

const calloutFn = /^function drawCallouts\b[\s\S]*?^}/m.exec(src);
assert.ok(calloutFn, 'drawCallouts() not found, or its closing brace is not at column 0');
assert.ok(/readAccent\s*\(/.test(calloutFn[0]),
  'readAccent() is not called from drawCallouts() — the accent has leaked elsewhere');

for (const v of ['--bot-bg', '--bot-grid', '--bot-cell', '--bot-cell-active',
                 '--bot-cell-ghost', '--bot-text', '--bot-text-dim']) {
  assert.ok(src.includes(v), `renderer never reads ${v}`);
}

console.log('renderer discipline OK: 0 color literals, --bot-accent read once, only in drawCallouts');
