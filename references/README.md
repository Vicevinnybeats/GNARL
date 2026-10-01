# Reference measurements

Measurements of reference tracks, made with `tools/measure.py`: **JSON
only**. The audio never enters this repository (CLAUDE.md §7).
`tools/check_fork.py` fails CI if any audio file is committed anywhere, and
`.gitignore` here keeps it from being staged by accident.

```bash
# On your own machine, with your own copy of the track:
ffmpeg -i "Track.mp3" /tmp/track.wav                      # if it is not a WAV
python3 tools/measure.py /tmp/track.wav --bpm 140 --from 61 --to 69 \
  --json references/artist-track-drop.json                # the 8 seconds you mean
python3 tools/compare.py references/artist-track-drop.json my_patch.vital --bpm 140 --note F1
```

Name each file after the track and the section, and put the section's
bounds in `--from` / `--to`: a whole track averages the drop with the intro.
Pick the note the bass actually plays for `--note`, or the comparison is of
two different pitches.

## Whole tracks, and the drums

`--scan 8` measures every 8-bar window of a whole track, so the drops find
themselves; `--isolate hpss` (librosa) or `--isolate demucs` measure the
bass without the drums; `--bpm auto` finds the tempo. Any file ffmpeg reads
(docs/design/phase3-02-isolate.md):

```bash
pip install librosa            # and demucs, for --isolate demucs
python3 tools/measure.py "Track.mp3" --bpm auto --isolate hpss --scan 8 \
  --json references/artist-track-scan.json
```

Measured so far: five tracks by phompy (`phompy-*`): a `-scan` of each, a
full-mix `-drop` window, and `-drop-bass` (drums removed) for 6:25:300 and
drac07.
