import { useEffect } from 'react';

import { getBackendResourceAddress } from '../juce/index.js';

/**
 * The instrument's background artwork.
 *
 * The image is embedded in the plugin binary and served by
 * WebUIResourceProvider like any other asset, so there is no file IO at
 * runtime and nothing to go missing on a customer's machine.
 *
 * A build with no artwork is not a broken build. The slot resolves to a
 * placeholder - a 1x1 image, or an empty file from the CMake stub - and this
 * module detects either and leaves the CSS variable at `none`, so the app
 * draws its procedural gradient instead. That is a finished look on its own,
 * which matters because the artwork is commissioned separately from the code
 * and the plugin has to be shippable in between.
 */

const ARTWORK_PATH = 'assets/backdrop.webp';

/** Anything this small is the placeholder, not a picture. */
const MINIMUM_USEFUL_DIMENSION = 8;

/**
 * Resolves the artwork and publishes it to CSS as `--gn-artwork`.
 *
 * Runs once. The image is decoded by the browser either way, so probing it
 * costs nothing beyond the load the CSS layer would have done anyway.
 */
export function useArtwork(): void {
  useEffect(() => {
    // getBackendResourceAddress rewrites a relative path onto the origin the
    // plugin's resource provider serves; in a plain browser it returns the
    // path unchanged, so the same call works in the preview.
    const url = getBackendResourceAddress(ARTWORK_PATH);
    const image = new Image();

    let cancelled = false;

    image.onload = () => {
      if (cancelled) return;

      const isPlaceholder =
        image.naturalWidth < MINIMUM_USEFUL_DIMENSION ||
        image.naturalHeight < MINIMUM_USEFUL_DIMENSION;

      if (isPlaceholder) return;

      // image.src, NOT the relative path.
      //
      // A relative url() inside a CSS custom property is resolved against the
      // STYLESHEET, not the document - and the stylesheet is at
      // /assets/index.css, so "assets/backdrop.png" became
      // /assets/assets/backdrop.png and 404'd. The probe above still
      // succeeded, because an Image() src resolves against the document, so
      // the flag said the artwork had loaded while nothing was painting.
      // Reading it back off the element gives the absolute URL the browser
      // resolved, which is correct from anywhere.
      document.documentElement.style.setProperty('--gn-artwork', `url("${image.src}")`);
      document.documentElement.dataset.artwork = 'true';
    };

    // A missing or unreadable image is not an error worth surfacing: the
    // gradient is the fallback and the user sees a finished UI either way.
    image.onerror = () => {};

    image.src = url;

    return () => {
      cancelled = true;
    };
  }, []);
}
