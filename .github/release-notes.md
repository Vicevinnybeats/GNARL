Windows and macOS. VST3 everywhere, AU on macOS, and a standalone that needs
no DAW.

**Start here:** unzip, then double-click `GNARL.exe` (Windows) or `GNARL.app`
(macOS) to hear it with no DAW and nothing installed. `INSTALL.txt` inside
the zip has the FL Studio and Ableton plugin paths.

**The 150 factory presets are built into the plugin** — nothing to install,
nothing to copy. Two optional zips here are about them:

- **`-bank-audition.zip`** — all 150 rendered as one continuous take with an
  index of timestamps, so you can hear the bank before installing anything.
- **`-presets.zip`** — the same 150 as `.gnarl` files, one folder per
  category. A library to back up, share, or open on a machine running a
  different build. You do not need it to use the presets.

**Windows: right-click the .zip → Properties → tick Unblock → Apply, before
you extract.** This build is unsigned, and Windows marks downloaded archives
so the mark is inherited by everything inside. With it in place Defender can
quarantine `GNARL.exe` during extraction, usually with no dialog — the file
is just missing afterwards and the zip looks like it shipped without it. It
did not; `GNARL.exe` is in there and is about 9 MB. If it has already
vanished, Windows Security → Protection history lists it and can restore it.

macOS is the same problem in a different coat: unsigned, so the first launch
needs Finder → right-click → Open.

**If the plugin window is blank** the machine is missing the WebView2
runtime. It ships with Windows 11 and recent 10, and is a free download from
Microsoft otherwise.
