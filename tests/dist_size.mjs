import assert from 'node:assert/strict';
import { readdirSync, statSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { join } from 'node:path';

const LIMIT = 500 * 1024;
const dir = fileURLToPath(new URL('../dist/', import.meta.url));

let total = 0;
const rows = [];
for (const name of readdirSync(dir).sort()) {
  const st = statSync(join(dir, name));
  if (!st.isFile()) continue;
  total += st.size;
  rows.push(`${name.padEnd(24)}${String(st.size).padStart(9)}`);
}
console.log(rows.join('\n'));
console.log(`${'TOTAL'.padEnd(24)}${String(total).padStart(9)}   limit ${LIMIT}`);
assert.ok(rows.length >= 2, 'dist/ is missing bot.js or bot.d.ts');
assert.ok(total < LIMIT, `/dist is ${total} bytes, over the ${LIMIT}-byte budget`);
console.log(`dist size OK: ${total} bytes, ${((total / LIMIT) * 100).toFixed(1)}% of budget`);
