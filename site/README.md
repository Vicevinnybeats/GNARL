# The GNARL site

**One page**, one continuous camera move. Three.js + GSAP, built by Vite to a
static bundle.

```bash
npm install
npm run dev            # localhost:5173

npm run build
(cd dist && python3 -m http.server 4174 --bind 127.0.0.1 &)
node ../tools/screenshot_site.mjs <output-directory>
```

It was briefly five pages. It is one now, because the thing being sold is a
continuous transformation and a page boundary is a cut: arriving on a new
document resets the scene, the camera and the reader's place in the idea all
at once. A one-pager's "next" is the scroll, so the forward-link chain went
with the pages — a button that does what the wheel already did.

## What the journey is

`src/journey.ts` is one scene. A `CatmullRomCurve3` runs ~350 units through
six control points; the camera **chases** a traveller along it, lagging
behind and easing, so the motion reads as flight rather than as a value being
set. Scroll drives the position along the curve and nothing else.

| Section | What the object is |
|---|---|
| top | Orbits around a bright core |
| engine | The oscillator's harmonic surface |
| presets | A double helix |
| fx | **A dissolve** — the form burning away into its own dust |
| download | The interface's own rectangle, resolving |

Those are five *formations* of one point set, not five objects: `FORMATIONS`
is a single GLSL block and the vertex shader morphs between two of them on a
`uT` the scroll supplies. That is what makes the transitions continuous —
there is no moment where one thing is swapped for another, because there is
only ever one thing.

The figure is `LineSegments`, not sprites. Line art is legible because the
strokes are thin and the gaps are empty; a dense field of soft sprites is
fog, and the first version was a white disc with the copy floating on it.

## The boot screen

`src/loader.ts` — a HUD dial that fills while the page loads, with the
wordmark inside it. Drawn **entirely in three.js**: no DOM text, no font
file, no image. Every glyph is a polyline in a small stroke font defined in
that file, and the percentage is seven-segment digits switched on and off
through a buffer attribute, so a readout that changes sixty times a second
never re-tessellates anything.

**The number is the real load.** It comes from the stages `boot.ts` reports
and from `THREE.DefaultLoadingManager`, and it never reaches 100 before the
page is genuinely ready — "ready" meaning a frame has actually been *drawn*,
not that the objects exist, because the first frame is where the shaders
compile and on a slow machine that is the longest pause of the whole load.
A bar on a timer is a lie, and on the one connection where it matters it is
confidently wrong.

**`journey.ts` is reached through a dynamic import**, which is what makes the
loader worth having: a static import puts the whole scene in the first chunk
and the browser parses every byte of it before a frame of the loader can be
drawn, so the screen that exists to cover the wait arrives at the end of it.
The build splits into a `journey` chunk because of that one `await import`.

**It dismisses on a click**, not on its own. The whole page is driven by
scroll, so an automatic dismissal drops the viewer into the opening shot
mid-gesture. A click is also the gesture a browser requires before a page may
make any sound. Enter and Space do the same thing — a loader you can only
leave with a mouse is one some people cannot leave.

Two things the screenshots caught that code review did not:

- **A clear colour is not a background.** `OutputPass` converts the frame to
  sRGB on the way out, so `setClearColor(0x05030e)` — chosen as a near-black —
  came back out a visible violet-grey, several stops brighter than the site it
  introduces. The backdrop is a CSS gradient on `#boot` instead, because CSS
  is not in that pipeline.
- **Everything was at the origin.** The bar ran straight through the dial's
  lower arc and the wordmark measured 0.735 across inside a ring 0.60 wide, so
  the L sat on the ring. It is a vertical stack now: dial, readout, bar,
  status. Same lesson as the plugin's tab layout, which learned it four times.

### Arriving on the page

Clicking through does not cut to a finished page. It starts one arrival that
the loader, the 3D scene and the DOM all take part in: the dial **rushes past
the camera** (cubic, so it accelerates out of frame rather than being politely
resized in place, with the dust sweeping wider than it for parallax), the
camera dollies in from 2.4× the framing distance, the figure's strokes fly in
from every side and converge, and the header, the hero's lines and the footer
slide in from the edges they belong to while the card they sit on fades
without moving.

**Each stroke flies in along its own line, on its own slice of the window**,
with a swirl that unwinds as it lands and extra brightness while it is still
travelling. Converging from all around rather than expanding out of a point is
what makes it read as being *built*; the per-stroke stagger is what stops it
popping into being on one frame.

**One clock drives all of it** — a clamped frame step in `boot.ts`, the same
step the loader's exit advances on. A CSS transition or a GSAP tween here
would be a second easing on a different clock, and it drifts out of step with
the 3D half on exactly the slow machine where the whole thing is most visible.
The build deliberately outlasts the loader: the dial is gone about a third of
the way through and the page spends the rest assembling behind it.

`isSettled()` waits on the figure's build as well as the dolly. The build
finishes later, so leaving it out let the screenshot tool photograph the page
with strokes still inbound and call it the composition.

Reduced motion gets the destination and not the journey **in the 3D half as
well as the CSS half** — opting out in the DOM while the strokes still fly in
from every side would honour the preference everywhere except the part that
actually moves.

### Making it smooth on a phone

The scene is a bloomed full-screen composite, so a phone runs out of **fill
rate** long before geometry. Three levers, in order of how much they matter:

- **Pixel ratio caps at 1.25** (not 1.5). A phone reports 3, so even 1.5 was
  rendering 2.25× the panel's pixels through a chain that touches each of them
  several times. This is the single largest lever.
- **The bloom runs at half resolution.** Its entire output is low frequency, so
  the mip chain built from a half-size target is very nearly the same picture
  for a quarter of the pixels touched.
- **MSAA off.** The bloom's own blur was buying most of what it was for.

`tools/screenshot_loader.mjs` shoots it loading, ready, four frames across the
arrival and where it lands, and throttles the connection through CDP, because
on a local server the state worth photographing lasts under a second.

Note that `tools/screenshot_site.mjs` now has to **click through the boot
screen** before it can scroll, or every picture it takes is of the loader.

---

## Dropping in a 3D model

The dissolve samples an object into particles. By default it builds a
**loudspeaker driver** in code (`createDriverGeometry`) — on theme, and with
no licence question, which matters for a page advertising something we sell.

To use your own model instead, put a `.glb` in `public/models/` and declare
it in `index.html`:

```html
<meta name="gnarl-model" content="./models/figure.glb" />
```

No tag, no request. (A hard-coded path that is usually absent means every
default build logs a 404, which trains you to ignore the console — where the
next real error is going to appear.)

### What makes a good model here

The sampler scatters points across the **triangles**, weighted by area, so
topology barely matters — but these do:

- **Under ~5 MB.** It is fetched before the page can draw.
- **A closed-ish surface.** Points land on faces; a model that is mostly
  open edges and loose planes samples as a cloud of scraps.
- **One clear silhouette.** It is seen at a distance, in one colour, through
  bloom. Detail below a few pixels is thrown away by the dissolve anyway.
- **`.glb`, not `.gltf` + folders.** One file, one request.

### Where to get them

Checked from this repo's network; the first two are reachable from CI.

| Source | Licence | Notes |
|---|---|---|
| [Khronos glTF-Sample-Assets](https://github.com/KhronosGroup/glTF-Sample-Assets) | mostly CC0 / CC-BY | Reference models, every one with its licence stated in the repo |
| [three.js `examples/models`](https://github.com/mrdoob/three.js/tree/dev/examples/models) | mixed — **check each** | LeePerrySmith (a head) is CC BY 3.0 and needs visible attribution |
| [Poly Haven](https://polyhaven.com/models) | CC0 | Best quality-per-effort; **blocked by this container's network policy**, download locally |
| [Sketchfab](https://sketchfab.com/search?features=downloadable&licenses=322a749bcfa841b29dff1e8a1bb74b0b) | filter to CC0 | Huge, but the licence filter is not optional |
| [Quaternius](https://quaternius.com/) | CC0 | Low-poly packs; the silhouettes read well at this distance |

**Theme-wise**, what suits a riddim synth: a speaker driver or horn, a
skull, a jaw or teeth, a helmet or gas mask, a monolith, an engine block, a
broken statue head. Avoid anything with fine surface detail — it disappears.

> **Licence discipline is the same as the plugin's** (CLAUDE.md §9): ship only
> what we made or hold a licence for. A downloaded mesh is an asset like a
> wavetable is. CC0 needs nothing; CC-BY needs the credit *visible on the
> page*, not buried in a repo.

---

## Things worth knowing before changing this

**Damping is a function of time, not frames.** `x += (target - x) * k` per
frame settles in half a second at 60 fps and two seconds at 15 — and each
frame jumps `k` × whatever distance a fast flick opened, so a fast scroll
shows four or five discrete positions and the page appears to cut between
them. That is exactly what "it teleports when I scroll fast" was. Everything
eases through `1 - exp(-rate * dt)`, which composes exactly. The render loop
measures its own delta and passes it down; nothing may read `0.075` per
frame again.

**Additive light sums.** The per-particle alpha that looks right for one
particle is flat white for ninety thousand. The dissolve's dust was rebuilt
twice, and the second cause was arithmetic rather than taste: the mote fade
window was 0.55 while the burn threshold stopped at 1.15, so the last motes
released only ever reached an age of 0.27 and sat there at near-full
brightness. The bloom **threshold** is the number that matters, not its
strength.

**The dissolve samples its noise in object space.** In world space the
pattern is nailed to the room, so rotating the form makes the burn crawl
across it like a searchlight. Surface and dust read the *same* field at the
*same* coordinates, from one shared GLSL string, so each mote lets go exactly
when the hole under it opens.

**The figure sits beside the words, never behind them**, and the offset is
measured along the camera's own **right vector** as a fraction of the
frustum — not in world x. The camera orbits; by the last section it is a
hundred degrees round the arc and world x has become depth.

**On a phone there is no fixed band to aim at, because the card scrolls.**
Aiming the camera up pitches the view and slides the world *down*. Raising
the object into the strip above the card works only at the top of the page.
What works is centred slightly high and scaled to about a quarter of the
frame, with the card passing over its lower part.

**Screenshot after any change.** `tools/screenshot_site.mjs` drives both
sizes to six fractions of the scroll — the interesting ones fall *between*
sections, where no element sits, which is why it scrolls by absolute offset
rather than into view.
