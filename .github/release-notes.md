**One download. One folder. Everything in it.**

Grab the zip for your platform, extract it, and you have a single `GNARL`
folder holding the plugin, the standalone, all 150 presets and the bank
audition. Nothing installs itself and nothing writes outside that folder.

```
GNARL/
  GNARL.exe        the standalone — double-click, no DAW needed
  GNARL.vst3/      the plugin — a FOLDER. Copy the whole thing.
  Presets/         the 150 factory presets as .gnarl files
  Bank audition/   all 150 rendered to one .wav, with an index
  INSTALL.txt      the FL Studio / Ableton / Reaper paths
```

**The VST3 is a folder — copy the whole folder.** A VST3 is specified as a
"bundle": a folder named `GNARL.vst3` with the binary inside it. That is the
form every host is required to scan, so it is what sits at the top. Copying
only the inner file is the most common way to end up with a plugin your DAW
cannot find.

v0.1.2 shipped the bare DLL at the top instead, on the claim that every
current host loads it. That was asserted rather than checked and it cost a
failed scan in FL Studio, so the order is reversed. The single file is still
there, in a subfolder, for any host that prefers it — the same binary, byte
for byte. Use one or the other, never both.

**Windows — put it in the right folder. This is the usual cause of "my DAW
cannot see it":**

```
C:\Program Files\Common Files\VST3\      ← VST3 goes HERE
C:\Program Files\VSTPlugins\             ← this is VST2. Not here.
```

`Common Files` is in the middle of that path and it is the part everybody
misses. A `.vst3` dropped in a VST2 folder is never found, and rescanning
will not help: a DAW looks for `.dll` in the VST2 paths and `.vst3` in the
VST3 path. That folder needs administrator rights, which is another way it
fails quietly.

**FL Studio still not finding it?** Options → Manage plugins → tick **"Rescan
previously verified plugins"** → *Find more plugins*. FL caches scan results
per folder and skips folders it has already seen. It then appears under
*Generators → New* — it is a synth, not an effect. `INSTALL.txt` has the full
checklist.

**Windows: right-click the .zip → Properties → tick Unblock → Apply, before
you extract.** This build is unsigned, and Windows marks downloaded archives
so the mark is inherited by everything inside. With it in place Defender can
quarantine `GNARL.exe` during extraction, usually with no dialog — the file
is just missing afterwards and the zip looks like it shipped without it. It
did not; `GNARL.exe` is in there and is about 9 MB. If it has already
vanished, Windows Security → Protection history lists it and can restore it.

macOS is the same problem in a different coat: unsigned, so the first launch
needs Finder → right-click → Open.

**The 150 presets are built into the plugin** — the `Presets` folder is a
copy to back up or share, not something you install. Open the browser and
they are already there.

The separate `-bank-audition.zip` is the same audio as the folder's copy, on
its own, for anyone who wants to hear the bank without downloading the
plugin.

**If the plugin window is blank** the machine is missing the WebView2
runtime. It ships with Windows 11 and recent 10, and is a free download from
Microsoft otherwise.
