import { defineConfig } from 'vite';
import { fileURLToPath } from 'node:url';

const here = fileURLToPath(new URL('.', import.meta.url));
const repoRoot = fileURLToPath(new URL('..', import.meta.url));

export default defineConfig({
  root: here,
  base: './',
  // es2022 for top-level await and class private fields, both of which the
  // wrapper and the Emscripten glue rely on.
  esbuild: { target: 'es2022' },
  build: {
    target: 'es2022',
    outDir: 'dist',        // demo/dist — gitignored by the rule added below
    emptyOutDir: true,
    rollupOptions: {
      input: {
        main: fileURLToPath(new URL('./index.html', import.meta.url)),
        embed: fileURLToPath(new URL('./embed.html', import.meta.url)),
      },
    },
  },
  // The demo imports ../js and ../renderers, which live outside the Vite root.
  server: { fs: { allow: [repoRoot] } },
});
