# GNARL background artwork — brief and prompts

The **Dream** theme draws a full-bleed background image behind the whole
interface. This file says what that image has to be, and gives prompts for
generating one.

## The workflow

```bash
cp your-artwork.png ui/public/assets/backdrop.png
cmake --build build --parallel      # rebuilds the UI and re-embeds it
```

That is the whole thing. The image is embedded in the plugin binary, so there
is no file IO at runtime and nothing to go missing on a customer's machine.

The slot currently holds a **1×1 transparent placeholder**. `ui/src/bridge/artwork.ts`
detects that and leaves the CSS variable unset, so the app falls back to its
procedural gradient — which is a finished look on its own. A build with no
artwork commissioned yet is shippable.

---

## Hard constraints

These are not style preferences; break one and the UI stops working.

| Constraint | Why |
|---|---|
| **2360 × 1440 px minimum**, 16:10 | Twice the 1180×720 design size. The window scales 70%–200%, and an upscaled backdrop looks like a mistake at 200%. |
| **No important detail anywhere** | The image is `background-size: cover` under the entire UI. Every part of it is cropped at some window size and covered by panels at every window size. It is a *texture and a mood*, not a picture anyone will look at. |
| **Dark, and evenly dark** | It sits under a scrim and behind 10px labels. A bright patch anywhere is a patch where the UI becomes unreadable. Keep it below roughly 25% luminance throughout; the theme supplies the brightness. |
| **Low contrast, low frequency** | Fine detail fights the controls and turns to noise once blurred. Think large soft forms, not intricate line work. |
| **No text, no logos, no letterforms** | They would be cropped into nonsense, and they compete with real labels. |
| **No recognisable faces or people** | Legal, see below. |
| **Violet → cyan** | It has to belong to the Dream palette: violet `#b06bff` body, cyan `#5fe3ff` accents, deep indigo `#12092a` ground. |
| **Edges must be quiet** | `cover` crops a different amount off each edge at each window size. Anything the eye would notice getting cut belongs away from the edges. |

## Legal — read before commissioning or generating

- **Do not reproduce the reference.** The Dopamine artwork the client showed as
  a direction is someone else's commissioned illustration. We take the *mood*
  from it — soft violet-to-cyan, dreamy, glowing — and nothing else. Matching a
  competitor's information architecture is fine; matching their artwork is not.
  See CLAUDE.md §9.
- **No faces.** The reference is built on illustrated faces. A generated face
  that resembles a real person is a likeness problem in a product we sell, and
  it is not a risk worth taking for a background nobody looks at directly.
- **AI-generated artwork has weak copyright protection** in the US and several
  other markets, and some generators' terms restrict commercial use. For a
  product we are selling, a generated backdrop is fine as a *placeholder* and
  for internal review; before release either commission the final art with a
  proper assignment of rights, or confirm the generator's licence allows
  commercial use and accept that we may not be able to stop someone copying it.
  Flag this to the client rather than deciding it quietly.

---

## Prompt — the main one

Aimed at Midjourney / Stable Diffusion / DALL·E. Adjust the trailing parameters
for whichever you use.

```
Abstract dreamy nebula texture for a dark music software background.
Deep indigo and midnight violet base, soft blooms of purple and magenta
light in the upper left falling away to deep blue-black, a cool cyan haze
in the upper right. Smooth volumetric gradients, gentle liquid smoke and
soft bokeh light, subtle film grain. Very dark, low contrast, evenly lit,
no bright hotspots. No subject, no focal point, no text, no people, no
faces, no objects — pure atmospheric colour field. Wide cinematic
composition, uniform density edge to edge. Ultra wide 2360x1440.
```

Negative prompt, where the tool takes one:

```
text, letters, watermark, logo, face, person, figure, hands, eyes, objects,
sharp detail, high contrast, bright highlights, white areas, busy pattern,
harsh edges, vignette, border, frame, centred subject
```

Parameters: `--ar 59:36 --style raw --stylize 150` (Midjourney), or 2360×1440
with a low CFG (4–6) elsewhere so it stays soft rather than crunchy.

## Prompt — variants worth trying

**Silk / liquid.** Softer and more organic; tends to give the most usable
evenness.

```
Abstract flowing silk in deep violet and indigo, lit from the upper left by
soft purple light with a faint cyan rim on the right. Slow liquid folds,
very soft focus, heavy shadow, low contrast, dark throughout. No subject,
no text, no faces. Seamless atmospheric texture, wide format 2360x1440.
```

**Aurora.** Closest to the reference's glow, but watch for bright bands — ask
for them dim and keep them out of the middle third.

```
Distant aurora over a black sky, deep violet and magenta curtains with a
cool cyan edge, extremely dim and diffuse, heavily blurred, no stars, no
horizon, no landscape. Dark abstract colour field, even density, no bright
areas, no text. Wide format 2360x1440.
```

**Frosted glass.** The safest for readability — almost pure gradient, which is
what the UI actually wants.

```
Frosted glass panel lit from behind by violet and cyan light, extreme soft
focus, smooth colour gradient from purple to deep indigo, very dark, very
low contrast, fine grain. Abstract, no subject, no text, no reflections of
people. Wide format 2360x1440.
```

## After generating

1. **Check it at size.** Drop it in, rebuild, and look at the UI at 70% and at
   200%. Both crops have to work.
2. **Darken it if in doubt.** The theme's scrim can be turned up
   (`--gn-artwork-scrim` in `ui/src/styles/tokens.css`) but the cheaper fix is
   a darker source image.
3. **Watch the file size.** It is embedded in the binary, so it is added to
   every install. Keep it under about 400 KB — a soft, low-contrast image
   compresses extremely well, so if it is bigger than that it probably has
   detail in it that the brief says it should not have.
4. **PNG is the slot's format** (`assets/backdrop.png`). To ship a different
   format, add it to `GNARL_UI_FILES` in `cmake/WebUI.cmake` and to
   `ARTWORK_PATH` in `ui/src/bridge/artwork.ts`; the resource provider already
   serves jpeg, webp and avif.
