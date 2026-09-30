import { defineConfig } from 'vite';

// One JS and one CSS file, no hashing: scripts/inline.mjs folds both into
// dist/gnarl-ui.html, a single file the plugin can embed and a phone can open.
export default defineConfig({
  base: './',
  build: {
    outDir: 'dist',
    assetsInlineLimit: 1_000_000,
    cssCodeSplit: false,
    modulePreload: false,
    rollupOptions: {
      output: {
        entryFileNames: 'app.js',
        assetFileNames: 'app.[ext]',
      },
    },
  },
});
