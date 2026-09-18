# GNARL marketing site — brief

**Status: not started, deliberately.** Parked at the client's request until the
plugin itself is further along. This file exists so the brief is not lost and
so nobody has to reconstruct it from a chat log.

## What was asked for

A site to **sell and distribute** GNARL:

- Three.js, with a 3D model of the plugin as the centrepiece.
- GSAP for the animation.
- **Sticky-scroll storytelling**: the plugin is pinned while the page scrolls,
  and the scroll position drives the animation — a zoom into the plugin as the
  user scrolls down, revealing sections as it goes.
- A download / purchase path.

## Notes for whoever builds it

These are constraints the brief implies rather than extra scope, and they are
worth writing down now because they are cheap to design for and expensive to
retrofit.

- **The 3D model is the plugin's own UI.** GNARL's interface is already an HTML
  page (`ui/`), so the panel art for the model can be rendered from the real UI
  rather than drawn separately — which keeps the site honest as the plugin
  changes, and means the marketing shots cannot drift from the product.
  `tools/screenshot_ui.mjs` already renders the UI at the design size.
- **Scroll-driven animation must degrade.** A scrolljacked page that does not
  respond to a trackpad flick, a keyboard PageDown, or `prefers-reduced-motion`
  is a page a chunk of visitors cannot use. The content has to be reachable
  with the 3D layer switched off entirely.
- **Budget the model.** A producer evaluating a synth is often on a laptop with
  a DAW already open. A heavy WebGL scene that stalls the page costs a sale
  more directly than a plainer one does.
- **The download is the point.** Whatever the scroll sequence does, the buy and
  download actions should be reachable from the first screen without scrolling
  through the whole story.
- The site is a separate deployment from `backend/` (Phase 7), but it shares
  the licensing and checkout flow, so build it after that exists rather than
  stubbing a second one.

## Relationship to the phase plan

Slots after Phase 7 (backend, licensing, subscription), because the purchase
and download paths are the site's reason to exist and they live there.
