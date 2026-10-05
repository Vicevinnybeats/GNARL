# Phase 4-06: DRUMS and RIDDIMIZE

## What the producer asked

"A tab in GNARL where you can do what RiddimSmith does" - Avant's plugin
that turns a sound into a riddim one-shot - "and a tab where you can generate
drums like kick, snare, hat and open hat like a real riddim track, exported
as a WAV to use in the track" (FL Studio's playlist, F5). And then: "the drum
loops should be like the tracks I uploaded before - listen to all of them."

Two buttons beside the AI button, each a sheet in the AI sheet's frame:
**DRUMS** and **RIDDIMIZE** (`ui/src/drums/`, `ui/src/riddimize/`). Desktop
header and phone (a row under the preset name).

Nothing in either is anyone else's sample: every drum is synthesized, and
RIDDIMIZE only processes the sound it is given.

## Getting a WAV out (`ui/src/audio/`)

`wav.ts` writes 24-bit PCM. `export.ts`: in a browser, a download; in the
plugin, where a web view's download goes nowhere useful, the page sends the
bytes (`gnarlExportWav`, base64) and `web_panel.cpp` writes them to
**Documents/GNARL/Exports** (`LoadSave::getDataDirectory()/Exports`; Music/
GNARL/Exports on a Mac), never overwriting ("Drums (2).wav"), and answers
with the path; OPEN FOLDER (`gnarlRevealExports`) opens it. The producer
drags the file from there into the playlist. Only a RIFF/WAVE payload is
written. Exports are rendered audio, not presets: the licence does not gate
them. `preview.ts` plays a loop through the computer's own output - in the
plugin not through the DAW, which a web view cannot reach; the exported file
is what goes into the track.

## DRUMS

### Measured from the producer's tracks

`tools/measure_drums.py` measures every drum of a track (numbers only; the
audio is decoded to a temporary file and deleted): the tempo, the drops, and
on a 48-tick bar (16ths every 3 ticks, triplet 8ths every 4) the share of
bars with a kick, snare or hat on each tick, and each drum's length,
pitch and brightness. Run on the fifteen tracks the producer uploaded
(phompy x6, KHOLD x4, WARLORD x2, WHAMMY, WOOCKEZ, ZYKO X NADII), 28 drops:
`references/drums.json`. `tools/drum_templates.py` turns the 21 drops it
could line up into `ui/src/drums/templates.json`.

What they share: a kick on beat 1 always; the **snare on beat 3** (half
time); extra kicks on beat 2, the 16th after beat 3 and around beat 4 in
phompy and WARLORD; hats sparse. In phompy, WARLORD's Snazz, KHOLD's 2
PHONES and thirty6ix the beat-3 snare also raises the sub as a kick does
(4 dB or more with the bass playing): it is layered with a kick, and those
templates keep the kick there. In WARLORD's Mr Snazzy and SPACE EXPLORERS
it is a plain snare.

### The instrument, and how it was made trustworthy

The first version was wrong in ways only a known answer showed. Each was
found by rendering GNARL's own loops, whose hits are known, and measuring
them (`tests/test_drums_riddimize.py`):

| Fault | Fix |
|---|---|
| A 55 Hz bass, per 128-sample hop, rose and fell every few hops: a stream of false kicks | each band's energy averaged over a window holding a cycle (kick 12 ms) |
| HPSS (2048-point frames) smeared every hit by 46 ms, more than a tick | timing from the mix; HPSS only for the sound numbers |
| A kick's pitch starts at 150-300 Hz: timed on the sub, every kick read one or two ticks late | timed on 40-250 Hz, CALLED a kick only if 30-120 Hz then rises 4 dB (with a held bass under them, kicks rose 4.9-10.9 dB, snares at most 2.6) |
| A hat's noise reaches the snare's band, a kick's click too | a snare needs its 150-300 Hz body to rise, and one within 25 ms of a kick is the kick |
| A snare's crack and tail read as hats | a hat must be 6 dB brighter above 7 kHz than at 1.5-5 kHz, or lift the top 6 dB more than the middle (under a growl) |
| Lining the bar up on "most kicks on tick 0" put another kick there when a bar had several | kick on 1 AND snare on 3, over half a bar either side, the middle of the best plateau (the first put the grid 18 ms early) |
| Kicks were dropped twice (picked, then picked again against each other) | already-picked onsets are used as they are |

On twelve GNARL loops (fills off) it now calls **335 of 369 judged steps
right (90.8%)**. What it still gets wrong: a kick a 16th after another (the
first still fills the sub), a hat beside a louder hit. Not judged at all: a
hat under a kick or snare or in a snare's tail, a snare on a kick. So the
templates' kick and snare rows are good; their hat rows are thin, and the
generator's hats are its own as much as the tracks'.

### The generator (`drums.ts`)

GENERATE picks a template. A step it hits in 60% of bars or more is in
every bar; 30-60% steps come and go; beat 1's kick and beat 3's snare are
always there; an open hat takes an offbeat 8th the closed hat leaves; the
fourth bar may roll the snare or hats into the next loop. The sounds:

- **kick**: a sine falling from the template's measured start pitch
  (90-220 Hz) to ~48 Hz, a 4 ms click, driven. Its body length is set (140-
  320 ms): the measured fall is the punch's, HPSS strips the tonal tail.
- **snare**: band-passed noise at the measured brightness (1.5-5 kHz) over a
  200 Hz body, the measured length (90-220 ms), driven, then high-passed at
  120 Hz - driving noise made rumble that read as a kick.
- **hats**: six square partials and noise, high-passed. The measured
  "centroid" is the whole mix's at the hat's moment, so it reads low; mapped
  3-8 kHz -> 7-11 kHz, keeping the tracks' order of brightness.
- **chokes**: a closed hat cuts an open one; a kick cuts the kick before it.
  Without the kick's, a kick a 16th after another started inside a still-full
  tail and rose by under 6 dB.

Four bars, the loop's tails wrapped to its start so it repeats without a
cut, peaking at -1 dBFS, at 140, 145 or 150 bpm. The grid shows one bar at a
time (BAR 1-4); a tap toggles a step. KICK TUNE, KICK LENGTH, SNARE LENGTH,
HAT LENGTH scale what GENERATE chose.

### Measured

`tests/test_drums_riddimize.py`: each row rendered alone has every hit
within 3 ms of its step and no other (with every hit 5 ms late, all five
seeds fail); a loop is exactly four bars (302,400 samples at 140) at -1.00
dBFS; the instrument's 90.8% above.

### Rebuilt like DrumSmith (2026-10-05)

The producer, on the first version: "the pattern is always repeating even
when I move a block, and the patterns are totally wrong - riddim is made
with a kick, a snare, and hats like 4 step or hip hop trap", "make colours
where you can see where the bar ends, like FL Studio", and "randomly
generated, like RiddimSmith". Three faults, each measured or seen:

- **An edit changed one bar in four.** Each bar was its own row of steps;
  the grid showed one (BAR 1-4) and a tap changed only it. Now the pattern
  is ONE bar, repeated, as a step sequencer's is (`setStep` writes every
  bar). `bridge.test.mjs` decodes the exported WAV and checks its four bars
  are the same sample for sample (0 of 8 million steps apart; with the old
  one-bar edit put back, 9,004,139).
- **Every edit restarted the loop** from its first beat; now the new sound
  takes over where the old one was playing (`playPreview(..., keepPlace)`).
- **The patterns.** Each bar drew its own steps from the templates' odds:
  kicks, snares and hats wandered from bar to bar. Now each row has a library
  of patterns (`PATTERNS`), riddim's skeleton in every one - kick on 1, snare
  on 3; extra kicks where the producer's tracks put theirs; hats 4 STEP,
  OFFBEAT, 1/8, 1/16, SPARSE and four TRAP patterns with 32nd and triplet
  rolls (a HAT step tapped again becomes a roll of 2, then 3).

Worked like Avant's DrumSmith (a library per row, a sound and a pattern per
row, re-rolled or locked on its own, stems out): each row shows its pattern
(`< 03 BOUNCE >`), RND re-rolls that row's pattern and sound (from one of
the fifteen tracks' measured numbers), LOCK keeps it through GENERATE, and
EXPORT STEMS writes each row as its own WAV. The grid is coloured as FL's
step sequencer: beats 1 and 3 blue, 2 and 4 red, a gap between beats, the
beat numbers above; while it plays, the sounding step is outlined and the
sounding bar (BAR 1-4) lit.

A 32nd roll falls half a sample off the grid; rounded as one time, it moved
a sample from bar to bar, which the four-bars test caught. Each hit is now
its bar's start plus its place in the bar, rounded apart (`hitPlaces`).

The measuring tool (`measure_drums.py`) no longer shapes the patterns, only
the sounds. On loops like the producer's tracks (snare on 3, hats 4 STEP,
OFFBEAT, 1/8 or SPARSE) it calls 444 of 488 steps right (91.0%, the test's
gate); on the whole library 402 of 468 (85.9%, printed): after a double kick
it can line the bar up one beat early.

## RIDDIMIZE (`riddimize.ts`)

A sound in - a file (any the browser decodes, dropped or chosen) or **USE MY
SOUND**, GNARL's loaded patch rendered by the page's engine at D#3 FL for
3.5 s (`renderPatch`, the MATCH workers) - and the chain:

1. from the first sound 26 dB under the peak, pitched (PITCH, semitones),
   looped to the LENGTH (1/2 bar, 1 bar, 2 bars) with a 10 ms crossfade;
2. a gate in the RHYTHM (1/8, 1/8T, 1/16, 1/4, STUTTER), GATE its share of
   each hit, CHOP from a smooth wub (raised cosine) to a hard chop (2 ms);
3. a FILTER that opens on every hit: WAH (a resonant low pass sweeping up to
   ~7 kHz), VOWEL (two band passes moving from O to A), or NONE; SWEEP how far;
4. DRIVE (tanh), FOLD (sine), CRUSH (to 4 bits);
5. a three-band squash towards a target level (OTT-like, the high band
   hardest), OTT its depth and mix - then the gate, so it cannot lift gaps;
6. SUB: a sine at the sound's note (autocorrelation), an octave down above
   80 Hz, gated with it;
7. peak -1 dBFS, 5 ms fade at the end.

GENERATE draws settings inside riddim's ranges, weighted to 1/8T and 1/8.

Measured with `tools/measure.py` on a plain saw at D#3: 1/8 moves 2.00 per
beat, 1/8T 3.00, 1/16 4.00, 1/4 1.00, at depth 0.83-0.86 (Vinny Bass 2's
growl moves 0.72); peaks -1.00 dBFS; SUB takes the share below 100 Hz of a
D#4 saw from 2.1% to 47.0%.

Nobody here has heard either. The numbers say the loops are where the
templates put them and the one-shots move in their rhythm; whether they
sound like the tracks is the producer's call. RiddimSmith itself was not
available to compare with: its maker's site is blocked from here, and what
is known of it is that it is a processor on a mixer track.
