import { defineConfig } from 'vite';

export default defineConfig({
  base: './',
  build: {
    // One page, one bundle. Splitting a single-page site buys a second
    // request, not a faster first paint.
    target: 'es2022',
  },
});
