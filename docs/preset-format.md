# The `.gnarl` preset format

Reference for the on-disk format, for anyone writing a bank by hand, building
a converter, or debugging a patch that will not recall.

The authority is [`plugin/source/preset/PresetFormat.h`](../plugin/source/preset/PresetFormat.h);
this file explains the reasoning that the header only states.

---

## Shape

A `.gnarl` file is XML. One `GNARL_PRESET` element, a `META` child first, then
the plugin's own state tree.

```xml
<GNARL_PRESET formatVersion="1" stateVersion="1">
  <META name="Triplet Growl"
        author="GNARL"
        category="Growl"
        description="Formant-swept growl on a triplet grid."
        tags="growl,triplet,formant"
        created="2026-04-11T20:14:03.412Z"
        pluginVersion="0.1.0"/>
  <GNARL>
    <PARAM id="osc1_table_pos" value="0.0"/>
    <PARAM id="filter1_cutoff" value="820.0"/>
    <!-- ... every parameter the plugin declares ... -->
    <MODSTATE>
      <LFOCURVE index="0">
        <POINT time="0.0" value="1.0" tension="0.0" step="0"/>
        <!-- ... -->
      </LFOCURVE>
      <MODSLOT index="0" destination="filter1_cutoff"/>
      <!-- ... -->
    </MODSTATE>
  </GNARL>
</GNARL_PRESET>
```

## A preset is the plugin's state plus metadata

Not a separate format. The same `ValueTree` that `getStateInformation` writes
into a host session is what the file holds, wrapped in an element that adds a
name and a category.

That is deliberate and it is the single most important thing about the format.
**Two serialisation paths drift**, and the one that drifts is the one a
customer notices: a patch that recalls correctly from the session and wrongly
from the browser, or the reverse, reported months later as "sometimes my
project opens wrong". One path means a preset and a session cannot disagree —
`GnarlProcessor::applyPresetState` is shared by `setStateInformation` and the
browser for the same reason.

## The metadata is a sibling of the state, not inside it

The browser lists a bank by reading every file's metadata, and a bank is
hundreds of files. A format that required parsing 430 parameters to learn a
preset's name would make opening the browser cost the whole bank. `META`
first, at the top, lets the lister stop reading after a few hundred bytes.

## Two version numbers, and they are not the same thing

| Attribute | What it versions | When it changes |
|---|---|---|
| `formatVersion` | This **file's shape** — the element names, where the metadata sits | When the container changes |
| `stateVersion` | The **meaning of the parameters** inside (`pid::kStateVersion`) | When an existing parameter's meaning changes |

Migration keys off `stateVersion`, in `setStateInformation`.

There is a third number that is **not** here and must never be confused with
either: `pid::kParameterVersionHint`. JUCE folds it into the AU parameter ID,
so bumping it invalidates every AU automation lane a customer has drawn. It is
a separate constant precisely so that a preset migration cannot take AU
automation down with it.

## Unknown and newer files load anyway

A preset written by a newer build loads on a best-effort basis rather than
being refused. APVTS ignores IDs it does not know, and parameters absent from
the tree keep their defaults, so a patch from a version with more parameters
comes back as close to right as it can be.

Refusing would be safer for us and worse for the person who just bought the
thing. A hand-written file containing nothing but a name and three parameters
is valid and will load.

## What is not a parameter

Two pieces of patch state are **not** host parameters and live in the
ValueTree rather than as `PARAM` elements:

- **The drawable LFO curves.** A curve is a list of points, not a number.
- **Each mod slot's destination.** A host parameter is a number, so a
  destination would have to be an index into an ordered list of targets — and
  that index shifts the moment the list changes, silently repointing every
  saved preset's modulation at the wrong parameter. Destinations are parameter
  **ID strings**.

A mod slot's depth, curve and enable *are* parameters, because those are worth
automating.

## Frozen things

These cannot change without breaking every preset already saved:

- **Parameter IDs.** Renaming one breaks every saved preset and every host
  automation lane pointing at it. To retire a parameter, leave the ID declared
  and stop reading it.
- **Choice-list order** (`ParameterChoices.h`). A preset stores the chosen
  *index*, not the name, so inserting an entry in the middle changes the
  meaning of every existing preset that used a later entry. **Append only.**
- **Parameter creation order** (`ParameterLayout.cpp`) — it sets the index a
  host shows in its automation list.

Display names are Title Case and may change freely. IDs may not.

## Categories

`Bass`, `Growl`, `Lead`, `Pluck`, `Pad`, `Keys`, `Drums`, `FX`, `Sequence`,
`Texture`.

Free text is accepted on load: a bank from elsewhere must not fail to list
because of a category we have not heard of.

## Where files live

| | |
|---|---|
| User presets | `~/Documents/GNARL/Presets` (macOS and Linux), `Documents\GNARL\Presets` (Windows) |
| Extension | `.gnarl` |
| Factory bank | Built into the binary, not on disk |

Subfolders are listed. Filenames are sanitised on save — characters a
filesystem rejects become `_`, and the preset's *name* in `META` is what the
browser shows, so a file can be renamed on disk without the patch changing its
name.

## Writing one by hand

The smallest valid file:

```xml
<GNARL_PRESET formatVersion="1" stateVersion="1">
  <META name="Hand Written"/>
  <GNARL>
    <PARAM id="filter1_cutoff" value="400.0"/>
  </GNARL>
</GNARL_PRESET>
```

Everything not named keeps its default.

`value` is the **real** value in the parameter's own units — seconds, Hz, dB,
semitones — **not** a 0..1 normalised one. This catches people out: an
envelope attack of `0.002` is two milliseconds, not 0.2% of the range. JUCE
writes it this way and so does this format.

`PARAM` elements come out alphabetically by ID, because that is the order
`APVTS::copyState` produces. Nothing reads that order — the ID is what binds a
value to a parameter — so a hand-written file may list them in any order.

To see every ID and its range, dump the live parameter tree:

```bash
cmake -B build -DGNARL_BUILD_DEMO_RENDERER=ON
cmake --build build --target GnarlDumpDefaults
./build/tests/GnarlDumpDefaults /tmp/parameters.json
```
