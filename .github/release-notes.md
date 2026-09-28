**One download. One folder. Everything in it.**

Grab the zip for your platform, extract it, and you have a single `GNARL`
folder holding the plugin, the standalone, all 150 presets and the bank
audition. Nothing installs itself and nothing writes outside that folder.

```
GNARL/
  GNARL.exe        the standalone — double-click, no DAW needed
  GNARL.vst3       the plugin, ONE FILE — copy this into your VST3 folder
  Presets/         the 150 factory presets as .gnarl files
  Bank audition/   all 150 rendered to one .wav, with an index
  INSTALL.txt      the FL Studio / Ableton / Reaper paths
```

**On Windows the VST3 is a single file, not a folder.** A VST3 is specified
as a bundle — a folder with the binary buried at `Contents/x86_64-win` — and
the spec also allows that binary on its own, which every current host loads.
"Drag this one file into your VST3 folder" is an instruction that cannot be
got half right; copying a folder is, and people end up copying the inner file
or the `Contents` directory and the DAW then finds nothing. The bundle form
is still in a subfolder for any host that insists. macOS keeps the bundle,
where it is mandatory.

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
