import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

/**
 * The bundle is embedded into the plugin binary as JUCE BinaryData, so output
 * filenames must be STABLE and UNHASHED — the C++ resource provider resolves
 * them by name at compile time and cannot follow a content hash.
 *
 * Keep this list in sync with GNARL_UI_FILES in cmake/WebUI.cmake.
 */
export default defineConfig({
  plugins: [react()],
  base: './',
  build: {
    outDir: 'dist',
    // One chunk: the C++ resource provider serves a fixed file list, so code
    // splitting would produce files nothing can resolve.
    codeSplitting: false,
    emptyOutDir: true,
    assetsInlineLimit: 0,
    // No sourcemaps in the shipped binary; they would double its size.
    sourcemap: process.env.NODE_ENV !== 'production',
    target: 'es2022',
    rollupOptions: {
      output: {
        entryFileNames: 'assets/index.js',
        chunkFileNames: 'assets/index.js',
        assetFileNames: 'assets/index.[ext]',
        manualChunks: undefined,
      },
    },
  },
  server: {
    port: 5173,
    strictPort: true,
  },
});
