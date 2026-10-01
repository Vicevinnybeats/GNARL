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
