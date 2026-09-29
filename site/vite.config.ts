import { resolve } from 'node:path';

import { defineConfig } from 'vite';

export default defineConfig({
  base: './',
  build: {
    // The landing page is one page and one bundle - splitting it buys a
    // second request, not a faster first paint.
    //
    // CHECKOUT IS A SEPARATE ENTRY, deliberately. It shares the tokens and
    // the type and none of the machinery: no three.js, no GSAP, no scroll
    // journey. Somebody on that page has already decided, and a 700 KB WebGL
    // scene behind a payment form is both slower and stranger than a form.
    rollupOptions: {
      input: {
        index: resolve(__dirname, 'index.html'),
        checkout: resolve(__dirname, 'checkout.html'),
      },
    },
    target: 'es2022',
  },
});
