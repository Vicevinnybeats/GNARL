# GNARL — user manual

A wavetable synthesizer for riddim and dubstep sound design. VST3, AU and
Standalone, macOS and Windows.

This manual is written for someone who already makes this kind of music. It
explains what each control *does to the sound* and why it is built the way it
is, and it says plainly where a control behaves differently from the synth you
came from.

---

## Contents

1. [Installing](#installing)
2. [The four tabs](#the-four-tabs)
3. [OSC — oscillators, sub, noise, filters](#osc)
4. [MOD — LFOs, envelopes, the matrix, macros](#mod)
5. [FX — the rack](#fx)
6. [Presets](#presets)
7. [Settings](#settings)
8. [Making the sounds](#making-the-sounds)
9. [CPU](#cpu)
10. [Licensing](#licensing)
11. [Troubleshooting](#troubleshooting)

---

## Installing

A successful build installs the plugin into the system folders automatically,
and your DAW picks it up on the next rescan.

| Format | macOS | Windows |
|---|---|---|
| VST3 | `~/Library/Audio/Plug-Ins/VST3/GNARL.vst3` | `C:\Program Files\Common Files\VST3\GNARL.vst3` |
| AU | `~/Library/Audio/Plug-Ins/Components/GNARL.component` | — |
| Standalone | an app you can just open | an exe you can just open |

**The Standalone is the fastest way to hear it.** It needs no DAW: open it,
pick an audio device and a MIDI input, and play.

Building from source is in the repository's own README and in
[`docs/windows-setup.md`](windows-setup.md) — Windows additionally needs the
Microsoft.Web.WebView2 package, without which the interface cannot render at
all.

Linux builds (VST3 and Standalone) work and are used for CI, but are not a
supported release target.

---

## The four tabs

**OSC**, **MOD**, **FX**, **AI**. That is the whole navigation — there is no
nesting below it, on purpose. Density is a feature here: you can see the
oscillators, both filters and the routing at once, because comparing two knobs
you have to hover one at a time is not comparing them.

Knob values are **always visible**, dim at rest and bright while you are
moving them.

Every knob: drag to change, double-click to reset to its default,
<kbd>Shift</kbd>-drag for fine control. Drags are wrapped as single gestures,
so a host records one automation move rather than three hundred writes.

---

## OSC

### Oscillator 1 and 2

**Table** picks the wavetable. **Pos** travels through its frames — this is
the control most worth modulating, and the display above shows you where you
are, drawing the frames receding in Z so that moving Pos reads as travelling
*through* the table rather than swapping one shape for another.

What the display draws is what the oscillator plays, band-limiting included:
the shapes are analysed back out of the generated tables rather than drawn
separately.

**Warp** reshapes each frame before it is played — sync, bend, phase
distortion, remap, mirror and the rest — with **Amt** as its depth. The
display follows the warp, so you can see what it does.

**Semi** and **Fine** tune in semitones and cents. **Level** and **Pan** are
per oscillator.

**Uni** stacks up to sixteen unison voices, **Detune** spreads their tuning
and **Blend** sets how loud the outer voices are against the centre one.
**Spread** is their stereo width.

**→F1, →F2, →OUT** are sends: how much of this oscillator goes to filter 1,
to filter 2, and straight to the output unfiltered. They are independent, so
an oscillator can go to both filters at once, or bypass them entirely. This is
the routing that makes a layered growl possible without two instances.

### Sub

A separate, always-clean oscillator, tunable from three octaves below to one
above (default: one below). **Dir** is direct
level — the sub can skip the filters entirely, which is what you want when the
growl above it is being mangled and the low end must not be.

### Noise

White, pink or filtered noise, with its own level and pan. Useful as the
excitation for a comb filter, or as the top-end grit in a screech.

### Filter 1 and Filter 2

Twelve types — LP and HP at 12 and 24 dB, BP at 12 and 24, notch at 12 and
24, ladder LP and HP, **comb**, and the **formant** filter that most of this
instrument's character comes from.

Every filter here is **zero-delay-feedback** (topology-preserving), and that
is not a specification detail you can ignore. It means the filter stays stable
when its cutoff is modulated at audio rate — and audio-rate filter modulation
is most of what makes a growl. You can put an LFO on cutoff at any speed you
like and it will not blow up or zipper.

- **Cutoff, Res** — as expected. Resonance is stable at extremes.
- **Drive** — saturation inside the filter, with its own **Drive Curve**.
  Drive is *not* a volume control: the stage compensates its own level, so
  turning it up makes the sound dirtier and not simply louder. (Rectify is the
  exception and cannot be level-matched, because it moves most of the signal's
  energy to DC, which the DC blocker then removes.)
- **Mix** — dry/wet for the filter itself.
- **Formant X / Y / Throat** — with the formant type selected, these move the
  vowel. X and Y travel around a vowel space; Throat changes the body. **The
  growl is the formant moving, not the cutoff sweeping** — that is the single
  most useful sentence in this manual.

**Routing** sets how the two filters relate: series, parallel, or split.

---

## MOD

### LFOs (×4)

Each LFO has the usual shapes plus a **drawable custom curve**:

- **Click** empty space to add a point.
- **Drag** a point to move it, or drag *between* two points to bend the
  segment.
- **Double-click** a point to make it a step — square instead of round.
- **Alt-click** (or right-click) a point to delete it.
- Hold **Shift** to ignore the grid.

**Sync** locks the rate to the host tempo, with straight, dotted and triplet
divisions. The triplet divisions are why this synth exists — a 1/8 triplet
growl is the sound.

The playhead on the editor shows where the LFO actually is, pushed from the
engine rather than simulated by the interface, so it cannot drift from what
you hear.

Modulation is applied in **32-sample chunks**, not once per block. A 1/16
wobble at 140 BPM completes in 107 ms; once per 256-sample block would be
about twenty steps per cycle, which is audibly stepped. 32 samples is about
160 steps per cycle of the same wobble.

### Envelopes (×4)

DAHDSR — delay, attack, hold, decay, sustain, release — with a **curve**
control per stage and a velocity amount. Envelope 1 is hard-wired to
amplitude; the rest are free.

### The modulation matrix

Sixteen slots. Each has a **source**, a **destination**, a **depth**, a
**curve** and an enable.

The destination is chosen by name and stored as a parameter **ID**, not as a
position in a list — so adding parameters in a future version cannot silently
repoint your saved patches at the wrong thing.

Depth, curve and enable are host parameters, so you can automate them. The
destination is not, for the reason above.

### Macros (×4)

Four knobs you can map to anything through the matrix, and the ones to put on
a controller.

---

## FX

Fourteen effects, **reorderable by dragging** the chain on the left. Order
matters and the rack respects it: two distortions with an EQ *between* them
sound different from the same two with an EQ after, because the EQ changes
what the second curve is given.

| | |
|---|---|
| **OTT** | Three-band up/down compressor. The signature "everything is loud" sound. It sits between the voice mix and the master fader, so riding the master does not change how hard it compresses. The three **GR** meters show each band, which routinely move in opposite directions — that is the point of it. |
| **Distortion** ×2 | Seven curves, pre-distortion tone tilt, bias. Level-matched by RMS, so turning up Drive changes the character and not the volume. |
| **EQ** ×2 | Six bands each. A flat EQ is bit-transparent — it does not touch the signal at all. |
| **Filter** ×2 | Six types, with the band-pass normalised so switching to it does not drop 6 dB. |
| **Delay** | Synced or free, with filtering in the feedback path. |
| **Chorus / Flanger / Phaser** | The phaser's stage count controls the **number** of notches (N/2 of them), not their depth. |
| **Hyper** | Multi-voice detune spreader. Its level holds within 4 dB from two to eight voices. |
| **Dimension** | Mid/side widener. Turned fully down it is **exactly** untouched, not almost. The mono sum is exactly the dry mid, so it is mono-safe by construction. |
| **Reverb** | Feedback delay network. |
| **Limiter** | Last in the chain regardless of the order you set. |

The whole rack runs at **2× oversampling** whenever anything nonlinear in it
is switched on, and at the base rate when nothing is — you do not pay for it
unless you are using it.

---

## Presets

Open the browser from the preset name in the header. Search, filter by
category, and click to load.

**Save** writes to `~/Documents/GNARL/Presets` (`Documents\GNARL\Presets` on
Windows) as a `.gnarl` file. Subfolders are listed, so you can organise a bank
however you like. The format is documented in
[`preset-format.md`](preset-format.md).

**Morph** blends between two presets on a single control. Continuous
parameters interpolate; discrete ones (filter type, LFO shape) step over at
the halfway point, because there is no meaningful value between "low-pass" and
"comb".

**Randomize** is not uniform noise across 430 parameters — that produces
silence or mud every time. Each parameter carries a policy: some are frozen,
some are gated by a probability, some are confined to a musically useful
sub-range, and only a few are free. You get something playable and usually
something surprising.

A preset load that changes wavetables generates them on a **background
thread**, so the window does not freeze. The browser says the patch is still
arriving rather than pretending it is complete.

### The factory bank

| Preset | Category | What it is |
|---|---|---|
| **Triplet Growl** | Growl | Formant filter under a drawn LFO at 1/8 triplet. The growl is the formant moving, not the filter sweeping. |
| **Sixteenth Wobble** | Bass | A 24 dB low-pass wobbling at 1/16 with the formant held still. Sits under a drop rather than being it. |
| **Reese Foundation** | Bass | Two oscillators detuned against each other through one filter. No effects: a starting point, not a finished sound. |
| **Screech Lead** | Lead | Top of the table through a band-pass, widened by the hyper and answered by a synced delay. |
| **Sub Drop** | Bass | The sub oscillator alone with a long release and the top end cut. Meant to sit under a mix, not in front of it. |
| **Metal Pluck** | Pluck | Comb filter tracking the note, with a short envelope. The ring is the comb's own resonance. |
| **Dream Pad** | Pad | Slow graintable pad through the mono-safe widener and a long reverb. The only patch in the bank that is not aggressive. |
| **Bitcrushed Stab** | FX | Bitcrush and fold in series. These two curves alias on purpose. |
| **Stacked Drive** | Growl | Two distortions with an EQ between them. The EQ changes what the second curve is given, which an EQ afterwards cannot reproduce. |
| **Init** | Keys | The blank slate. |

---

## Settings

The gear icon in the header.

**These are yours, not the patch's.** Theme, animations, hover glow, help text
and knob travel live in the interface, not in the preset — so loading someone
else's patch cannot turn your animations back on, and sharing yours cannot
push your accessibility preference onto them.

| | |
|---|---|
| **Theme** | Acid, Ember, Dream |
| **Animations** | Off honours `prefers-reduced-motion` by default. An animation in flight when you switch lands on its target rather than snapping back. |
| **Hover glow** | The pointer halo only. The glow that marks what is **enabled** never switches off — a UI that stops telling you which of fourteen effects are on is broken, not calmer. |
| **Help text** | The descriptions in the status bar. |
| **Knob travel** | How far you drag for full range. |

---

## Making the sounds

### A triplet growl

1. Load **Triplet Growl**, or build it: one oscillator, a bright table, Pos
   around 35%.
2. Filter 1 to **Formant**, mix 100%, resonance around 45%.
3. LFO 1: **custom** shape, **sync on**, **1/8 triplet**.
4. Matrix slot 1: LFO 1 → **filter1_formant_x**, depth around 80%.
5. Draw the curve. This is the part that matters — the rhythm of the growl is
   the shape you draw, not the LFO's rate. Steps give you the classic
   stuttered articulation; smooth bends give you a vowel sliding.
6. Sub on, an octave down, **Dir** up so it bypasses the filter.
7. Distortion 1 with some drive, then the limiter.

**Modulate the formant, not the cutoff.** A cutoff sweep is a wobble; a
formant move is a voice. Doing both at once usually sounds like neither.

### A wobble that sits under a drop

Same structure, but the formant held still and the **cutoff** modulated at a
straight 1/16 with a 24 dB low-pass. Slower, wider, and it leaves room above
it — which is the difference between a bass line and a lead.

### A reese

Two oscillators, same table, detuned against each other by 10–25 cents, both
sent to one filter. Keep the unison count low: the beating between the two
oscillators is the sound, and sixteen voices of unison smears it into a pad.

### Stacking distortion

Two distortion instances with an **EQ between them** does something a single
distortion cannot, because the second curve receives a different spectrum. Cut
before the second stage to keep the low end from turning to mush; boost before
it to bring a specific band into the dirt.

---

## CPU

- **Oversampling** (header, top right) sets the voice section's rate. 2× is
  the default and the right answer almost always. Measured, 4× is not
  meaningfully cleaner than 2× here — past 2× the folded partials are already
  below the oscillator's own interpolation floor — so 4× costs you CPU for a
  difference you are unlikely to hear.
- **Voices** caps polyphony.
- The FX rack oversamples itself only when something nonlinear in it is on.
- Wavetables are generated **lazily** and cached, so a fresh instance is cheap
  and the first load of an unused table costs about 50 ms on the interface
  thread, not the audio thread.
- An idle interface costs nothing: the plugin stops sending frames when
  nothing is moving.

---

## Licensing

- **The licence check never silences the plugin.** If verification fails,
  audio keeps playing. Preset *saving* and the AI features disable and a
  banner appears; everything you can hear continues to work.
- The **offline grace period is 30 days** and it is not negotiable. A producer
  in a studio with no wifi must not be locked out mid-take.
- Verification runs on a background thread with a timeout. It never touches
  the audio thread and never blocks the interface.
- A server that cannot be *reached* gets the grace period. A server that
  answers and says no does not — but audio still plays, because audio always
  plays.

---

## Troubleshooting

**The plugin window is blank.**
On Windows, this is almost always the missing WebView2 package — see
[`docs/windows-setup.md`](windows-setup.md).

**The DAW does not see the plugin.**
Rescan. Some hosts cache a blocklist: check whether GNARL is in it after a
crash during a previous scan.

**A preset sounds different after reopening the project.**
It should be impossible — a preset and a session go through the same load
path, deliberately. Please report it with the project and the preset.

**A patch loads dull and then brightens a second later.**
That is a wavetable still being generated on the background thread. It is
working as intended; the browser shows the patch as still arriving.

**Knob values read 0% everywhere in a browser preview.**
That is the development preview with a stale parameter dump, not the plugin.

**Audio keeps playing but I cannot save presets.**
The licence check could not confirm. See [Licensing](#licensing) — check the
banner for how many of the 30 grace days are left.
