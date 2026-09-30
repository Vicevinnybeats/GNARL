#!/usr/bin/env python3
"""Builds a test patch from a COMPLETE init preset plus named overrides.

Usage:  make_test_patch.py init.vital out.vital key=value [key=value ...]

The init preset comes from `gnarl-render --headless --save init.vital`, i.e.
the engine's own save path, so a test patch can never be missing a section
the loader expects. Unknown keys are an error rather than silently ignored:
the loader drops a name it does not know and falls back to the default, so
a typo would produce a patch that quietly tests nothing.
"""
import json, sys

def main():
    src, dst, *pairs = sys.argv[1:]
    d = json.load(open(src))
    s = d['settings']
    for p in pairs:
        k, v = p.split('=', 1)
        if k not in s:
            sys.exit(f'unknown parameter: {k}')
        s[k] = float(v)
    d['preset_name'] = dst.rsplit('/', 1)[-1].rsplit('.', 1)[0]
    json.dump(d, open(dst, 'w'))

if __name__ == '__main__':
    main()
