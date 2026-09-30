import { defineConfig } from 'vite';

// One JS and one CSS file, no hashing: scripts/inline.mjs folds both into
// dist/gnarl-ui.html, a single file the plugin can embed and a phone can open.
export default defineConfig({
  base: './',
  // ASCII output (non-ASCII escaped): the plugin's Linux web view decoded the
  // UTF-8 page as windows-1252 whatever its meta tag and Content-Type said.
  // An ASCII page reads the same under every decoder. inline.mjs checks.
  esbuild: { charset: 'ascii' },
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
