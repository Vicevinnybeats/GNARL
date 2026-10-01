GNARL - wavetable synthesizer for riddim and dubstep
=====================================================

What is in this folder (Apple silicon and Intel)
  GNARL.vst3       the VST3 plugin (Ableton Live, FL Studio, ...)
  GNARL.component  the Audio Unit (Logic, GarageBand, Ableton Live)
  GNARL.app        the same instrument without a DAW (standalone)

Install
  1. Copy GNARL.vst3 into       ~/Library/Audio/Plug-Ins/VST3/
     Copy GNARL.component into  ~/Library/Audio/Plug-Ins/Components/
     (In Finder: Go > Go to Folder..., paste the path.)
  2. This beta is not signed by Apple yet, so macOS blocks it until you
     allow it. In Terminal, run once:
       xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/GNARL.vst3
       xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/GNARL.component
     For the standalone: right-click GNARL.app > Open > Open.
  3. Rescan plugins in your DAW. GNARL appears as an instrument by
     "Gnarl Audio".

Licence
  GNARL is free software under the GNU GPL version 3 (LICENSE.txt): you may
  share and modify it under those terms. Source code and the copyright
  notices of everyone whose work it contains:
  https://github.com/Vicevinnybeats/GNARL
