#!/usr/bin/env python3
"""Minimal reader for Xcode's project.pbxproj (old-style ASCII plist).

EXISTS BECAUSE A REGEX DAMAGED ONE. Removing the quoted value
"firebase_auth.framework" left `name = ;` behind - not valid plist - and
xcodebuild refused the whole project with exit 74 and nothing on stdout.
The guard only parsed XML at the time, so it passed, and the file is only
ever opened on a macOS CI runner. This parses the format properly, so
check_fork.py can reject a damaged project in seconds on Linux.
"""
import re

_TOKEN = re.compile(r'''
    (?P<ws>\s+)
  | (?P<comment>/\*.*?\*/|//[^\n]*)
  | (?P<str>"(?:[^"\\]|\\.)*")
  | (?P<word>[A-Za-z0-9_$/.:\-+<>~]+)
  | (?P<punct>[{}()=;,])
''', re.S | re.X)

def tokens(text):
    pos = 0
    while pos < len(text):
        m = _TOKEN.match(text, pos)
        if not m:
            raise ValueError(f'unexpected character {text[pos]!r} at offset {pos}')
        pos = m.end()
        kind = m.lastgroup
        if kind in ('ws', 'comment'):
            continue
        yield kind, m.group(kind), m.start()

def parse(text):
    toks = list(tokens(text))
    i = 0
    def expect(p):
        nonlocal i
        if i >= len(toks) or toks[i][1] != p:
            got = toks[i][1] if i < len(toks) else 'EOF'
            off = toks[i][2] if i < len(toks) else len(text)
            line = text.count('\n', 0, off) + 1
            raise ValueError(f'line {line}: expected {p!r}, got {got!r}')
        i += 1
    def value():
        nonlocal i
        if i >= len(toks):
            raise ValueError('unexpected end of file')
        kind, v, off = toks[i]
        if v == '{':
            i += 1; d = {}
            while toks[i][1] != '}':
                k = value(); expect('='); d[k] = value(); expect(';')
            i += 1; return d
        if v == '(':
            i += 1; a = []
            while toks[i][1] != ')':
                a.append(value())
                if toks[i][1] == ',': i += 1
            i += 1; return a
        if kind in ('str', 'word'):
            i += 1; return v[1:-1] if kind == 'str' else v
        line = text.count('\n', 0, off) + 1
        raise ValueError(f'line {line}: expected a value, got {v!r}')
    root = value()
    if i != len(toks):
        raise ValueError('trailing content after the root dictionary')
    return root
