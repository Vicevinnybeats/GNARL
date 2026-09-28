import { resolve } from 'node:path';
import { defineConfig } from 'vite';

/*  FIVE PAGES, FIVE ENTRY POINTS. Real .html files rather than a router:
    the site is five documents with almost no shared state, so a router
    would buy a smoother transition and cost the thing a static page gets
    free - a URL that works without JavaScript, a crawler that can read it,
    and a back button that is the browser's rather than ours. */
export default defineConfig({
  base: './',
  build: {
    target: 'es2022',
    rollupOptions: {
      input: {
        home: resolve(__dirname, 'index.html'),
        engine: resolve(__dirname, 'engine.html'),
        presets: resolve(__dirname, 'presets.html'),
        fx: resolve(__dirname, 'fx.html'),
        download: resolve(__dirname, 'download.html'),
      },
    },
  },
});
