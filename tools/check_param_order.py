#!/usr/bin/env python3
"""Fails if the plugin's host parameter order changed anywhere but the end.

    check_param_order.py probe_output.txt tests/host_parameters.txt [--update]

A DAW keys automation lanes by parameter index and ID. Adding a parameter at
the END changes nothing a user has drawn; inserting, renaming, reordering or
removing one silently moves or breaks lanes in every project that uses the
plugin. So the committed snapshot must remain an exact PREFIX of the live
list. JUCE's trailing "MIDI CC" block is excluded: it is generated after the
synth's own parameters and is not ours to order.

--update rewrites the snapshot from the live list, for use in the same commit
that deliberately APPENDS parameters.
"""
import re, sys

def synth_params(path):
    rows = []
    for line in open(path):
        m = re.match(r'\s*(\d+)\s+id=(\d+)\s+(.*)$', line.rstrip('\n'))
        if m and not m.group(3).startswith('MIDI CC '):
            rows.append(f'{m.group(1)}\t{m.group(2)}\t{m.group(3)}')
    return rows

def main():
    live_path, snap_path = sys.argv[1], sys.argv[2]
    live = synth_params(live_path)
    if not live:
        sys.exit('no parameters found in probe output - was it run with --params?')
    if '--update' in sys.argv:
        open(snap_path, 'w').write('\n'.join(live) + '\n')
        print(f'snapshot written: {len(live)} parameters')
        return 0
    snap = [l for l in open(snap_path).read().split('\n') if l]
    for i, expected in enumerate(snap):
        if i >= len(live) or live[i] != expected:
            got = live[i] if i < len(live) else '(missing)'
            print(f'FAIL host parameter {i} changed:\n  snapshot: {expected}\n  now:      {got}')
            return 1
    print(f'OK: {len(snap)} snapshot parameters unchanged, {len(live) - len(snap)} appended')
    return 0

if __name__ == '__main__':
    sys.exit(main())
