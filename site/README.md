# The GNARL site

Five pages, one universe. Three.js + GSAP, built by Vite to static files.

```bash
npm install
npm run dev            # localhost:5173

npm run build
(cd dist && python3 -m http.server 4174 --bind 127.0.0.1 &)
node ../tools/screenshot_site.mjs <output-directory>
```

| Page | What the scene is |
|---|---|
| `index.html` | Orbits around a bright core, opening into the oscillator's surface |
| `engine.html` | The harmonic surface, opening into modulation |
| `presets.html` | A double helix winding into the rack |
| `fx.html` | **A dissolve** — a solid object burning away into its own dust |
| `download.html` | The vortex resolving into the interface itself |

Four of the five are the *same* shader walking a different slice of one
continuous transformation, so arriving on ENGINE picks up where HOME left
off. `universe.ts` holds all five formations; a page differs only by the
`{ from, to }` it walks.

---

## Dropping in a 3D model

The FX page samples an object into particles. By default it builds a
**loudspeaker driver** in code (`createDriverGeometry`) — on theme, and with
no licence question, which matters for a page advertising something we sell.

To use your own model instead, put a `.glb` in `public/models/` and declare
it in `fx.html`:

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
frame jumps `k` × whatever distance a fast flick opened, which is what made
an earlier version appear to teleport. Everything eases through
`1 - exp(-rate * dt)`.

**Additive light sums.** The per-particle alpha that looks right for one
particle is flat white for ninety thousand. Both the figure and the dissolve's
dust were rebuilt after a screenshot showed a white disc with the copy
floating on it. The bloom *threshold* is the number that matters, not its
strength.

**The figure sits beside the words, never behind them**, and the offset is
measured along the camera's own right vector as a fraction of the frustum —
not in world x. The camera orbits, so world x becomes depth halfway round.

**Screenshot after any change.** `tools/screenshot_site.mjs` drives both
sizes to the top *and the foot* of all five pages; the foot is where the
dissolve has finished and where the interface resolves, and a picture of
only the top says nothing about either.
