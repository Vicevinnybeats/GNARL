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
