import { defineConfig } from 'vite';

export default defineConfig({
  base: './',
  build: {
    // The scene and the page are one bundle; splitting a single-page site
    // buys a second request, not a faster first paint.
    target: 'es2022',
  },
});
